import logging
import json
from datetime import datetime, timedelta
from enum import Enum
import random

import asyncio
import tornado.web
import tornado.locks
import tornado.escape
import tornado.ioloop
from tornado.options import define, options, parse_command_line


define("port", default=5001, help="run on the given port", type=int)
define("debug", default=True, help="run in debug mode")


class TimerStatus(str, Enum):
     stopped = "stopped"
     running = "running"
     prepared = "prepared"


class SegmentStatus(object):
    def __init__(self):
        self.is_set = False
        self.start = datetime.now()
        self.end = datetime.now()
        self.shortest_duration = timedelta(seconds=0)
        self.shortest_time = timedelta(seconds=0)
        self.name = ""


class GroupStatus(object):
    def __init__(self):
        self.update_lock = tornado.locks.Condition()
        self.update_time = datetime.fromtimestamp(0)
        self.start_time = datetime.fromtimestamp(0)
        self.stop_time = datetime.fromtimestamp(0)
        self.status = TimerStatus.stopped
        self.segments : list[SegmentStatus] = []
        self.changeSemaphore = tornado.locks.Semaphore(1)
        self.update_id = -1

        def set_segment(self, segment_num, time, name): 
            is_new_segment = False
            if segment_num == len(self.segments):
                self.segments.append(SegmentStatus())
                is_new_segment = True

            segment = self.segments[segment_num]
            segment.is_set = True
            segment.name = name

            if is_new_segment or time > segment.end:
                segment.end = time

            if segment_num <  len(self.segments) - 1:
                adjust_segment = self.segments[segment_num + 1]
                if adjust_segment.is_set:
                    adjust_segment.start = self.segments[segment_num].end
                    shortest_duration = adjust_segment.end - adjust_segment.start
                    if shortest_duration < adjust_segment.shortest_duration:
                        adjust_segment.shortest_duration = shortest_duration

            if segment_num > 0:
                segment.start = self.segments[segment_num-1].end
            else:
                segment.start = self.start_time

            shortest_time : timedelta = segment.end - self.start_time
            shortest_duration : timedelta = segment.end - segment.start

            if is_new_segment or (shortest_time < segment.shortest_time):
                segment.shortest_time = shortest_time

            if is_new_segment or (shortest_duration < segment.shortest_duration):
                segment.shortest_duration = shortest_duration


class GroupStatusEncoder(json.JSONEncoder):
    def default(self, group):
        if isinstance(group, GroupStatus):
            json_dict = {
                "status": group.status,
                "start_time": group.start_time.isoformat(),
                "stop_time": group.stop_time.isoformat(),
                "update_id": group.update_id,
                "segments": []
            }

            for segment in group.segments:
                json_dict["segments"].append({
                    "is_set": segment.is_set,
                    "start": segment.start.isoformat(),
                    "end": segment.end.isoformat(),
                    "shortest_duration": segment.shortest_duration/timedelta(milliseconds=1),
                    "shortest_time": segment.shortest_time/timedelta(milliseconds=1),
                    "name": segment.name
                })

            return json_dict
        else:
            type_name = group.__class__.__name__
            raise TypeError("Unexpected type {0}".format(type_name))


global_groups : dict[str, GroupStatus] = {}


class RequestData(object):
    def __init__(self):
        self.time = datetime.fromtimestamp(0)
        self.segment_num = 0
        self.update_id = 0


class JsonHandler(tornado.web.RequestHandler):
    def set_default_headers(self, *args, **kwargs):
        self.set_header('Type', 'application/json')

    def prepare(self) -> None:
        if self.request.headers['Content-Type'].startswith('application/json'):
            body = self.request.body.decode("utf-8")
            args = tornado.escape.json_decode(body)
            self.args = RequestData()
            if 'time' in args.keys():
                self.args.time = datetime.fromisoformat(args['time'])
            if 'segment_num' in args.keys():
                self.args.segment_num = int(args['segment_num'])
            if 'update_id' in args.keys():
                self.args.update_id = int(args['update_id'])
            if 'name' in args.keys():
                self.args.name = str(args['name'])


class GroupModifyHandler(JsonHandler):
    async def prepare(self) -> None:
        super().prepare()
        self.group = GroupStatus()
        group_id = self.path_args[0]
        if group_id in global_groups:
            self.group = global_groups[group_id]
        else:
            logging.info(f'registering group {group_id}')
            global_groups[group_id] = self.group
        await self.group.changeSemaphore.acquire()

    def on_finish(self) -> None:
        self.group.update_time = datetime.utcnow()
        self.group.update_id = random.randint(1, 1000000)
        self.group.changeSemaphore.release()
        self.group.update_lock.notify_all()
        return super().on_finish()


class MainHandler(tornado.web.RequestHandler):
    def get(self):
        self.write({
            'app': 'arcdps-timer-server-v2',
            'version': 8
        })


class StartHandler(GroupModifyHandler):
    async def post(self, _):
        if self.group.status == TimerStatus.running:
            is_newer = self.group.start_time < self.args.time
            if is_newer:
                self.group.start_time = self.args.time
        else:
            self.group.status = TimerStatus.running
            self.group.start_time = self.args.time

            for segment in self.group.segments:
                segment.is_set = False


class StopHandler(GroupModifyHandler):
    async def post(self, _):
        if self.group.status in [TimerStatus.running, TimerStatus.prepared]:
            self.group.status = TimerStatus.stopped
            self.group.stop_time = self.args.time
        
            segments_unset = [i for i in range(len(self.group.segments)) if self.group.segments[i].is_set == False]

            segment_num = None
            if len(segments_unset) > 0:
                segment_num = segments_unset[0]
            else:
                segment_num = len(self.group.segments)

            self.group.set_segment(self.group, segment_num, self.args.time, "")


class ResetHandler(GroupModifyHandler):
    async def post(self, _):
        self.group.status = TimerStatus.stopped
        self.group.start_time = datetime.now()
        self.group.stop_time = datetime.now()

        for segment in self.group.segments:
            segment.is_set = False


class PrepareHandler(GroupModifyHandler):
    async def post(self, _):
        self.group.status = TimerStatus.prepared

        for segment in self.group.segments:
            segment.is_set = False


class StatusHandler(JsonHandler):
    async def prepare(self) -> None:
        super().prepare()
        self.group = GroupStatus()
        group_id = self.path_args[0]
        if group_id in global_groups:
            self.group = global_groups[group_id]
        else:
            global_groups[group_id] = self.group

    async def post(self, _):
        if self.group.update_id == self.args.update_id:
            self.update_future = self.group.update_lock.wait()
            try:
                await self.update_future
            except asyncio.CancelledError:
                return
        if self.request.connection.stream.closed():
            return
        self.write(json.dumps(self.group, cls=GroupStatusEncoder))

    def on_connection_close(self):
        self.update_future.cancel()


class SegmentHandler(GroupModifyHandler):
    async def post(self, _):
        logging.info(f"segment num: {self.args.segment_num}")
        self.group.set_segment(self.args.segment_num, self.args.time, self.args.name)


class ClearSegmentsHandler(GroupModifyHandler):
    async def post(self, _):
        self.group.segments.clear()


def cleanupGroups():
    global global_groups

    logging.info("Running cleanup")
    cleaned_global_groups : dict[str, GroupStatus] = {}

    for group_id, status in global_groups.items():
        if datetime.utcnow() - status.update_time < timedelta(days=1): 
            cleaned_global_groups[group_id] = status

    global_groups = cleaned_global_groups


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
