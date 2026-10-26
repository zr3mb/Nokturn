"""YouTube audio for clients that can't run yt-dlp themselves (the Android app).

The server resolves a video with yt-dlp and hands out a signed URL of its own; requests to it are
forwarded to YouTube with their Range header, so the client fetches the audio in chunks as usual.
The stream goes through the server because YouTube ties its URLs to the address that asked for them.
"""

import asyncio
import hashlib
import hmac
import logging
import os
import re
import secrets
import shutil
import sys
import time
from urllib.parse import parse_qs, urlparse

from aiohttp import ClientError, ClientSession, ClientTimeout, web

log = logging.getLogger("nokturn.stream")

VIDEO_ID = re.compile(r"^[A-Za-z0-9_-]{11}$")
LINK_LIFETIME = 6 * 3600
RESOLVE_TIMEOUT = 60
FORWARDED_HEADERS = ("Content-Type", "Content-Length", "Content-Range", "Accept-Ranges")


def find_ytdlp():
    """yt-dlp from NOKTURN_YTDLP, the server's own venv, or PATH."""
    explicit = os.environ.get("NOKTURN_YTDLP")
    if explicit:
        return explicit
    beside = os.path.join(os.path.dirname(sys.executable), "yt-dlp")
    if os.access(beside, os.X_OK):
        return beside
    return shutil.which("yt-dlp")


def load_secret(db_path):
    """A signing key kept next to the database, so links survive a restart."""
    if db_path == ":memory:":
        return secrets.token_bytes(32)
    path = os.path.join(os.path.dirname(os.path.abspath(db_path)), "stream.key")
    try:
        with open(path, "rb") as f:
            key = f.read()
        if len(key) >= 32:
            return key
    except FileNotFoundError:
        pass
    key = secrets.token_bytes(32)
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, "wb") as f:
        f.write(key)
    return key


class StreamError(Exception):
    pass


class Streams:
    def __init__(self, secret, ytdlp, cookies=None):
        self.secret = secret
        self.ytdlp = ytdlp
        # YouTube asks servers in data centres to "sign in to confirm you're not a bot" now and then;
        # a cookies file of a (spare) account, exported from a browser, gets past it.
        self.cookies = cookies
        self.cache = {}       # video id -> (upstream url, expires, content length)
        self.pending = {}     # video id -> future of a running yt-dlp
        self.slots = asyncio.Semaphore(3)
        self.session = None

    @property
    def available(self):
        return bool(self.ytdlp)

    def sign(self, video_id, expires):
        msg = f"{video_id}:{expires}".encode()
        return hmac.new(self.secret, msg, hashlib.sha256).hexdigest()[:32]

    def verify(self, video_id, expires, signature):
        try:
            expires = int(expires)
        except (TypeError, ValueError):
            return False
        return expires > time.time() and hmac.compare_digest(self.sign(video_id, expires), signature or "")

    async def resolve(self, video_id, fresh=False):
        entry = self.cache.get(video_id)
        if entry and not fresh and entry[1] > time.time() + 120:
            return entry
        if video_id in self.pending:
            return await asyncio.shield(self.pending[video_id])
        future = asyncio.get_running_loop().create_future()
        self.pending[video_id] = future
        try:
            entry = await self._run(video_id)
            self.cache[video_id] = entry
            future.set_result(entry)
            return entry
        except Exception as e:
            future.set_exception(e)
            future.exception()   # marks it retrieved when nobody else waits
            raise
        finally:
            del self.pending[video_id]

    async def _run(self, video_id):
        if not self.ytdlp:
            raise StreamError("yt-dlp is not installed on the server")
        # Live streams are HLS playlists whose segments come straight from YouTube — they can't be relayed.
        args = [self.ytdlp, "--no-warnings", "--no-playlist", "--no-progress", "--match-filter", "!is_live",
                "-f", "bestaudio[ext=m4a]/bestaudio/best", "--print", "urls",
                "https://music.youtube.com/watch?v=" + video_id]
        if self.cookies and os.path.isfile(self.cookies):
            args[1:1] = ["--cookies", self.cookies]
        async with self.slots:
            proc = await asyncio.create_subprocess_exec(*args, stdout=asyncio.subprocess.PIPE,
                                                        stderr=asyncio.subprocess.PIPE)
            try:
                out, err = await asyncio.wait_for(proc.communicate(), RESOLVE_TIMEOUT)
            except asyncio.TimeoutError:
                proc.kill()
                await proc.wait()
                raise StreamError("yt-dlp took too long")
        lines = out.decode(errors="replace").split()
        url = lines[0] if lines else ""
        if proc.returncode == 0 and not url:
            raise StreamError("live streams can't be played through the server")
        if proc.returncode != 0 or not url.startswith("http"):
            message = err.decode(errors="replace").strip().splitlines()
            message = message[-1].replace("ERROR: ", "") if message else "yt-dlp failed"
            raise StreamError(message[:200])
        query = parse_qs(urlparse(url).query)
        try:
            expires = int(query.get("expire", ["0"])[0])
        except ValueError:
            expires = 0
        if expires <= 0:
            expires = int(time.time()) + 3600
        try:
            length = int(query.get("clen", ["0"])[0])
        except ValueError:
            length = 0
        return url, expires, length

    def link(self, public_url, video_id, entry):
        expires = int(min(entry[1], time.time() + LINK_LIFETIME))
        url = (f"{public_url}/api/v1/stream/{video_id}/audio?e={expires}"
               f"&s={self.sign(video_id, expires)}")
        if entry[2] > 0:
            url += f"&clen={entry[2]}"
        return url, expires

    async def forward(self, request, video_id):
        if self.session is None:
            self.session = ClientSession(timeout=ClientTimeout(total=None, sock_connect=10, sock_read=30))
        headers = {"User-Agent": "Mozilla/5.0"}
        if "Range" in request.headers:
            headers["Range"] = request.headers["Range"]
        for attempt in range(2):
            url = (await self.resolve(video_id, fresh=attempt > 0))[0]
            try:
                upstream = await self.session.get(url, headers=headers)
            except ClientError as e:
                raise StreamError(f"YouTube didn't answer: {e}")
            if upstream.status in (403, 404, 410) and attempt == 0:
                upstream.release()
                continue   # the URL expired early: resolve the video again
            break
        try:
            response = web.StreamResponse(status=upstream.status)
            for name in FORWARDED_HEADERS:
                if name in upstream.headers:
                    response.headers[name] = upstream.headers[name]
            await response.prepare(request)
            async for chunk in upstream.content.iter_chunked(64 * 1024):
                await response.write(chunk)
            await response.write_eof()
            return response
        finally:
            upstream.release()

    async def close(self):
        if self.session is not None:
            await self.session.close()
