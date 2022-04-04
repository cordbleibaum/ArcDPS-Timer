import logging
from datetime import datetime, timedelta
from enum import Enum

import tornado.web
import tornado.locks
import tornado.escape
import tornado.ioloop
from tornado.options import define, options, parse_command_line

define("port", default=5000, help="run on the given port", type=int)
define("debug", default=True, help="run in debug mode")


class TimerStatus(Enum):
     stopped = 0
     started = 1
     prepared = 2
     resetted = 3


class SegmentStatus(object):
    def __init__(self):
        self.is_set = False
        self.is_used = False
        self.start = datetime.now()
        self.end = datetime.now()
        self.shortest_duration = timedelta(seconds=0)
        self.shortest_time = timedelta(seconds=0)


class GroupStatus(object):
    def __init__(self):
        self.update_lock = tornado.locks.Condition()
        self.last_update = datetime.fromtimestamp(0)
        self.start_time = datetime.fromtimestamp(0)
        self.stop_time = datetime.fromtimestamp(0)
        self.status = TimerStatus.stopped
        self.segments : list[SegmentStatus] = []
        self.changeSemaphore = tornado.locks.Semaphore(1)


global_groups : dict[str, GroupStatus] = {}


class JsonHandler(tornado.web.RequestHandler):
    def prepare(self) -> None:
        if self.request.headers['Content-Type'] == 'application/json':
            self.args = tornado.escape.json_decode(self.request.body)


class GroupModifyHandler(JsonHandler):
    async def prepare(self) -> None:
        super().prepare()
        self.status = GroupStatus()
        group_id = self.path_args[0]
        if group_id in global_groups:
            self.status = global_groups[group_id]
        await self.status.changeSemaphore.acquire()

    def on_finish(self) -> None:
        self.status.changeSemaphore.release()
        return super().on_finish()


class MainHandler(tornado.web.RequestHandler):
    def get(self):
        self.write({
            'app': 'arcdps-timer-server-v2',
            'version': 7
        })


class StartHandler(GroupModifyHandler):
    async def post(self, group_id: str):
        pass


class StopHandler(GroupModifyHandler):
    async def post(self, group_id: str):
        pass


class ResetHandler(GroupModifyHandler):
    async def post(self, group_id: str):
        pass


class PrepareHandler(GroupModifyHandler):
    async def post(self, group_id: str):
        pass


class StatusHandler(JsonHandler):
    async def post(self, group_id: str):
        last_update = self.get_argument("last_update", datetime.fromtimestamp(0))

    def on_connection_close(self):
        self.update_future.cancel()


class SegmentHandler(GroupModifyHandler):
    async def post(self, group_id: str):
        pass


class ClearSegmentsHandler(GroupModifyHandler):
    async def post(self, group_id: str):
        pass


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
            (r"/groups/([^/]+)/segment", SegmentHandler),
            (r"/groups/([^/]+)/clear_segment", ClearSegmentsHandler),
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
