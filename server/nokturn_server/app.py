"""HTTP API, WebSocket hub and link pages."""

import asyncio
import json
import logging
import secrets
import time
import unicodedata
from collections import defaultdict, deque

from aiohttp import WSMsgType, web

from . import pages
from .db import Database, LimitError
from .rooms import RoomError, Rooms, server_time
from .tracks import TrackError, clean_track

log = logging.getLogger("nokturn")

API = "/api/v1"
MAX_BODY = 512 * 1024
MAX_WS_MESSAGE = 256 * 1024
MAX_TRACKS_PER_REQUEST = 500


# Typed keys on aiohttp 3.9+; plain strings on older versions (Ubuntu 22.04 ships 3.8).
def _app_key(name):
    return web.AppKey(name, object) if hasattr(web, "AppKey") else name


def _request_key(name):
    return web.RequestKey(name, object) if hasattr(web, "RequestKey") else name


CONFIG, DB, ROOMS, HUB, LIMITS, HOUSEKEEPING = (_app_key(n) for n in
                                                 ("config", "db", "rooms", "hub", "limits", "housekeeping"))
USER = _request_key("user")


class Config:
    def __init__(self, db_path, public_url, trust_proxy=True):
        self.db_path = db_path
        self.public_url = public_url.rstrip("/")
        self.trust_proxy = trust_proxy


# ---- helpers ------------------------------------------------------------------


def clean_name(value, limit=40):
    if not isinstance(value, str):
        return ""
    text = "".join(c for c in value if unicodedata.category(c)[0] != "C")
    return " ".join(text.split())[:limit]


def error(status, code, message):
    return web.json_response({"error": code, "message": message}, status=status)


class RateLimiter:
    """At most `count` events per `window` seconds for each key."""

    def __init__(self, count, window):
        self.count = count
        self.window = window
        self.events = defaultdict(deque)

    def allow(self, key):
        now = time.monotonic()
        q = self.events[key]
        while q and now - q[0] > self.window:
            q.popleft()
        if len(q) >= self.count:
            return False
        q.append(now)
        return True

    def prune(self):
        now = time.monotonic()
        for key in [k for k, q in self.events.items() if not q or now - q[-1] > self.window]:
            del self.events[key]


def client_ip(request):
    cfg = request.app[CONFIG]
    if cfg.trust_proxy and request.remote in ("127.0.0.1", "::1"):
        return request.headers.get("X-Real-IP") or request.remote
    return request.remote


async def read_json(request):
    if request.content_length and request.content_length > MAX_BODY:
        raise web.HTTPRequestEntityTooLarge(max_size=MAX_BODY, actual_size=request.content_length)
    try:
        body = await request.json()
    except (json.JSONDecodeError, UnicodeDecodeError):
        raise web.HTTPBadRequest(text='{"error":"bad_request","message":"invalid JSON"}',
                                 content_type="application/json")
    if not isinstance(body, dict):
        raise web.HTTPBadRequest(text='{"error":"bad_request","message":"expected an object"}',
                                 content_type="application/json")
    return body


def bearer(request):
    header = request.headers.get("Authorization", "")
    return header[7:].strip() if header.startswith("Bearer ") else ""


def current_user(request):
    token = bearer(request)
    return request.app[DB].user_by_token(token) if token else None


def require_user(handler):
    async def wrapped(request):
        user = current_user(request)
        if not user:
            return error(401, "unauthorized", "sign in first")
        request[USER] = user
        return await handler(request)
    return wrapped


# Browsers can't send an Authorization header with a WebSocket: the web app first asks for a ticket (with its
# token, like any request) and opens the socket with it — single use, half a minute, so the token itself never
# ends up in a URL.
_tickets = {}


@require_user
async def ws_ticket(request):
    now = time.time()
    for k in [k for k, v in _tickets.items() if v[1] < now]:
        del _tickets[k]
    ticket = secrets.token_urlsafe(24)
    _tickets[ticket] = (request[USER], now + 30)
    return web.json_response({"ticket": ticket})


def ticket_user(request):
    entry = _tickets.pop(request.query.get("ticket", ""), None)
    return entry[0] if entry and entry[1] >= time.time() else None


def clean_tracks(raw, limit=MAX_TRACKS_PER_REQUEST):
    if not isinstance(raw, list):
        return []
    out = []
    for item in raw[:limit]:
        try:
            out.append(clean_track(item))
        except TrackError:
            continue
    return out


# ---- the hub: who is connected --------------------------------------------------


class Connection:
    def __init__(self, ws, user):
        self.ws = ws
        self.user = user
        self.room = None
        self.limiter = RateLimiter(40, 2.0)

    async def send(self, message):
        if self.ws.closed:
            return
        try:
            await self.ws.send_str(json.dumps(message, separators=(",", ":")))
        except (ConnectionResetError, RuntimeError):
            pass


class Hub:
    def __init__(self, db, rooms):
        self.db = db
        self.rooms = rooms
        self.by_user = defaultdict(set)

    def add(self, conn):
        self.by_user[conn.user["id"]].add(conn)

    def remove(self, conn):
        conns = self.by_user.get(conn.user["id"])
        if conns:
            conns.discard(conn)
            if not conns:
                del self.by_user[conn.user["id"]]

    async def to_users(self, user_ids, message):
        sends = [c.send(message) for uid in set(user_ids) for c in list(self.by_user.get(uid, ()))]
        if sends:
            await asyncio.gather(*sends)

    async def playlist_changed(self, playlist_id, extra_users=()):
        p = self.db.playlist_row(playlist_id)
        members = self.db.members(playlist_id) if p else []
        message = {"type": "playlist", "id": playlist_id, "version": p["version"] if p else 0}
        await self.to_users(members, message)
        if extra_users:
            await self.to_users(extra_users, {"type": "playlist.removed", "id": playlist_id})

    def room_connections(self, room):
        return [c for uid in room.members for c in self.by_user.get(uid, ()) if c.room is room]

    async def broadcast_room(self, room):
        message = {"type": "room", "room": room.state(), "serverTime": server_time()}
        sends = [c.send(message) for c in self.room_connections(room)]
        if sends:
            await asyncio.gather(*sends)


# ---- REST: accounts -------------------------------------------------------------


async def health(request):
    return web.json_response({"ok": True, "rooms": len(request.app[ROOMS].rooms), "time": server_time()})


async def register(request):
    if not request.app[LIMITS]["register"].allow(client_ip(request)):
        return error(429, "rate_limited", "too many new accounts from this address, try later")
    body = await read_json(request)
    name = clean_name(body.get("name")) or "Listener"
    user, token = request.app[DB].create_user(name)
    return web.json_response({"user": user, "token": token})


@require_user
async def me(request):
    return web.json_response({"user": request[USER]})


@require_user
async def update_me(request):
    body = await read_json(request)
    name = clean_name(body.get("name"))
    if not name:
        return error(400, "bad_request", "name is required")
    user = request[USER]
    request.app[DB].rename_user(user["id"], name)
    user["name"] = name
    return web.json_response({"user": user})


@require_user
async def delete_me(request):
    user = request[USER]
    db, hub = request.app[DB], request.app[HUB]
    member_of = [p["id"] for p in db.playlists_of(user["id"])]
    owned = db.delete_user(user["id"])
    for pid in member_of:
        if pid in owned:
            continue
        await hub.playlist_changed(pid)
    return web.json_response({"deleted": True})


# ---- REST: the library and paired devices -------------------------------------


@require_user
async def get_library(request):
    return web.json_response(request.app[DB].library(request[USER]["id"]))


async def _library_changed(request, version):
    # the account's other devices fetch it again
    await request.app[HUB].to_users([request[USER]["id"]], {"type": "library", "version": version})
    return web.json_response({"version": version})


@require_user
async def like_track(request):
    if not request.app[LIMITS]["library"].allow(request[USER]["id"]):
        return error(429, "rate_limited", "too many changes, try again later")
    body = await read_json(request)
    # {"track", "at"?} or {"items": [{"track", "at"?}, …]} — "at" (ms) places likes made before
    raw = body.get("items") if isinstance(body.get("items"), list) else [body]
    latest = time.time() * 1000 + 60_000
    items = []
    for item in raw[:MAX_TRACKS_PER_REQUEST]:
        if not isinstance(item, dict):
            continue
        at = item.get("at")
        at = int(at) if isinstance(at, (int, float)) and 0 < at < latest else None
        try:
            items.append((clean_track(item.get("track")), at))
        except TrackError as e:
            if len(raw) == 1:
                return error(400, "bad_track", str(e))
    if not items:
        return error(400, "bad_track", "no track")
    try:
        version = request.app[DB].like_many(request[USER]["id"], items)
    except LimitError as e:
        return error(409, "limit", str(e))
    return await _library_changed(request, version)


@require_user
async def unlike_track(request):
    if not request.app[LIMITS]["library"].allow(request[USER]["id"]):
        return error(429, "rate_limited", "too many changes, try again later")
    version = request.app[DB].set_liked(request[USER]["id"], request.match_info["track"], False)
    return await _library_changed(request, version)


@require_user
async def add_history(request):
    if not request.app[LIMITS]["library"].allow(request[USER]["id"]):
        return error(429, "rate_limited", "too many changes, try again later")
    body = await read_json(request)
    try:
        version = request.app[DB].add_history(request[USER]["id"], clean_track(body.get("track")))
    except TrackError as e:
        return error(400, "bad_track", str(e))
    return await _library_changed(request, version)


@require_user
async def clear_history(request):
    return await _library_changed(request, request.app[DB].clear_history(request[USER]["id"]))


@require_user
async def create_pair_code(request):
    if not request.app[LIMITS]["pair"].allow(request[USER]["id"]):
        return error(429, "rate_limited", "too many codes, try again later")
    code, expires = request.app[DB].create_pair_code(request[USER]["id"])
    return web.json_response({"code": code, "expires": expires})


async def redeem_pair_code(request):
    if not request.app[LIMITS]["pair"].allow(client_ip(request)):
        return error(429, "rate_limited", "too many attempts, try again later")
    body = await read_json(request)
    result = request.app[DB].redeem_pair_code(body.get("code"))
    if not result:
        return error(404, "bad_code", "the code is wrong or has expired")
    user, token = result
    return web.json_response({"user": user, "token": token})


@require_user
async def delete_me(request):
    user = request[USER]
    db, hub = request.app[DB], request.app[HUB]
    member_of = [p["id"] for p in db.playlists_of(user["id"])]
    owned = db.delete_user(user["id"])
    for pid in member_of:
        if pid in owned:
            continue
        await hub.playlist_changed(pid)
    return web.json_response({"deleted": True})


# ---- REST: shared tracks --------------------------------------------------------


@require_user
async def create_share(request):
    user = request[USER]
    if not request.app[LIMITS]["share"].allow(user["id"]):
        return error(429, "rate_limited", "too many links, try again later")
    body = await read_json(request)
    try:
        track = clean_track(body.get("track"))
    except TrackError as e:
        return error(400, "bad_track", str(e))
    code = request.app[DB].create_share(user["id"], track)
    return web.json_response({"code": code, "url": f'{request.app[CONFIG].public_url}/t/{code}'})


async def get_share(request):
    share = request.app[DB].get_share(request.match_info["code"], count_open=True)
    if not share:
        return error(404, "not_found", "no such link")
    return web.json_response(share)


# ---- REST: shared playlists -----------------------------------------------------


def playlist_json(request, p):
    p["url"] = f'{request.app[CONFIG].public_url}/p/{p["id"]}'
    return p


@require_user
async def list_playlists(request):
    playlists = request.app[DB].playlists_of(request[USER]["id"])
    return web.json_response({"playlists": [playlist_json(request, p) for p in playlists]})


@require_user
async def create_playlist(request):
    user = request[USER]
    if not request.app[LIMITS]["playlist"].allow(user["id"]):
        return error(429, "rate_limited", "too many new playlists, try again later")
    body = await read_json(request)
    name = clean_name(body.get("name"), 100)
    if not name:
        return error(400, "bad_request", "name is required")
    description = clean_name(body.get("description"), 500)
    db = request.app[DB]
    try:
        playlist_id = db.create_playlist(user["id"], name, description, clean_tracks(body.get("tracks")),
                                         clean_name(body.get("origin"), 100))
    except LimitError as e:
        return error(403, "limit", str(e))
    return web.json_response(playlist_json(request, db.playlist(playlist_id)))


def _playlist_or_404(request):
    p = request.app[DB].playlist_row(request.match_info["id"])
    if not p:
        raise web.HTTPNotFound(text='{"error":"not_found","message":"no such playlist"}', content_type="application/json")
    return p


@require_user
async def get_playlist(request):
    p = _playlist_or_404(request)
    db = request.app[DB]
    if not db.is_member(p["id"], request[USER]["id"]):
        # Not joined yet: what the invite is about, without the tracks.
        preview = db.playlist(p["id"], with_tracks=False)
        preview["member"] = False
        return web.json_response(playlist_json(request, preview))
    out = db.playlist(p["id"])
    out["member"] = True
    return web.json_response(playlist_json(request, out))


@require_user
async def join_playlist(request):
    p = _playlist_or_404(request)
    db = request.app[DB]
    try:
        joined = db.join_playlist(p["id"], request[USER]["id"])
    except LimitError as e:
        return error(403, "limit", str(e))
    if joined:
        await request.app[HUB].playlist_changed(p["id"])
    out = db.playlist(p["id"])
    out["member"] = True
    return web.json_response(playlist_json(request, out))


@require_user
async def leave_playlist(request):
    p = _playlist_or_404(request)
    user = request[USER]
    db, hub = request.app[DB], request.app[HUB]
    if p["owner_id"] == user["id"]:
        return error(400, "owner", "the owner deletes the playlist instead of leaving it")
    db.leave_playlist(p["id"], user["id"])
    await hub.playlist_changed(p["id"], extra_users=[user["id"]])
    return web.json_response({"left": True})


@require_user
async def replace_playlist_tracks(request):
    p = _playlist_or_404(request)
    if p["owner_id"] != request[USER]["id"]:
        return error(403, "owner_only", "only the owner can replace the tracks")
    body = await read_json(request)
    db = request.app[DB]
    db.replace_tracks(p["id"], request[USER]["id"], clean_tracks(body.get("tracks")))
    await request.app[HUB].playlist_changed(p["id"])
    return web.json_response(playlist_json(request, db.playlist(p["id"])))


@require_user
async def update_playlist(request):
    p = _playlist_or_404(request)
    if p["owner_id"] != request[USER]["id"]:
        return error(403, "owner_only", "only the owner can rename the playlist")
    body = await read_json(request)
    name = clean_name(body["name"], 100) if "name" in body else None
    if name == "":
        return error(400, "bad_request", "name is required")
    description = clean_name(body["description"], 500) if "description" in body else None
    db = request.app[DB]
    db.update_playlist(p["id"], name, description)
    await request.app[HUB].playlist_changed(p["id"])
    return web.json_response(playlist_json(request, db.playlist(p["id"])))


@require_user
async def delete_playlist(request):
    p = _playlist_or_404(request)
    if p["owner_id"] != request[USER]["id"]:
        return error(403, "owner_only", "only the owner can delete the playlist")
    db = request.app[DB]
    members = db.members(p["id"])
    db.delete_playlist(p["id"])
    await request.app[HUB].to_users(members, {"type": "playlist.removed", "id": p["id"]})
    return web.json_response({"deleted": True})


def _require_member(request, p):
    if not request.app[DB].is_member(p["id"], request[USER]["id"]):
        raise web.HTTPForbidden(text='{"error":"not_member","message":"join the playlist first"}',
                                content_type="application/json")


@require_user
async def add_playlist_tracks(request):
    p = _playlist_or_404(request)
    _require_member(request, p)
    body = await read_json(request)
    tracks = clean_tracks(body.get("tracks"))
    if not tracks:
        return error(400, "bad_track", "no tracks that can be shared (YouTube Music or SoundCloud)")
    db = request.app[DB]
    try:
        added = db.add_tracks(p["id"], request[USER]["id"], tracks)
    except LimitError as e:
        return error(403, "limit", str(e))
    if added:
        await request.app[HUB].playlist_changed(p["id"])
    out = playlist_json(request, db.playlist(p["id"]))
    out["added"] = added
    return web.json_response(out)


@require_user
async def remove_playlist_track(request):
    p = _playlist_or_404(request)
    _require_member(request, p)
    db = request.app[DB]
    if db.remove_track(p["id"], request.match_info["entry"]):
        await request.app[HUB].playlist_changed(p["id"])
    return web.json_response(playlist_json(request, db.playlist(p["id"])))


@require_user
async def move_playlist_track(request):
    p = _playlist_or_404(request)
    _require_member(request, p)
    body = await read_json(request)
    to = body.get("to")
    if not isinstance(to, int):
        return error(400, "bad_request", "'to' must be a number")
    db = request.app[DB]
    if db.move_track(p["id"], request.match_info["entry"], to):
        await request.app[HUB].playlist_changed(p["id"])
    return web.json_response(playlist_json(request, db.playlist(p["id"])))


# ---- REST: rooms (previews; everything else goes over the WebSocket) ------------


async def get_room(request):
    room = request.app[ROOMS].get(request.match_info["code"])
    if not room:
        return error(404, "not_found", "no such room")
    out = room.preview()
    out["url"] = f'{request.app[CONFIG].public_url}/r/{room.code}'
    return web.json_response(out)


# ---- WebSocket ----------------------------------------------------------------


async def websocket(request):
    user = current_user(request) or ticket_user(request)
    if not user:
        return error(401, "unauthorized", "sign in first")
    ws = web.WebSocketResponse(heartbeat=25, max_msg_size=MAX_WS_MESSAGE)
    await ws.prepare(request)
    hub = request.app[HUB]
    conn = Connection(ws, user)
    hub.add(conn)
    await conn.send({"type": "hello", "user": user, "serverTime": server_time()})
    try:
        async for msg in ws:
            if msg.type != WSMsgType.TEXT:
                if msg.type == WSMsgType.ERROR:
                    break
                continue
            if not conn.limiter.allow(0):
                await conn.send({"type": "error", "error": "rate_limited", "message": "slow down"})
                continue
            try:
                data = json.loads(msg.data)
            except json.JSONDecodeError:
                continue
            if not isinstance(data, dict):
                continue
            try:
                await handle_message(request.app, conn, data)
            except RoomError as e:
                await conn.send({"type": "error", "error": e.code, "message": str(e), "request": data.get("type")})
            except (TypeError, ValueError, KeyError) as e:
                await conn.send({"type": "error", "error": "bad_request", "message": str(e), "request": data.get("type")})
    finally:
        hub.remove(conn)
        if conn.room:
            room = conn.room
            conn.room = None
            room.disconnect(user["id"])
            await hub.broadcast_room(room)
    return ws


async def handle_message(app, conn, data):
    kind = data.get("type")
    hub, rooms = app[HUB], app[ROOMS]
    user_id = conn.user["id"]

    if kind == "ping":
        await conn.send({"type": "pong", "t": data.get("t"), "serverTime": server_time()})
        return

    if kind == "room.create":
        if not app[LIMITS]["room"].allow(user_id):
            raise RoomError("rate_limited", "too many new rooms, try again later")
        name = clean_name(data.get("name"), 60) or f'{conn.user["name"]}\'s room'
        room = rooms.create(conn.user, name, data.get("mode", "open"))
        await _enter_room(hub, conn, room)
        return

    if kind == "room.join":
        room = rooms.get(data.get("code"))
        if not room:
            raise RoomError("no_room", "this room doesn't exist anymore")
        await _enter_room(hub, conn, room)
        return

    room = conn.room
    if room is None:
        raise RoomError("no_room", "you are not in a room")

    if kind == "room.leave":
        conn.room = None
        room.leave(user_id)
        await conn.send({"type": "room.left", "code": room.code})
        await hub.broadcast_room(room)
        return

    position = data.get("position")
    position = float(position) if isinstance(position, (int, float)) else None

    if kind == "room.add":
        tracks = clean_tracks(data.get("tracks"), 100)
        if not tracks:
            raise RoomError("bad_track", "only YouTube Music and SoundCloud tracks can go into a room")
        room.add(user_id, tracks, play_next=bool(data.get("next")))
    elif kind == "room.remove":
        room.remove(user_id, str(data.get("itemId")))
    elif kind == "room.move":
        room.move(user_id, str(data.get("itemId")), int(data.get("to")))
    elif kind == "room.play":
        room.play_item(user_id, str(data.get("itemId")))
    elif kind == "room.pause":
        room.pause(user_id, position)
    elif kind == "room.resume":
        room.resume(user_id)
    elif kind == "room.seek":
        if position is None:
            raise RoomError("bad_request", "position is required")
        room.seek(user_id, position)
    elif kind == "room.next":
        room.next(user_id)
    elif kind == "room.previous":
        room.previous(user_id)
    elif kind == "room.ended":
        if not room.ended(user_id, str(data.get("itemId"))):
            return
    elif kind == "room.mode":
        room.set_mode(user_id, data.get("mode"))
    elif kind == "room.host":
        room.set_host(user_id, str(data.get("userId")))
    elif kind == "room.rename":
        name = clean_name(data.get("name"), 60)
        if not name:
            raise RoomError("bad_request", "name is required")
        room.rename(user_id, name)
    elif kind == "room.kick":
        target = str(data.get("userId"))
        room.kick(user_id, target)
        for c in list(hub.by_user.get(target, ())):
            if c.room is room:
                c.room = None
                await c.send({"type": "room.left", "code": room.code, "reason": "kicked"})
    else:
        raise RoomError("bad_request", f"unknown message type {kind!r}")
    await hub.broadcast_room(room)


async def _enter_room(hub, conn, room):
    if conn.room is room:
        await conn.send({"type": "room", "room": room.state(), "serverTime": server_time()})
        return
    if conn.room is not None:
        previous = conn.room
        conn.room = None
        # Another connection of the same person may still be in the old room.
        if any(c.room is previous for c in hub.by_user.get(conn.user["id"], ())):
            previous.disconnect(conn.user["id"])
        else:
            previous.leave(conn.user["id"])
        await hub.broadcast_room(previous)
    conn.room = room
    room.join(conn.user)
    await hub.broadcast_room(room)


# ---- link pages -----------------------------------------------------------------


def _html(text, status=200):
    return web.Response(text=text, status=status, content_type="text/html", charset="utf-8",
                        headers={"Cache-Control": "no-cache", "X-Content-Type-Options": "nosniff",
                                 "Referrer-Policy": "no-referrer",
                                 "Content-Security-Policy": "default-src 'none'; img-src https: http: data:; "
                                                            "style-src 'unsafe-inline'"})


async def track_page(request):
    lang = pages.language(request)
    code = request.match_info["code"]
    url = f'{request.app[CONFIG].public_url}/t/{code}'
    share = request.app[DB].get_share(code)
    if not share:
        return _html(pages.missing_page(lang, "track", url), 404)
    return _html(pages.track_page(lang, share, url))


async def room_page(request):
    lang = pages.language(request)
    code = request.match_info["code"].upper()
    url = f'{request.app[CONFIG].public_url}/r/{code}'
    room = request.app[ROOMS].get(code)
    if not room:
        return _html(pages.missing_page(lang, "room", url), 404)
    return _html(pages.room_page(lang, room.preview(), url))


async def playlist_page(request):
    lang = pages.language(request)
    playlist_id = request.match_info["id"]
    url = f'{request.app[CONFIG].public_url}/p/{playlist_id}'
    db = request.app[DB]
    p = db.playlist(playlist_id, with_tracks=False)
    if not p:
        return _html(pages.missing_page(lang, "playlist", url), 404)
    return _html(pages.playlist_page(lang, p, db.preview_covers(playlist_id), url))


async def home(request):
    return _html(pages.home_page(pages.language(request), request.app[CONFIG].public_url))


# ---- the app ------------------------------------------------------------------


async def housekeeping(app):
    hub, rooms = app[HUB], app[ROOMS]
    ticks = 0
    while True:
        await asyncio.sleep(1.0)
        try:
            for room in rooms.sweep():
                await hub.broadcast_room(room)
            ticks += 1
            if ticks % 600 == 0:
                for limiter in app[LIMITS].values():
                    limiter.prune()
        except Exception:   # keep the loop alive whatever happens to one room
            log.exception("housekeeping failed")


async def _start_background(app):
    app[HOUSEKEEPING] = asyncio.create_task(housekeeping(app))


async def _stop_background(app):
    app[HOUSEKEEPING].cancel()
    try:
        await app[HOUSEKEEPING]
    except asyncio.CancelledError:
        pass
    for conns in list(app[HUB].by_user.values()):
        for c in list(conns):
            await c.ws.close(code=1001, message=b"server shutdown")
    app[DB].close()


@web.middleware
async def json_errors(request, handler):
    try:
        return await handler(request)
    except web.HTTPException as e:
        if request.path.startswith(API) and e.content_type != "application/json":
            return error(e.status, "http_error", e.reason)
        raise


def create_app(config):
    app = web.Application(client_max_size=MAX_BODY, middlewares=[json_errors])
    app[CONFIG] = config
    app[DB] = Database(config.db_path)
    app[ROOMS] = Rooms()
    app[HUB] = Hub(app[DB], app[ROOMS])
    app[LIMITS] = {
        "register": RateLimiter(20, 3600),
        "share": RateLimiter(120, 3600),
        "playlist": RateLimiter(30, 3600),
        "room": RateLimiter(30, 3600),
        "library": RateLimiter(3000, 3600),
        "pair": RateLimiter(20, 3600),
    }
    r = app.router
    r.add_get(f"{API}/health", health)
    r.add_post(f"{API}/register", register)
    r.add_get(f"{API}/me", me)
    r.add_patch(f"{API}/me", update_me)
    r.add_delete(f"{API}/me", delete_me)
    r.add_post(f"{API}/shares", create_share)
    r.add_get(f"{API}/shares/{{code}}", get_share)
    r.add_get(f"{API}/playlists", list_playlists)
    r.add_post(f"{API}/playlists", create_playlist)
    r.add_get(f"{API}/playlists/{{id}}", get_playlist)
    r.add_patch(f"{API}/playlists/{{id}}", update_playlist)
    r.add_delete(f"{API}/playlists/{{id}}", delete_playlist)
    r.add_post(f"{API}/playlists/{{id}}/join", join_playlist)
    r.add_post(f"{API}/playlists/{{id}}/leave", leave_playlist)
    r.add_post(f"{API}/playlists/{{id}}/tracks", add_playlist_tracks)
    r.add_put(f"{API}/playlists/{{id}}/tracks", replace_playlist_tracks)
    r.add_delete(f"{API}/playlists/{{id}}/tracks/{{entry}}", remove_playlist_track)
    r.add_post(f"{API}/playlists/{{id}}/tracks/{{entry}}/move", move_playlist_track)
    r.add_get(f"{API}/rooms/{{code}}", get_room)
    r.add_get(f"{API}/ws", websocket)
    r.add_post(f"{API}/ws/ticket", ws_ticket)
    r.add_get(f"{API}/library", get_library)
    r.add_post(f"{API}/library/liked", like_track)
    r.add_delete(f"{API}/library/liked/{{track}}", unlike_track)
    r.add_post(f"{API}/library/history", add_history)
    r.add_delete(f"{API}/library/history", clear_history)
    r.add_post(f"{API}/me/pair", create_pair_code)
    r.add_post(f"{API}/pair", redeem_pair_code)
    r.add_get("/", home)
    r.add_get("/t/{code}", track_page)
    r.add_get("/r/{code}", room_page)
    r.add_get("/p/{id}", playlist_page)
    app.on_startup.append(_start_background)
    app.on_cleanup.append(_stop_background)
    return app
