"""Web pages behind the links: a shared track, a room invite, a playlist invite.

Each page shows what the link is about (with OpenGraph tags, so chat apps show a preview), opens it
in Nokturn through the nokturn:// scheme and offers the download for people who don't have it yet.
"""

from html import escape

from .tracks import listen_url

DOWNLOAD_URL = "https://github.com/zr3mb/Nokturn/releases/latest"

STRINGS = {
    "en": {
        "open": "Open in Nokturn",
        "listen_yt": "Listen on YouTube Music",
        "listen_sc": "Listen on SoundCloud",
        "download": "Don't have Nokturn? Download it",
        "shared_by": "Shared by {name}",
        "track": "Track",
        "room": "Listening room",
        "room_desc_open": "Everyone adds to the queue — {n} listening now",
        "room_desc_host": "{host} picks the music — {n} listening now",
        "room_join": "Join the room",
        "now": "Now playing",
        "idle": "Nothing is playing yet",
        "playlist": "Shared playlist",
        "playlist_desc": "{count} tracks · by {owner} · {members} people add to it",
        "playlist_join": "Join the playlist",
        "missing_title": "Link not found",
        "missing_room": "This room has closed. Rooms disappear a while after everyone leaves.",
        "missing_other": "This link doesn't exist or has been removed.",
        "home_title": "Nokturn",
        "home_desc": "A music player for local files, YouTube Music, Spotify and SoundCloud — with listening rooms and shared playlists.",
        "home_download": "Download Nokturn",
        "hint": "Nothing happened? Nokturn has to be started once on this computer before its links open.",
    },
    "pl": {
        "open": "Otwórz w Nokturn",
        "listen_yt": "Słuchaj w YouTube Music",
        "listen_sc": "Słuchaj w SoundCloud",
        "download": "Nie masz Nokturna? Pobierz go",
        "shared_by": "Udostępnia {name}",
        "track": "Utwór",
        "room": "Pokój słuchania",
        "room_desc_open": "Każdy dodaje do kolejki — słucha teraz {n}",
        "room_desc_host": "Muzykę wybiera {host} — słucha teraz {n}",
        "room_join": "Dołącz do pokoju",
        "now": "Teraz gra",
        "idle": "Jeszcze nic nie gra",
        "playlist": "Wspólna playlista",
        "playlist_desc": "Utwory: {count} · tworzy {owner} · dodaje osób: {members}",
        "playlist_join": "Dołącz do playlisty",
        "missing_title": "Nie ma takiego linku",
        "missing_room": "Ten pokój został zamknięty. Pokoje znikają chwilę po wyjściu wszystkich.",
        "missing_other": "Ten link nie istnieje albo został usunięty.",
        "home_title": "Nokturn",
        "home_desc": "Odtwarzacz muzyki z plików, YouTube Music, Spotify i SoundCloud — z pokojami słuchania i wspólnymi playlistami.",
        "home_download": "Pobierz Nokturn",
        "hint": "Nic się nie stało? Nokturn musi zostać raz uruchomiony na tym komputerze, zanim zacznie otwierać linki.",
    },
}


def language(request):
    header = request.headers.get("Accept-Language", "")
    for part in header.split(","):
        code = part.split(";")[0].strip().lower()
        if code.startswith("pl"):
            return "pl"
        if code.startswith("en"):
            return "en"
    return "en"


STYLE = """
:root { color-scheme: dark; --bg: #0f1116; --surface: #181b23; --text: #eef0f4; --dim: #9aa1b0;
        --faint: #6b7282; --accent: #9fbfa8; }
* { box-sizing: border-box; }
body { margin: 0; min-height: 100vh; display: flex; align-items: center; justify-content: center;
       background: radial-gradient(1200px 700px at 20% 0%, #232a36 0%, var(--bg) 60%); color: var(--text);
       font: 16px/1.5 "Segoe UI", system-ui, -apple-system, Roboto, sans-serif; padding: 24px 16px; }
main { width: 100%; max-width: 440px; background: var(--surface); border: 1px solid rgba(255,255,255,.07);
       border-radius: 24px; padding: 28px; box-shadow: 0 30px 80px rgba(0,0,0,.45); text-align: center; }
.cover { width: 220px; height: 220px; max-width: 100%; border-radius: 18px; object-fit: cover; display: block;
         margin: 0 auto 22px; background: linear-gradient(135deg, #3e4f5a, #9fb5a8); box-shadow: 0 18px 50px rgba(0,0,0,.5); }
.mosaic { display: grid; grid-template-columns: 1fr 1fr; width: 220px; height: 220px; max-width: 100%; margin: 0 auto 22px;
          border-radius: 18px; overflow: hidden; box-shadow: 0 18px 50px rgba(0,0,0,.5); }
.mosaic img { width: 100%; height: 100%; object-fit: cover; display: block; }
.kicker { color: var(--accent); font-size: 12px; font-weight: 600; letter-spacing: .14em; text-transform: uppercase; }
h1 { font-size: 26px; line-height: 1.2; margin: 6px 0 4px; overflow-wrap: anywhere; }
.sub { color: var(--dim); margin: 0 0 6px; overflow-wrap: anywhere; }
.meta { color: var(--faint); font-size: 14px; margin: 0 0 22px; }
.button { display: block; text-decoration: none; border-radius: 999px; padding: 13px 18px; font-weight: 600; margin-top: 10px; }
.primary { background: var(--accent); color: #10161a; }
.ghost { background: rgba(255,255,255,.06); color: var(--text); }
.small { display: inline-block; margin-top: 18px; color: var(--dim); font-size: 14px; }
.hint { color: var(--faint); font-size: 12px; margin-top: 14px; }
.logo { font-weight: 600; letter-spacing: .02em; color: var(--dim); margin-bottom: 18px; }
"""


def _page(lang, title, description, body, url, image=""):
    og_image = f'<meta property="og:image" content="{escape(image)}">' if image else ""
    twitter = "summary_large_image" if image else "summary"
    return f"""<!doctype html>
<html lang="{lang}">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{escape(title)}</title>
<meta name="description" content="{escape(description)}">
<meta name="theme-color" content="#0f1116">
<meta property="og:site_name" content="Nokturn">
<meta property="og:type" content="website">
<meta property="og:title" content="{escape(title)}">
<meta property="og:description" content="{escape(description)}">
<meta property="og:url" content="{escape(url)}">
{og_image}
<meta name="twitter:card" content="{twitter}">
<style>{STYLE}</style>
</head>
<body><main>
<div class="logo">Nokturn</div>
{body}
</main></body>
</html>"""


def _open_buttons(s, deep_link, join_label=None):
    return (f'<a class="button primary" href="{escape(deep_link)}">{escape(join_label or s["open"])}</a>'
            f'<p class="hint">{escape(s["hint"])}</p>')


def _download(s):
    return f'<a class="small" href="{DOWNLOAD_URL}">{escape(s["download"])}</a>'


def _cover(url):
    if url:
        return f'<img class="cover" src="{escape(url)}" alt="">'
    return '<div class="cover"></div>'


def track_page(lang, share, url):
    s = STRINGS[lang]
    t = share["track"]
    listen = listen_url(t)
    listen_label = s["listen_yt"] if t["source"] == "youtube" else s["listen_sc"]
    title = t["title"] + (" — " + t["artist"] if t["artist"] else "")
    body = (f'{_cover(t["cover"])}<div class="kicker">{escape(s["track"])}</div>'
            f'<h1>{escape(t["title"])}</h1><p class="sub">{escape(t["artist"])}</p>'
            f'<p class="meta">{escape(s["shared_by"].format(name=share["by"]))}</p>'
            f'{_open_buttons(s, "nokturn://t/" + share["code"])}')
    if listen:
        body += f'<a class="button ghost" href="{escape(listen)}">{escape(listen_label)}</a>'
    body += _download(s)
    return _page(lang, title, s["shared_by"].format(name=share["by"]), body, url, t["cover"])


def room_page(lang, preview, url):
    s = STRINGS[lang]
    track = preview["track"]
    if preview["mode"] == "open":
        desc = s["room_desc_open"].format(n=preview["listeners"])
    else:
        desc = s["room_desc_host"].format(host=preview["host"], n=preview["listeners"])
    now = (f'{escape(s["now"])}: {escape(track["title"])}' + (f' — {escape(track["artist"])}' if track["artist"] else "")
           if track else escape(s["idle"]))
    body = (f'{_cover(track["cover"] if track else "")}<div class="kicker">{escape(s["room"])}</div>'
            f'<h1>{escape(preview["name"])}</h1><p class="sub">{escape(desc)}</p><p class="meta">{now}</p>'
            f'{_open_buttons(s, "nokturn://r/" + preview["code"], s["room_join"])}{_download(s)}')
    return _page(lang, preview["name"] + " · " + s["room"], desc, body, url, track["cover"] if track else "")


def playlist_page(lang, playlist, covers, url):
    s = STRINGS[lang]
    desc = s["playlist_desc"].format(count=playlist["count"], owner=playlist["owner"]["name"],
                                     members=len(playlist["members"]))
    if len(covers) >= 4:
        art = '<div class="mosaic">' + "".join(f'<img src="{escape(c)}" alt="">' for c in covers[:4]) + "</div>"
    else:
        art = _cover(covers[0] if covers else "")
    body = (f'{art}<div class="kicker">{escape(s["playlist"])}</div><h1>{escape(playlist["name"])}</h1>'
            + (f'<p class="sub">{escape(playlist["description"])}</p>' if playlist["description"] else "")
            + f'<p class="meta">{escape(desc)}</p>'
            f'{_open_buttons(s, "nokturn://p/" + playlist["id"], s["playlist_join"])}{_download(s)}')
    return _page(lang, playlist["name"] + " · " + s["playlist"], desc, body, url, covers[0] if covers else "")


def missing_page(lang, kind, url):
    s = STRINGS[lang]
    text = s["missing_room"] if kind == "room" else s["missing_other"]
    body = f'<h1>{escape(s["missing_title"])}</h1><p class="sub">{escape(text)}</p>{_download(s)}'
    return _page(lang, s["missing_title"], text, body, url)


def home_page(lang, url):
    s = STRINGS[lang]
    body = (f'<h1>{escape(s["home_title"])}</h1><p class="sub">{escape(s["home_desc"])}</p>'
            f'<a class="button primary" href="{DOWNLOAD_URL}">{escape(s["home_download"])}</a>')
    return _page(lang, s["home_title"], s["home_desc"], body, url)
