"""python3 -m nokturn_server — settings come from the arguments or NOKTURN_* environment variables."""

import argparse
import logging
import os

from aiohttp import web

from .app import Config, create_app


def main():
    parser = argparse.ArgumentParser(prog="nokturn_server", description="Nokturn online features server")
    parser.add_argument("--host", default=os.environ.get("NOKTURN_HOST", "127.0.0.1"))
    parser.add_argument("--port", type=int, default=int(os.environ.get("NOKTURN_PORT", "8710")))
    parser.add_argument("--db", default=os.environ.get("NOKTURN_DB", "nokturn.db"))
    parser.add_argument("--public-url", default=os.environ.get("NOKTURN_PUBLIC_URL", "https://nokturn.404piwko.wtf"),
                        help="the address links point to")
    parser.add_argument("--no-proxy", action="store_true",
                        help="don't trust X-Real-IP from localhost (when not running behind nginx)")
    args = parser.parse_args()

    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(name)s: %(message)s")
    app = create_app(Config(args.db, args.public_url, trust_proxy=not args.no_proxy))
    web.run_app(app, host=args.host, port=args.port, access_log=None, print=None)


if __name__ == "__main__":
    main()
