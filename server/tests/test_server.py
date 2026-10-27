"""python3 -m unittest discover -s tests (from the server directory)."""

import asyncio
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from aiohttp.test_utils import AioHTTPTestCase  # noqa: E402

from nokturn_server import app as app_module  # noqa: E402
from nokturn_server import rooms as rooms_module  # noqa: E402
from nokturn_server.app import Config, create_app  # noqa: E402
from nokturn_server.rooms import Room, RoomError  # noqa: E402

YT = {"id": "yt:dQw4w9WgXcQ", "source": "youtube", "path": "dQw4w9WgXcQ", "title": "Track one",
      "artist": "Artist", "duration": 200, "cover": "https://i.ytimg.com/vi/x/hq.jpg"}
YT2 = {"id": "yt:aaaaaaaaaaa", "source": "youtube", "path": "aaaaaaaaaaa", "title": "Track two",
       "artist": "Artist", "duration": 180}
SC = {"id": "sc:123456", "source": "soundcloud", "path": "https://soundcloud.com/a/b", "title": "Track three",
      "duration": 100}
LOCAL = {"id": "/music/a.flac", "source": "local", "path": "/music/a.flac", "title": "Local"}


class ServerTest(AioHTTPTestCase):
    async def get_application(self):
        self.tmp = tempfile.TemporaryDirectory()
        return create_app(Config(os.path.join(self.tmp.name, "test.db"), "https://nokturn.example"))

    async def asyncTearDown(self):
        await super().asyncTearDown()
        self.tmp.cleanup()

    async def register(self, name="Ala"):
        r = await self.client.post("/api/v1/register", json={"name": name})
        self.assertEqual(r.status, 200)
        data = await r.json()
        return data["token"], data["user"]

    def auth(self, token):
        return {"Authorization": "Bearer " + token}

    async def ws(self, token):
        ws = await self.client.ws_connect("/api/v1/ws", headers=self.auth(token))
        hello = await ws.receive_json(timeout=2)
        self.assertEqual(hello["type"], "hello")
        return ws

    async def next_of(self, ws, kind):
        while True:
            msg = await ws.receive_json(timeout=2)
            if msg["type"] == kind:
                return msg

    async def room_where(self, ws, condition):
        while True:
            room = (await self.next_of(ws, "room"))["room"]
            if condition(room):
                return room

    # ---- accounts -------------------------------------------------------

    async def test_register_and_rename(self):
        token, user = await self.register("  Ala\n  Kot ")
        self.assertEqual(user["name"], "Ala Kot")
        r = await self.client.get("/api/v1/me", headers=self.auth(token))
        self.assertEqual((await r.json())["user"]["id"], user["id"])
        r = await self.client.patch("/api/v1/me", json={"name": "Ola"}, headers=self.auth(token))
        self.assertEqual((await r.json())["user"]["name"], "Ola")
        r = await self.client.get("/api/v1/me", headers=self.auth("wrong"))
        self.assertEqual(r.status, 401)

    async def test_bad_json(self):
        r = await self.client.post("/api/v1/register", data="nope", headers={"Content-Type": "application/json"})
        self.assertEqual(r.status, 400)
        self.assertEqual((await r.json())["error"], "bad_request")

    # ---- shares ---------------------------------------------------------

    async def test_share_link(self):
        token, _ = await self.register()
        r = await self.client.post("/api/v1/shares", json={"track": YT}, headers=self.auth(token))
        data = await r.json()
        self.assertTrue(data["url"].startswith("https://nokturn.example/t/"))
        again = await (await self.client.post("/api/v1/shares", json={"track": YT}, headers=self.auth(token))).json()
        self.assertEqual(again["code"], data["code"])

        share = await (await self.client.get(f'/api/v1/shares/{data["code"]}')).json()
        self.assertEqual(share["track"]["title"], "Track one")
        self.assertEqual(share["by"], "Ala")

        page = await self.client.get(f'/t/{data["code"]}', headers={"Accept-Language": "pl-PL,pl;q=0.9"})
        html = await page.text()
        self.assertIn("Otwórz w Nokturn", html)
        self.assertIn(f'nokturn://t/{data["code"]}', html)
        self.assertIn('og:image', html)
        self.assertIn("music.youtube.com/watch?v=dQw4w9WgXcQ", html)

        r = await self.client.post("/api/v1/shares", json={"track": LOCAL}, headers=self.auth(token))
        self.assertEqual(r.status, 400)
        self.assertEqual((await self.client.get("/t/missing")).status, 404)

    async def test_share_escapes_html(self):
        token, _ = await self.register()
        evil = dict(YT, title='<script>alert(1)</script>', cover='javascript:alert(1)')
        data = await (await self.client.post("/api/v1/shares", json={"track": evil}, headers=self.auth(token))).json()
        html = await (await self.client.get(f'/t/{data["code"]}')).text()
        self.assertNotIn("<script>", html)
        self.assertNotIn("javascript:", html)

    # ---- shared playlists -----------------------------------------------

    async def test_shared_playlist_flow(self):
        owner, _ = await self.register("Owner")
        friend, friend_user = await self.register("Friend")
        owner_ws = await self.ws(owner)

        r = await self.client.post("/api/v1/playlists", json={"name": "Road trip", "tracks": [YT, LOCAL]},
                                   headers=self.auth(owner))
        p = await r.json()
        self.assertEqual(p["count"], 1)   # the local file is left out
        self.assertTrue(p["url"].endswith("/p/" + p["id"]))

        preview = await (await self.client.get(f'/api/v1/playlists/{p["id"]}', headers=self.auth(friend))).json()
        self.assertFalse(preview["member"])
        self.assertNotIn("tracks", preview)

        joined = await (await self.client.post(f'/api/v1/playlists/{p["id"]}/join', headers=self.auth(friend))).json()
        self.assertEqual(len(joined["members"]), 2)
        await self.next_of(owner_ws, "playlist")

        added = await (await self.client.post(f'/api/v1/playlists/{p["id"]}/tracks', json={"tracks": [YT, SC]},
                                              headers=self.auth(friend))).json()
        self.assertEqual(added["added"], 1)   # YT was there already
        self.assertEqual([t["track"]["id"] for t in added["tracks"]], ["yt:dQw4w9WgXcQ", "sc:123456"])
        self.assertEqual(added["tracks"][1]["addedByName"], "Friend")
        changed = await self.next_of(owner_ws, "playlist")
        self.assertEqual(changed["id"], p["id"])

        mine = await (await self.client.get("/api/v1/playlists", headers=self.auth(friend))).json()
        self.assertEqual([x["id"] for x in mine["playlists"]], [p["id"]])

        entry = added["tracks"][0]["entryId"]
        moved = await (await self.client.post(f'/api/v1/playlists/{p["id"]}/tracks/{entry}/move', json={"to": 1},
                                              headers=self.auth(friend))).json()
        self.assertEqual(moved["tracks"][1]["entryId"], entry)
        removed = await (await self.client.delete(f'/api/v1/playlists/{p["id"]}/tracks/{entry}',
                                                  headers=self.auth(friend))).json()
        self.assertEqual(removed["count"], 1)

        r = await self.client.patch(f'/api/v1/playlists/{p["id"]}', json={"name": "x"}, headers=self.auth(friend))
        self.assertEqual(r.status, 403)
        r = await self.client.delete(f'/api/v1/playlists/{p["id"]}', headers=self.auth(friend))
        self.assertEqual(r.status, 403)

        page = await (await self.client.get(f'/p/{p["id"]}')).text()
        self.assertIn("Road trip", page)
        self.assertIn(f'nokturn://p/{p["id"]}', page)

        r = await self.client.post(f'/api/v1/playlists/{p["id"]}/leave', headers=self.auth(friend))
        self.assertEqual(r.status, 200)
        r = await self.client.post(f'/api/v1/playlists/{p["id"]}/tracks', json={"tracks": [YT2]}, headers=self.auth(friend))
        self.assertEqual(r.status, 403)

        r = await self.client.delete(f'/api/v1/playlists/{p["id"]}', headers=self.auth(owner))
        self.assertEqual(r.status, 200)
        await self.next_of(owner_ws, "playlist.removed")
        self.assertEqual((await self.client.get(f'/p/{p["id"]}')).status, 404)
        await owner_ws.close()

    # ---- rooms ----------------------------------------------------------

    async def test_room_open_mode(self):
        a, user_a = await self.register("A")
        b, user_b = await self.register("B")
        wa, wb = await self.ws(a), await self.ws(b)

        await wa.send_json({"type": "room.create", "name": "Friday", "mode": "open"})
        room = (await self.next_of(wa, "room"))["room"]
        code = room["code"]
        self.assertEqual(room["hostId"], user_a["id"])

        preview = await (await self.client.get(f"/api/v1/rooms/{code.lower()}")).json()
        self.assertEqual(preview["name"], "Friday")
        html = await (await self.client.get(f"/r/{code}")).text()
        self.assertIn(f"nokturn://r/{code}", html)

        await wb.send_json({"type": "room.join", "code": code})
        room = (await self.next_of(wb, "room"))["room"]
        self.assertEqual(len(room["members"]), 2)
        await self.next_of(wa, "room")

        # Anyone adds; the first track starts on its own.
        await wb.send_json({"type": "room.add", "tracks": [YT, YT2, LOCAL]})
        room = (await self.next_of(wa, "room"))["room"]
        self.assertEqual(len(room["queue"]), 2)
        self.assertEqual(room["current"], 0)
        self.assertTrue(room["playing"])
        self.assertEqual(room["queue"][0]["addedByName"], "B")
        await self.next_of(wb, "room")

        # An early "ended" from someone who isn't the host doesn't skip the track.
        first = room["queue"][0]["itemId"]
        await wb.send_json({"type": "room.ended", "itemId": first})
        await wb.send_json({"type": "ping", "t": 1})
        self.assertEqual((await wb.receive_json(timeout=2))["type"], "pong")

        # The host's "ended" moves the room on.
        await wa.send_json({"type": "room.ended", "itemId": first})
        room = (await self.next_of(wb, "room"))["room"]
        self.assertEqual(room["current"], 1)
        await self.next_of(wa, "room")

        await wb.send_json({"type": "room.pause", "position": 12.5})
        room = (await self.next_of(wa, "room"))["room"]
        self.assertFalse(room["playing"])
        self.assertAlmostEqual(room["position"], 12.5)

        await wa.send_json({"type": "room.leave"})
        await self.next_of(wa, "room.left")
        room = await self.room_where(wb, lambda r: len(r["members"]) == 1)
        self.assertEqual(room["hostId"], user_b["id"])
        await wa.close()
        await wb.close()

    async def test_room_host_mode(self):
        a, _ = await self.register("Host")
        b, user_b = await self.register("Guest")
        wa, wb = await self.ws(a), await self.ws(b)
        await wa.send_json({"type": "room.create", "mode": "host"})
        room = (await self.next_of(wa, "room"))["room"]
        self.assertEqual(room["name"], "Host's room")
        await wb.send_json({"type": "room.join", "code": room["code"]})
        await self.next_of(wb, "room")

        await wb.send_json({"type": "room.add", "tracks": [YT]})
        err = await self.next_of(wb, "error")
        self.assertEqual(err["error"], "host_only")

        await wa.send_json({"type": "room.host", "userId": user_b["id"]})
        room = (await self.next_of(wb, "room"))["room"]
        self.assertEqual(room["hostId"], user_b["id"])
        await wb.send_json({"type": "room.add", "tracks": [YT]})
        room = (await self.next_of(wb, "room"))["room"]
        self.assertEqual(len(room["queue"]), 1)

        await wb.send_json({"type": "room.kick", "userId": room["members"][0]["id"]})
        left = await self.next_of(wa, "room.left")
        self.assertEqual(left["reason"], "kicked")
        await wa.close()
        await wb.close()

    async def test_room_disconnect_grace(self):
        a, user_a = await self.register("A")
        wa = await self.ws(a)
        await wa.send_json({"type": "room.create"})
        code = (await self.next_of(wa, "room"))["room"]["code"]
        await wa.close()
        await asyncio.sleep(0.1)
        room = self.app[app_module.ROOMS].get(code)
        self.assertIn(user_a["id"], room.members)
        self.assertFalse(room.state()["members"][0]["online"])
        wa = await self.ws(a)
        await wa.send_json({"type": "room.join", "code": code})
        room = (await self.next_of(wa, "room"))["room"]
        self.assertTrue(room["members"][0]["online"])
        await wa.close()

    async def test_unknown_room(self):
        a, _ = await self.register()
        wa = await self.ws(a)
        await wa.send_json({"type": "room.join", "code": "ZZZZZZ"})
        self.assertEqual((await self.next_of(wa, "error"))["error"], "no_room")
        self.assertEqual((await self.client.get("/r/ZZZZZZ")).status, 404)
        await wa.close()

    async def test_likes_in_order_and_mirrored_playlists(self):
        a, _ = await self.register("Ala")
        items = [{"track": YT, "at": 1000}, {"track": YT2, "at": 2000}]
        r = await self.client.post("/api/v1/library/liked", json={"items": items}, headers=self.auth(a))
        self.assertEqual(r.status, 200)
        lib = await (await self.client.get("/api/v1/library", headers=self.auth(a))).json()
        self.assertEqual([t["id"] for t in lib["liked"]], [YT2["id"], YT["id"]])
        # moved: the same like with a newer time comes first
        await self.client.post("/api/v1/library/liked", json={"track": YT, "at": 3000}, headers=self.auth(a))
        lib = await (await self.client.get("/api/v1/library", headers=self.auth(a))).json()
        self.assertEqual([t["id"] for t in lib["liked"]], [YT["id"], YT2["id"]])

        r = await self.client.post("/api/v1/playlists", json={"name": "Copy", "tracks": [YT], "origin": "ytmusic:PL1"},
                                   headers=self.auth(a))
        p = await r.json()
        self.assertEqual(p["origin"], "ytmusic:PL1")
        r = await self.client.put(f'/api/v1/playlists/{p["id"]}/tracks', json={"tracks": [YT2, YT]}, headers=self.auth(a))
        self.assertEqual([e["track"]["id"] for e in (await r.json())["tracks"]], [YT2["id"], YT["id"]])
        b, _ = await self.register("Ola")
        r = await self.client.put(f'/api/v1/playlists/{p["id"]}/tracks', json={"tracks": []}, headers=self.auth(b))
        self.assertEqual(r.status, 403)

    async def test_playlist_password(self):
        a, _ = await self.register("Ala")
        b, _ = await self.register("Ola")
        p = await (await self.client.post("/api/v1/playlists", json={"name": "Secret", "tracks": [YT]},
                                          headers=self.auth(a))).json()
        r = await self.client.patch(f'/api/v1/playlists/{p["id"]}', json={"password": "kot123"}, headers=self.auth(a))
        self.assertTrue((await r.json())["protected"])
        preview = await (await self.client.get(f'/api/v1/playlists/{p["id"]}', headers=self.auth(b))).json()
        self.assertEqual((preview["name"], preview["protected"]), ("", True))
        r = await self.client.post(f'/api/v1/playlists/{p["id"]}/join', headers=self.auth(b))
        self.assertEqual((await r.json())["error"], "password_required")
        r = await self.client.post(f'/api/v1/playlists/{p["id"]}/join', json={"password": "pies"}, headers=self.auth(b))
        self.assertEqual((await r.json())["error"], "bad_password")
        r = await self.client.post(f'/api/v1/playlists/{p["id"]}/join', json={"password": "kot123"}, headers=self.auth(b))
        self.assertEqual(r.status, 200)
        self.assertEqual(len((await r.json())["tracks"]), 1)
        page = await (await self.client.get(f'/p/{p["id"]}')).text()
        self.assertNotIn("Secret", page)
        await self.client.patch(f'/api/v1/playlists/{p["id"]}', json={"password": ""}, headers=self.auth(a))
        preview = await (await self.client.get(f'/api/v1/playlists/{p["id"]}', headers=self.auth(b))).json()
        self.assertFalse(preview["protected"])

    async def test_library_and_pairing(self):
        a, _ = await self.register("Ala")
        r = await self.client.get("/api/v1/library", headers=self.auth(a))
        self.assertEqual(await r.json(), {"version": 0, "liked": [], "history": []})
        r = await self.client.post("/api/v1/library/liked", json={"track": YT}, headers=self.auth(a))
        self.assertEqual((await r.json())["version"], 1)
        await self.client.post("/api/v1/library/liked", json={"track": YT}, headers=self.auth(a))   # twice: once
        r = await self.client.post("/api/v1/library/liked", json={"track": LOCAL}, headers=self.auth(a))
        self.assertEqual(r.status, 400)
        for t in (YT, YT2, YT):
            await self.client.post("/api/v1/library/history", json={"track": t}, headers=self.auth(a))
        lib = await (await self.client.get("/api/v1/library", headers=self.auth(a))).json()
        self.assertEqual([t["id"] for t in lib["liked"]], [YT["id"]])
        self.assertEqual([t["id"] for t in lib["history"]], [YT["id"], YT2["id"]])   # latest first, no repeats

        # a second device signs in to the same account with a code
        code = (await (await self.client.post("/api/v1/me/pair", headers=self.auth(a))).json())["code"]
        r = await self.client.post("/api/v1/pair", json={"code": "WRONG123"})
        self.assertEqual(r.status, 404)
        r = await self.client.post("/api/v1/pair", json={"code": code.lower()})
        self.assertEqual(r.status, 200)
        b = (await r.json())["token"]
        self.assertEqual((await self.client.post("/api/v1/pair", json={"code": code})).status, 404)   # single use
        lib_b = await (await self.client.get("/api/v1/library", headers=self.auth(b))).json()
        self.assertEqual(lib_b["version"], lib["version"])
        await self.client.delete("/api/v1/library/liked/" + YT["id"], headers=self.auth(b))
        lib = await (await self.client.get("/api/v1/library", headers=self.auth(a))).json()
        self.assertEqual(lib["liked"], [])
        await self.client.delete("/api/v1/library/history", headers=self.auth(a))
        self.assertEqual((await (await self.client.get("/api/v1/library", headers=self.auth(b))).json())["history"], [])
        self.assertEqual((await self.client.delete("/api/v1/me")).status, 401)

    async def test_ws_ticket(self):
        a, user = await self.register("Ala")
        ticket = (await (await self.client.post("/api/v1/ws/ticket", headers=self.auth(a))).json())["ticket"]
        ws = await self.client.ws_connect("/api/v1/ws?ticket=" + ticket)
        self.assertEqual((await ws.receive_json(timeout=2))["user"]["id"], user["id"])
        await ws.close()
        with self.assertRaises(Exception):   # single use
            await self.client.ws_connect("/api/v1/ws?ticket=" + ticket)


class RoomLogicTest(unittest.TestCase):
    def setUp(self):
        self.room = Room("ABCDEF", "Test", "open", "a")
        self.room.join({"id": "a", "name": "A"})
        self.room.join({"id": "b", "name": "B"})

    def test_queue_and_navigation(self):
        r = self.room
        r.add("a", [YT, YT2])
        self.assertEqual(r.current, 0)
        r.next("b")
        self.assertEqual(r.current, 1)
        r.next("b")
        self.assertTrue(r.finished)
        self.assertFalse(r.playing)
        r.add("b", [SC])           # adding after the end starts the new track
        self.assertEqual(r.current, 2)
        self.assertTrue(r.playing)
        r.previous("a")            # at the start of a track: back to the previous one
        self.assertEqual(r.current, 1)

    def test_play_next_and_remove_current(self):
        r = self.room
        r.add("a", [YT, YT2])
        r.add("a", [SC], play_next=True)
        self.assertEqual([i["track"]["id"] for i in r.queue], ["yt:dQw4w9WgXcQ", "sc:123456", "yt:aaaaaaaaaaa"])
        r.remove("a", r.queue[0]["itemId"])
        self.assertEqual(r.current_item()["track"]["id"], "sc:123456")
        r.remove("a", r.queue[-1]["itemId"])
        r.remove("a", r.queue[0]["itemId"])
        self.assertTrue(r.finished)

    def test_overdue_track_advances(self):
        r = self.room
        r.add("a", [YT, YT2])
        r.since -= int((YT["duration"] + rooms_module.END_SLACK + 1) * 1000)
        self.assertTrue(r.tick())
        self.assertEqual(r.current, 1)

    def test_host_mode_permissions(self):
        r = self.room
        r.set_mode("a", "host")
        with self.assertRaises(RoomError):
            r.add("b", [YT])
        with self.assertRaises(RoomError):
            r.set_mode("b", "open")
        r.add("a", [YT])
        self.assertFalse(r.ended("b", r.current_item()["itemId"]))

    def test_offline_host_hands_over(self):
        r = self.room
        r.disconnect("a")
        r.members["a"]["left_at"] -= rooms_module.OFFLINE_GRACE / 3 + 1
        self.assertTrue(r.expire_members())
        self.assertEqual(r.host_id, "b")
        r.members["a"]["left_at"] -= rooms_module.OFFLINE_GRACE
        r.expire_members()
        self.assertNotIn("a", r.members)

    def test_empty_room_expires(self):
        r = self.room
        r.leave("a")
        r.leave("b")
        self.assertFalse(r.is_expired())
        self.assertTrue(r.is_expired(now=r.empty_since + rooms_module.EMPTY_ROOM_TTL + 1))


if __name__ == "__main__":
    unittest.main()
