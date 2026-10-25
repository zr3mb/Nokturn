"""Tracks as clients send them: only sources every client can play are accepted."""

import re

VIDEO_ID = re.compile(r"^[A-Za-z0-9_-]{6,20}$")
SOUNDCLOUD_ID = re.compile(r"^[0-9]{1,20}$")

_TEXT_LIMITS = {"title": 300, "artist": 300, "artistId": 80, "album": 300}


class TrackError(ValueError):
    pass


def _text(value, limit):
    if not isinstance(value, str):
        return ""
    return value.strip()[:limit]


def _url(value):
    value = _text(value, 1000)
    return value if value.startswith(("https://", "http://")) else ""


def clean_track(raw):
    """Validates a track from a client and returns the stored form.

    YouTube tracks are "yt:<videoId>", SoundCloud tracks "sc:<number>" with the permalink as path.
    Local files and Spotify tracks are converted to one of those by the client before they're sent.
    """
    if not isinstance(raw, dict):
        raise TrackError("track must be an object")
    track_id = _text(raw.get("id"), 64)
    source = raw.get("source")
    if source == "youtube":
        video_id = track_id[3:] if track_id.startswith("yt:") else ""
        if not VIDEO_ID.match(video_id):
            raise TrackError("invalid YouTube video id")
        path = video_id
    elif source == "soundcloud":
        number = track_id[3:] if track_id.startswith("sc:") else ""
        if not SOUNDCLOUD_ID.match(number):
            raise TrackError("invalid SoundCloud track id")
        path = _url(raw.get("path"))
        if path and not path.startswith("https://soundcloud.com/"):
            path = ""
    else:
        raise TrackError("only YouTube Music and SoundCloud tracks can be shared")

    track = {"id": track_id, "source": source, "path": path}
    for key, limit in _TEXT_LIMITS.items():
        track[key] = _text(raw.get(key), limit)
    if not track["title"]:
        raise TrackError("track has no title")
    duration = raw.get("duration")
    track["duration"] = int(duration) if isinstance(duration, (int, float)) and 0 < duration < 86400 else 0
    track["cover"] = _url(raw.get("cover"))
    return track


def listen_url(track):
    """Where the track can be heard without Nokturn."""
    if track["source"] == "youtube":
        return "https://music.youtube.com/watch?v=" + track["path"]
    return track.get("path") or ""
