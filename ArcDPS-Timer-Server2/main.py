import logging
from datetime import datetime
from typing import Dict

import tornado.web
import tornado.locks
import tornado.escape
import tornado.ioloop
from tornado.options import define, options, parse_command_line

define("port", default=5000, help="run on the given port", type=int)
define("debug", default=True, help="run in debug mode")


class GroupStatus(object):
    def __init__(self):
        self.update_lock = tornado.locks.Condition()


global_groups : Dict[str, GroupStatus] = {}


class JsonHandler(tornado.web.RequestHandler):
    def prepare(self):
        if self.request.headers['Content-Type'] == 'application/json':
            self.args = tornado.escape.json_decode(self.request.body)


class MainHandler(tornado.web.RequestHandler):
    def get(self):
        self.write({
            'app': 'arcdps-timer-server-v2',
            'version': 7
        })


class StartHandler(JsonHandler):
    def post(self, group_id: str):
        pass


class StopHandler(JsonHandler):
    def post(self, group_id: str):
        pass


class ResetHandler(JsonHandler):
    def post(self, group_id: str):
        pass


class PrepareHandler(JsonHandler):
    def post(self, group_id: str):
        pass


class StatusHandler(JsonHandler):
    async def post(self, group_id: str):
        last_update = self.get_argument("last_update", datetime.fromtimestamp(0))

    def on_connection_close(self):
        self.update_future.cancel()


def cleanupGroups():
    logging.info("Running cleanup")


def main():
    logging.basicConfig(level=logging.INFO)
    logging.info("Starting server")
    parse_command_line()
    app = tornado.web.Application(
        [
            (r"/", MainHandler),
            (r"/groups/([^/]+)/", StatusHandler),
            (r"/groups/([^/]+)/start", StartHandler),
            (r"/groups/([^/]+)/stop", StopHandler),
            (r"/groups/([^/]+)/prepare", PrepareHandler),
            (r"/groups/([^/]+)/reset", ResetHandler),
        ],
        debug=options.debug
    )
    app.listen(options.port)
   
    cleanupTimer = 1000*60*60
    cleanupCallback = tornado.ioloop.PeriodicCallback(callback=cleanupGroups, callback_time=cleanupTimer)
    cleanupCallback.start()

    io_loop = tornado.ioloop.IOLoop.current()
    io_loop.start()


if __name__ == "__main__":
    main()
