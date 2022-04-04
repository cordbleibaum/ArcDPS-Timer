import logging
from datetime import datetime, timedelta
from enum import Enum

import asyncio
import tornado.web
import tornado.locks
import tornado.escape
import tornado.ioloop
from tornado.options import define, options, parse_command_line

define("port", default=5000, help="run on the given port", type=int)
define("debug", default=True, help="run in debug mode")


class TimerStatus(Enum):
     stopped = 0
     running = 1
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
        self.group = GroupStatus()
        group_id = self.path_args[0]
        if group_id in global_groups:
            self.group = global_groups[group_id]
        else:
            global_groups[group_id] = self.group
        await self.group.changeSemaphore.acquire()

    def on_finish(self) -> None:
        self.group.last_update = self.args.update_time
        self.group.changeSemaphore.release()
        self.group.update_lock.notify_all()
        return super().on_finish()


class MainHandler(tornado.web.RequestHandler):
    def get(self):
        self.write({
            'app': 'arcdps-timer-server-v2',
            'version': 7
        })


class StartHandler(GroupModifyHandler):
    async def post(self):
        if self.group.status == TimerStatus.running:
            is_newer = self.group.start_time < self.args.time
            if is_newer:
                self.group.start_time = self.args.time
        else:
            self.group.status = TimerStatus.running
            self.group.start_time = self.args.time


class StopHandler(GroupModifyHandler):
    async def post(self):
        if self.group.status in [TimerStatus.running, TimerStatus.prepared]:
            self.group.status = TimerStatus.stopped
            self.group.stop_time = self.args.time
        elif self.group.status == TimerStatus.stopped:
            is_older = self.group.stop_time > self.args.time
            if is_older:
                self.group.stop_time = self.args.time


class ResetHandler(GroupModifyHandler):
    async def post(self):
        self.group.status = TimerStatus.resetted


class PrepareHandler(GroupModifyHandler):
    async def post(self):
        self.group.status = TimerStatus.prepared


class StatusHandler(JsonHandler):
    async def prepare(self) -> None:
        super().prepare()
        self.group = GroupStatus()
        group_id = self.path_args[0]
        if group_id in global_groups:
            self.group = global_groups[group_id]
        else:
            global_groups[group_id] = self.group

    async def post(self):
        last_update = self.args.update_time
        is_newer = self.group.start_time < last_update

        if not is_newer:
            self.update_future = self.group.update_lock.wait()
            try:
                await self.wait_future
            except asyncio.CancelledError:
                return

        return_json = {}
        return_json["status"] = self.group.status
        return_json["start_time"] = self.group.start_time
        return_json["stop_time"] = self.group.stop_time
        if self.request.connection.stream.closed():
            return
        self.write(return_json)

    def on_connection_close(self):
        self.update_future.cancel()


class SegmentHandler(GroupModifyHandler):
    async def post(self):
        pass


class ClearSegmentsHandler(GroupModifyHandler):
    async def post(self):
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
