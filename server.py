import argparse
import logging
import random
import string
import asyncio
import base64

from aiohttp import web

from pythemis import ssession


class Transport(ssession.mem_transport):  # necessary callback
    def get_pub_key_by_id(self, user_id):
        return base64.b64decode(user_id.decode("utf8"))


class COMMAND:
    NEW_ROOM = 'NEWROOM'
    OPEN_ROOM = 'OPENROOM'
    ERROR_ROOM = 'ERRORROOM'
    INVITE = 'INVITE'
    INVITE_RESPONSE = 'INVITE_RESP'
    INVALID_INVITE = 'INVALIDINVITE'
    MESSAGE = 'MESSAGE'
    MESSAGE_ERROR = 'ERRORMESSAGE'
    PUBLIC_KEY = 'PUBKEY'
    ERROR = 'ERROR'


id_symbols = string.ascii_letters + string.digits
def generate_id():
    return ''.join([random.choice(id_symbols) for _ in range(32)])


@asyncio.coroutine
def handle(request):
    name = request.match_info.get('name', "Anonymous")
    text = "Hello, {}".format(name)
    return web.Response(body=text.encode('utf-8'))


@asyncio.coroutine
def wshandler(request):
    logger.info('new connection')
    ws_response = web.WebSocketResponse()
    yield from ws_response.prepare(request)
    pub_key = ""
    while True:
        message = yield from ws_response.receive()
        # session = ssession.ssession(server_private_key, base64.b64decode(server_private_key), transport())
        if message.tp == web.MsgType.text:
            # msg = session.unwrap(base64.b64decode(mesg.data))
            # if msg.is_control:
            #    ws.send_str(base64.b64encode(msg));
            # else:
            msg = message.data
            logger.info('request:' + msg)
            message_params = msg.split()
            if message_params[0] == COMMAND.NEW_ROOM and pub_key:
                new_room_id = generate_id()
                rooms[new_room_id] = {'owner': pub_key, 'users': [pub_key]}
                ws_response.send_str('{} {}'.format(COMMAND.NEW_ROOM, new_room_id))
            elif message_params[0] == COMMAND.INVITE and pub_key:
                if (message_params[2] in rooms and message_params[3] == rooms[message_params[2]]['owner'] and
                            message_params[3] in online):
                    online[message_params[3]].send_str(msg)
                else:
                    ws_response.send_str('{} {}'.format(COMMAND.INVALID_INVITE, message_params[1]))
            elif message_params[0] == COMMAND.INVITE_RESPONSE and pub_key:
                if message_params[1] in online and pub_key == rooms[message_params[2]]['owner']:
                    rooms[message_params[2]]['users'].append(message_params[1])
                    online[message_params[1]].send_str(msg)
            elif message_params[0] == COMMAND.OPEN_ROOM and pub_key:
                if message_params[1] in rooms and pub_key in rooms[message_params[1]]['users']:
                    ws_response.send_str('{} 111'.format(COMMAND.OPEN_ROOM))
                else:
                    ws_response.send_str('{} {}'.format(COMMAND.ERROR_ROOM, message_params[1]))
            elif message_params[0] == COMMAND.MESSAGE and pub_key:
                if message_params[1] in rooms and pub_key in rooms[message_params[1]]['users']:
                    for user_public_key in rooms[message_params[1]]['users']:
                        if user_public_key in online and user_public_key != pub_key:
                            online[user_public_key].send_str(msg)
                        else:
                            if user_public_key in history:
                                history[user_public_key].append(msg)
                            else:
                                history[user_public_key] = [msg]
                else:
                    ws_response.send_str('{} {}'.format(COMMAND.MESSAGE_ERROR, message_params[1]))
            elif message_params[0] == COMMAND.PUBLIC_KEY:
                pub_key = message_params[1]
                online[pub_key] = ws_response
                if pub_key in history:
                    for history_message in history[pub_key]:
                        ws_response.send_str(history_message)
                    history[pub_key] = []
            else:
                ws_response.send_str('{} malformed request'.format(COMMAND.ERROR))
        elif message.tp == web.MsgType.closed  or message.tp == web.MsgType.close:
           if pub_key in online:
               del online[pub_key]
           logger.info('connection closed')
           break;
        else:
            ws_response.send_str('{} malformed request'.format(COMMAND.ERROR))
            if 'msg' in locals():
                logger.info('error:' + msg)
    if pub_key in online:
        del online[pub_key]
    logger.info('closed')
    return ws_response


@asyncio.coroutine
def init(port, loop):
    app = web.Application(loop=loop)
    app.router.add_route('GET', '/', handle)
    app.router.add_route('GET', '/ws', wshandler)
    handler = app.make_handler()
    srv = yield from loop.create_server(handler, '0.0.0.0', port)
    logger.info("Server started at http://0.0.0.0:{}".format(port))
    return handler, app, srv


@asyncio.coroutine
def finish(app, srv, handler):
    global online
    for sockets in online.values():
        for socket in sockets:
            socket.close()

    yield from asyncio.sleep(0.1)
    srv.close()
    yield from handler.finish_connections()
    yield from srv.wait_closed()


if __name__ == '__main__':

    parser = argparse.ArgumentParser(
        description='Run server')

    parser.add_argument('-p', '--port', type=int, help='Port number', default=8888)
    parser.add_argument('-v', '--verbose', action='store_true', help='Output verbosity')
    args = parser.parse_args()
    port = args.port

    logging.basicConfig(level=logging.INFO if args.verbose else logging.WARNING)
    logger = logging.getLogger(__name__)

    server_private_key = "UkVDMgAAAC1whm6SAJ7vIP18Kq5QXgLd413DMjnb6Z5jAeiRgUeekMqMC0+x"
    rooms = {}
    pub_keys = {}
    online = {}
    history = {}

    loop = asyncio.get_event_loop()
    handler, app, srv = loop.run_until_complete(init(port, loop))
    try:
        loop.run_forever()
    except KeyboardInterrupt:
        loop.run_until_complete(finish(app, srv, handler))
