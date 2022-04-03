import tornado.ioloop
import tornado.web

from tornado.options import define, options, parse_command_line

define("port", default=5000, help="run on the given port", type=int)
define("debug", default=True, help="run in debug mode")


class MainHandler(tornado.web.RequestHandler):
    def get(self):
        self.write({
            'app': 'arcdps-timer-server-v2',
            'version': 7
        })


class StartHandler(tornado.web.RequestHandler):
    def post(self, group_id):
        pass


class StopHandler(tornado.web.RequestHandler):
    def post(self, group_id):
        pass


class ResetHandler(tornado.web.RequestHandler):
    def post(self, group_id):
        pass


class PrepareHandler(tornado.web.RequestHandler):
    def post(self, group_id):
        pass


class StatusHandler(tornado.web.RequestHandler):
    async def post(self, group_id):
        pass

    def on_connection_close(self):
        pass


def main():
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
    tornado.ioloop.IOLoop.current().start()


if __name__ == "__main__":
    main()