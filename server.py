import random
import string
import asyncio
from aiohttp import web

rooms={}
pub_keys={}
online={}

async def handle(request):
    name = request.match_info.get('name', "Anonymous")
    text = "Hello, " + name
    return web.Response(body=text.encode('utf-8'))

async def wshandler(request):
    print('new connection')
    ws = web.WebSocketResponse()
    await ws.prepare(request)
    pub_key=""
    async for msg in ws:
        if msg.tp == web.MsgType.text:
            print('request:'+msg.data)
            arr = msg.data.split()
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
                            online[ww].send_str(msg.data);
                else:
                    ws.send_str('ERRORMESSAGE '+arr[1])                    
            elif arr[0] == 'PUBKEY':
                pub_key=arr[1]
                online[pub_key]=ws
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
