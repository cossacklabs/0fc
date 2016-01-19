import random
import string
import asyncio
import base64
from pythemis import ssession;
from aiohttp import web

server_private_key = "UkVDMgAAAC1whm6SAJ7vIP18Kq5QXgLd413DMjnb6Z5jAeiRgUeekMqMC0+x"

class transport(ssession.mem_transport):        #necessary callback
    def get_pub_key_by_id(self, user_id):
        return base64.b64decode(user_id.decode("utf8")); 

rooms={}
pub_keys={}
online={}

history={}

class transport(ssession.mem_transport):        #necessary callback
    def get_pub_key_by_id(self, user_id):
        if user_id != b"client":                        #we have only one peer with id "client"
            raise Exception("no such id");
        return client_pub; 

async def handle(request):
    name = request.match_info.get('name', "Anonymous")
    text = "Hello, " + name
    return web.Response(body=text.encode('utf-8'))

async def wshandler(request):
    print('new connection')
    ws = web.WebSocketResponse()
    await ws.prepare(request)
    pub_key=""
    async for mesg in ws:
        #session = ssession.ssession(server_private_key, base64.b64decode(server_private_key), transport())
        if mesg.tp == web.MsgType.text:
            #msg = session.unwrap(base64.b64decode(mesg.data))
            #if msg.is_control:
            #    ws.send_str(base64.b64encode(msg));
            #else:
                msg = mwsg.data
                print('request:'+msg)
                arr = msg.split()
                if arr[0] == "NEWROOM" and pub_key != "":
                    new_room_id=''.join([random.choice(string.ascii_letters + string.digits) for n in range(32)])
                    rooms[new_room_id]={'owner':pub_key, 'users':[pub_key]}
                    ws.send_str('NEWROOM '+new_room_id)
                elif arr[0] == "INVITE" and pub_key != "":
                    if arr[2] in rooms and arr[3] == rooms[arr[2]]['owner'] and arr[3] in online:
                        online[arr[3]].send_str(msg.data)
                    else:
                        ws.send_str('INVALIDINVITE '+arr[1])
                elif arr[0] == "INVITE_RESP" and pub_key != "":
                    if arr[1] in online and pub_key == rooms[arr[2]]['owner']:
                        rooms[arr[2]]['users'].append(arr[1])
                        online[arr[1]].send_str(msg.data)
                elif arr[0] == "OPENROOM" and pub_key != "":
                    if arr[1] in rooms and pub_key in rooms[arr[1]]['users']:
                        ws.send_str('OPENROOM 111')
                    else:
                        ws.send_str('ERRORROOM '+arr[1])
                elif arr[0] == "MESSAGE" and pub_key != "":
                    if arr[1] in rooms and pub_key in rooms[arr[1]]['users']:
                        for ww in rooms[arr[1]]['users']:
                            if ww in online and ww!=pub_key:
                                online[ww].send_str(msg);
                            else:
                                if ww in history:
                                    history[ww].append(msg)
                                else
                                    history[ww]=[msg]
                    else:
                        ws.send_str('ERRORMESSAGE '+arr[1])
                elif arr[0] == 'PUBKEY':
                    pub_key=arr[1]
                    online[pub_key]=ws
                    for mm in history[pub_key]:
                        ws.send_str(mm)
                    history[pub_key]=[]
                else:
                    ws.send_str('ERROR malformed request');
        elif msg.tp == web.MsgType.close:
            print('connection closed')
        else:
            ws.send_str('ERROR malformed request')
            print('error:'+msg.data)
    return ws

async def init(loop):
    app = web.Application(loop=loop)
    app.router.add_route('GET', '/', handle)
    app.router.add_route('GET', '/ws', wshandler)

    srv = await loop.create_server(app.make_handler(), '0.0.0.0', 8888)
    print("Server started at http://127.0.0.1:8888")
    return srv

loop = asyncio.get_event_loop()
loop.run_until_complete(init(loop))
loop.run_forever()
