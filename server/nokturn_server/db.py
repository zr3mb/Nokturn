"""SQLite storage: accounts (and the devices signed in to them), the library (liked tracks, history), shared
tracks and playlists. Listening rooms live in memory."""

import hashlib
import json
import secrets
import sqlite3
import time

SCHEMA = """
CREATE TABLE IF NOT EXISTS users (
    id          TEXT PRIMARY KEY,
    token_hash  TEXT NOT NULL UNIQUE,
    name        TEXT NOT NULL,
    created     INTEGER NOT NULL,
    seen        INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS shares (
    code        TEXT PRIMARY KEY,
    track       TEXT NOT NULL,
    user_id     TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    created     INTEGER NOT NULL,
    opens       INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS playlists (
    id          TEXT PRIMARY KEY,
    owner_id    TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    name        TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    created     INTEGER NOT NULL,
    updated     INTEGER NOT NULL,
    version     INTEGER NOT NULL DEFAULT 1,
    origin      TEXT NOT NULL DEFAULT ''
);
CREATE TABLE IF NOT EXISTS playlist_members (
    playlist_id TEXT NOT NULL REFERENCES playlists(id) ON DELETE CASCADE,
    user_id     TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    joined      INTEGER NOT NULL,
    PRIMARY KEY (playlist_id, user_id)
);
CREATE INDEX IF NOT EXISTS playlist_members_user ON playlist_members(user_id);
CREATE TABLE IF NOT EXISTS playlist_tracks (
    entry_id    TEXT PRIMARY KEY,
    playlist_id TEXT NOT NULL REFERENCES playlists(id) ON DELETE CASCADE,
    position    INTEGER NOT NULL,
    track_id    TEXT NOT NULL,
    track       TEXT NOT NULL,
    added_by    TEXT NOT NULL,
    added_at    INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS playlist_tracks_playlist ON playlist_tracks(playlist_id, position);
CREATE TABLE IF NOT EXISTS tokens (
    token_hash  TEXT PRIMARY KEY,
    user_id     TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    created     INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS pair_codes (
    code        TEXT PRIMARY KEY,
    user_id     TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    expires     INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS library (
    user_id     TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    kind        TEXT NOT NULL,
    track_id    TEXT NOT NULL,
    track       TEXT NOT NULL,
    added_at    INTEGER NOT NULL,
    PRIMARY KEY (user_id, kind, track_id)
);
CREATE INDEX IF NOT EXISTS library_user ON library(user_id, kind, added_at);
CREATE TABLE IF NOT EXISTS library_versions (
    user_id     TEXT PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
    version     INTEGER NOT NULL
);
"""

MAX_PLAYLIST_TRACKS = 2000
MAX_PLAYLISTS_PER_USER = 200
MAX_LIKED = 5000
MAX_HISTORY = 300
PAIR_CODE_SECONDS = 600

# Codes in links: letters and digits only, so they survive any chat app.
_ALPHABET = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789"


def new_code(length):
    return "".join(secrets.choice(_ALPHABET) for _ in range(length))


def now_ms():
    return int(time.time() * 1000)


def hash_token(token):
    return hashlib.sha256(token.encode()).hexdigest()


class LimitError(Exception):
    pass


class Database:
    def __init__(self, path):
        self.conn = sqlite3.connect(path, isolation_level=None, check_same_thread=False)
        self.conn.row_factory = sqlite3.Row
        self.conn.execute("PRAGMA journal_mode=WAL")
        self.conn.execute("PRAGMA foreign_keys=ON")
        self.conn.executescript(SCHEMA)
        # Databases made before playlists had an origin (a copy of a YouTube Music playlist: "ytmusic:<id>").
        if "origin" not in {r["name"] for r in self.conn.execute("PRAGMA table_info(playlists)")}:
            self.conn.execute("ALTER TABLE playlists ADD COLUMN origin TEXT NOT NULL DEFAULT ''")

    def close(self):
        self.conn.close()

    def _tx(self):
        return _Transaction(self.conn)

    # ---- users --------------------------------------------------------------

    def create_user(self, name):
        user_id = new_code(12)
        token = secrets.token_urlsafe(32)
        t = now_ms()
        self.conn.execute("INSERT INTO users (id, token_hash, name, created, seen) VALUES (?, ?, ?, ?, ?)",
                          (user_id, hash_token(token), name, t, t))
        return {"id": user_id, "name": name}, token

    def user_by_token(self, token):
        h = hash_token(token)
        row = self.conn.execute("SELECT id, name, seen FROM users WHERE token_hash = ?", (h,)).fetchone()
        if not row:   # another device of the same account (paired with a code)
            row = self.conn.execute("SELECT u.id, u.name, u.seen FROM tokens t JOIN users u ON u.id = t.user_id "
                                    "WHERE t.token_hash = ?", (h,)).fetchone()
        if not row:
            return None
        t = now_ms()
        if t - row["seen"] > 3600_000:
            self.conn.execute("UPDATE users SET seen = ? WHERE id = ?", (t, row["id"]))
        return {"id": row["id"], "name": row["name"]}

    # ---- devices: one account on several devices --------------------------------

    def create_pair_code(self, user_id):
        """A short code another device enters to sign in to this account; single use, ten minutes."""
        t = int(time.time())
        self.conn.execute("DELETE FROM pair_codes WHERE expires < ? OR user_id = ?", (t, user_id))
        code = "".join(secrets.choice("ABCDEFGHJKLMNPQRSTUVWXYZ23456789") for _ in range(8))
        self.conn.execute("INSERT INTO pair_codes (code, user_id, expires) VALUES (?, ?, ?)",
                          (code, user_id, t + PAIR_CODE_SECONDS))
        return code, t + PAIR_CODE_SECONDS

    def redeem_pair_code(self, code):
        """The account and a new token for the device that entered the code, or None."""
        code = (code or "").strip().upper().replace("-", "").replace(" ", "")
        with self._tx():
            row = self.conn.execute("SELECT user_id, expires FROM pair_codes WHERE code = ?", (code,)).fetchone()
            if not row or row["expires"] < time.time():
                return None
            self.conn.execute("DELETE FROM pair_codes WHERE code = ?", (code,))
            token = secrets.token_urlsafe(32)
            self.conn.execute("INSERT INTO tokens (token_hash, user_id, created) VALUES (?, ?, ?)",
                              (hash_token(token), row["user_id"], now_ms()))
            user = self.conn.execute("SELECT id, name FROM users WHERE id = ?", (row["user_id"],)).fetchone()
        return {"id": user["id"], "name": user["name"]}, token

    # ---- the library ---------------------------------------------------------------

    def library_version(self, user_id):
        row = self.conn.execute("SELECT version FROM library_versions WHERE user_id = ?", (user_id,)).fetchone()
        return row["version"] if row else 0

    def _bump_library(self, user_id):
        self.conn.execute("INSERT INTO library_versions (user_id, version) VALUES (?, 1) "
                          "ON CONFLICT(user_id) DO UPDATE SET version = version + 1", (user_id,))
        return self.library_version(user_id)

    def library(self, user_id):
        def kind(k, limit):
            # "at": when it was liked / last played (ms) — the apps put other devices' plays in their history
            return [dict(json.loads(r["track"]), at=r["added_at"]) for r in self.conn.execute(
                "SELECT track, added_at FROM library WHERE user_id = ? AND kind = ? ORDER BY added_at DESC LIMIT ?",
                (user_id, k, limit))]
        return {"version": self.library_version(user_id), "liked": kind("liked", MAX_LIKED),
                "history": kind("history", MAX_HISTORY)}

    def set_liked(self, user_id, track, liked):
        if liked:
            return self.like_many(user_id, [(track, None)])
        with self._tx():
            self.conn.execute("DELETE FROM library WHERE user_id = ? AND kind = 'liked' AND track_id = ?",
                              (user_id, track if isinstance(track, str) else track["id"]))
            return self._bump_library(user_id)

    def like_many(self, user_id, items):
        """[(track, at)]: at (ms) puts the like at its place in the list (an app sending likes made before, in
        their order) and moves one already there; without it the like is new and an existing one stays."""
        with self._tx():
            for track, at in items:
                if at:
                    self.conn.execute(
                        "INSERT INTO library (user_id, kind, track_id, track, added_at) VALUES (?, 'liked', ?, ?, ?) "
                        "ON CONFLICT(user_id, kind, track_id) DO UPDATE SET added_at = excluded.added_at",
                        (user_id, track["id"], json.dumps(track), at))
                else:
                    self.conn.execute("INSERT OR IGNORE INTO library (user_id, kind, track_id, track, added_at) "
                                      "VALUES (?, 'liked', ?, ?, ?)", (user_id, track["id"], json.dumps(track), now_ms()))
            count = self.conn.execute("SELECT COUNT(*) FROM library WHERE user_id = ? AND kind = 'liked'",
                                      (user_id,)).fetchone()[0]
            if count > MAX_LIKED:
                raise LimitError("too many liked tracks")   # nothing of it is kept
            return self._bump_library(user_id)

    def add_history(self, user_id, track):
        with self._tx():
            self.conn.execute("INSERT INTO library (user_id, kind, track_id, track, added_at) VALUES (?, 'history', ?, ?, ?) "
                              "ON CONFLICT(user_id, kind, track_id) DO UPDATE SET added_at = excluded.added_at, "
                              "track = excluded.track", (user_id, track["id"], json.dumps(track), now_ms()))
            self.conn.execute("DELETE FROM library WHERE user_id = ? AND kind = 'history' AND track_id NOT IN "
                              "(SELECT track_id FROM library WHERE user_id = ? AND kind = 'history' "
                              "ORDER BY added_at DESC LIMIT ?)", (user_id, user_id, MAX_HISTORY))
            return self._bump_library(user_id)

    def clear_history(self, user_id):
        with self._tx():
            self.conn.execute("DELETE FROM library WHERE user_id = ? AND kind = 'history'", (user_id,))
            return self._bump_library(user_id)

    def rename_user(self, user_id, name):
        self.conn.execute("UPDATE users SET name = ? WHERE id = ?", (name, user_id))

    def delete_user(self, user_id):
        with self._tx():
            owned = [r["id"] for r in self.conn.execute("SELECT id FROM playlists WHERE owner_id = ?", (user_id,))]
            self.conn.execute("DELETE FROM users WHERE id = ?", (user_id,))
        return owned

    def user_names(self, ids):
        ids = list(set(ids))
        if not ids:
            return {}
        marks = ",".join("?" * len(ids))
        return {r["id"]: r["name"] for r in self.conn.execute(f"SELECT id, name FROM users WHERE id IN ({marks})", ids)}

    # ---- shared tracks ------------------------------------------------------

    def create_share(self, user_id, track):
        # The same track shared twice by the same person gets the same link.
        for row in self.conn.execute("SELECT code, track FROM shares WHERE user_id = ? ORDER BY created DESC LIMIT 200",
                                     (user_id,)):
            if json.loads(row["track"]).get("id") == track["id"]:
                return row["code"]
        code = new_code(8)
        self.conn.execute("INSERT INTO shares (code, track, user_id, created) VALUES (?, ?, ?, ?)",
                          (code, json.dumps(track), user_id, now_ms()))
        return code

    def get_share(self, code, count_open=False):
        row = self.conn.execute("SELECT s.code, s.track, s.created, s.opens, u.name FROM shares s "
                                "JOIN users u ON u.id = s.user_id WHERE s.code = ?", (code,)).fetchone()
        if not row:
            return None
        if count_open:
            self.conn.execute("UPDATE shares SET opens = opens + 1 WHERE code = ?", (code,))
        return {"code": row["code"], "track": json.loads(row["track"]), "by": row["name"],
                "created": row["created"], "opens": row["opens"]}

    # ---- shared playlists ---------------------------------------------------

    def create_playlist(self, owner_id, name, description, tracks, origin=""):
        count = self.conn.execute("SELECT COUNT(*) FROM playlist_members WHERE user_id = ?", (owner_id,)).fetchone()[0]
        if count >= MAX_PLAYLISTS_PER_USER:
            raise LimitError("too many shared playlists")
        playlist_id = new_code(10)
        t = now_ms()
        with self._tx():
            self.conn.execute("INSERT INTO playlists (id, owner_id, name, description, created, updated, origin) "
                              "VALUES (?, ?, ?, ?, ?, ?, ?)", (playlist_id, owner_id, name, description, t, t, origin))
            self.conn.execute("INSERT INTO playlist_members (playlist_id, user_id, joined) VALUES (?, ?, ?)",
                              (playlist_id, owner_id, t))
            self._insert_tracks(playlist_id, owner_id, tracks)
        return playlist_id

    def _insert_tracks(self, playlist_id, user_id, tracks):
        rows = self.conn.execute("SELECT track_id FROM playlist_tracks WHERE playlist_id = ?", (playlist_id,)).fetchall()
        present = {r["track_id"] for r in rows}
        position = self.conn.execute("SELECT COALESCE(MAX(position), -1) FROM playlist_tracks WHERE playlist_id = ?",
                                     (playlist_id,)).fetchone()[0]
        added = 0
        t = now_ms()
        for track in tracks:
            if track["id"] in present:
                continue
            if len(present) >= MAX_PLAYLIST_TRACKS:
                raise LimitError("the playlist is full")
            position += 1
            self.conn.execute("INSERT INTO playlist_tracks (entry_id, playlist_id, position, track_id, track, added_by, added_at) "
                              "VALUES (?, ?, ?, ?, ?, ?, ?)",
                              (new_code(12), playlist_id, position, track["id"], json.dumps(track), user_id, t))
            present.add(track["id"])
            added += 1
        return added

    def _touch(self, playlist_id):
        self.conn.execute("UPDATE playlists SET version = version + 1, updated = ? WHERE id = ?", (now_ms(), playlist_id))

    def playlist_row(self, playlist_id):
        return self.conn.execute("SELECT * FROM playlists WHERE id = ?", (playlist_id,)).fetchone()

    def is_member(self, playlist_id, user_id):
        return self.conn.execute("SELECT 1 FROM playlist_members WHERE playlist_id = ? AND user_id = ?",
                                 (playlist_id, user_id)).fetchone() is not None

    def members(self, playlist_id):
        return [r["user_id"] for r in self.conn.execute(
            "SELECT user_id FROM playlist_members WHERE playlist_id = ? ORDER BY joined", (playlist_id,))]

    def playlist(self, playlist_id, with_tracks=True):
        p = self.playlist_row(playlist_id)
        if not p:
            return None
        member_ids = self.members(playlist_id)
        tracks = []
        if with_tracks:
            tracks = [{"entryId": r["entry_id"], "track": json.loads(r["track"]), "addedBy": r["added_by"],
                       "addedAt": r["added_at"]}
                      for r in self.conn.execute("SELECT * FROM playlist_tracks WHERE playlist_id = ? ORDER BY position",
                                                 (playlist_id,))]
        names = self.user_names(member_ids + [p["owner_id"]] + [t["addedBy"] for t in tracks])
        for t in tracks:
            t["addedByName"] = names.get(t["addedBy"], "")
        count = len(tracks) if with_tracks else self.conn.execute(
            "SELECT COUNT(*) FROM playlist_tracks WHERE playlist_id = ?", (playlist_id,)).fetchone()[0]
        out = {
            "id": p["id"], "name": p["name"], "description": p["description"],
            "owner": {"id": p["owner_id"], "name": names.get(p["owner_id"], "")},
            "members": [{"id": m, "name": names.get(m, "")} for m in member_ids],
            "version": p["version"], "created": p["created"], "updated": p["updated"], "count": count,
            "origin": p["origin"],
        }
        if with_tracks:
            out["tracks"] = tracks
        return out

    def preview_covers(self, playlist_id, limit=4):
        covers = []
        for r in self.conn.execute("SELECT track FROM playlist_tracks WHERE playlist_id = ? ORDER BY position LIMIT 50",
                                   (playlist_id,)):
            cover = json.loads(r["track"]).get("cover")
            if cover and cover not in covers:
                covers.append(cover)
            if len(covers) >= limit:
                break
        return covers

    def playlists_of(self, user_id):
        ids = [r["playlist_id"] for r in self.conn.execute(
            "SELECT playlist_id FROM playlist_members WHERE user_id = ? ORDER BY joined", (user_id,))]
        return [p for p in (self.playlist(i) for i in ids) if p]

    def join_playlist(self, playlist_id, user_id):
        if self.is_member(playlist_id, user_id):
            return False
        count = self.conn.execute("SELECT COUNT(*) FROM playlist_members WHERE user_id = ?", (user_id,)).fetchone()[0]
        if count >= MAX_PLAYLISTS_PER_USER:
            raise LimitError("too many shared playlists")
        with self._tx():
            self.conn.execute("INSERT INTO playlist_members (playlist_id, user_id, joined) VALUES (?, ?, ?)",
                              (playlist_id, user_id, now_ms()))
            self._touch(playlist_id)
        return True

    def leave_playlist(self, playlist_id, user_id):
        with self._tx():
            self.conn.execute("DELETE FROM playlist_members WHERE playlist_id = ? AND user_id = ?", (playlist_id, user_id))
            self._touch(playlist_id)

    def update_playlist(self, playlist_id, name=None, description=None):
        with self._tx():
            if name is not None:
                self.conn.execute("UPDATE playlists SET name = ? WHERE id = ?", (name, playlist_id))
            if description is not None:
                self.conn.execute("UPDATE playlists SET description = ? WHERE id = ?", (description, playlist_id))
            self._touch(playlist_id)

    def delete_playlist(self, playlist_id):
        self.conn.execute("DELETE FROM playlists WHERE id = ?", (playlist_id,))

    def add_tracks(self, playlist_id, user_id, tracks):
        with self._tx():
            added = self._insert_tracks(playlist_id, user_id, tracks)
            if added:
                self._touch(playlist_id)
        return added

    def replace_tracks(self, playlist_id, user_id, tracks):
        """The whole list at once (a copy of a YouTube Music playlist following its source)."""
        with self._tx():
            self.conn.execute("DELETE FROM playlist_tracks WHERE playlist_id = ?", (playlist_id,))
            self._insert_tracks(playlist_id, user_id, tracks[:MAX_PLAYLIST_TRACKS])
            self._touch(playlist_id)

    def remove_track(self, playlist_id, entry_id):
        with self._tx():
            removed = self.conn.execute("DELETE FROM playlist_tracks WHERE playlist_id = ? AND entry_id = ?",
                                        (playlist_id, entry_id)).rowcount
            if removed:
                self._touch(playlist_id)
        return removed > 0

    def move_track(self, playlist_id, entry_id, to):
        with self._tx():
            entries = [r["entry_id"] for r in self.conn.execute(
                "SELECT entry_id FROM playlist_tracks WHERE playlist_id = ? ORDER BY position", (playlist_id,))]
            if entry_id not in entries:
                return False
            entries.remove(entry_id)
            entries.insert(max(0, min(to, len(entries))), entry_id)
            for i, e in enumerate(entries):
                self.conn.execute("UPDATE playlist_tracks SET position = ? WHERE entry_id = ?", (i, e))
            self._touch(playlist_id)
        return True


class _Transaction:
    def __init__(self, conn):
        self.conn = conn

    def __enter__(self):
        self.conn.execute("BEGIN")

    def __exit__(self, exc_type, exc, tb):
        self.conn.execute("ROLLBACK" if exc_type else "COMMIT")
        return False
