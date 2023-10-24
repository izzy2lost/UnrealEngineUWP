import io
import os
import re
import base64
import random
import socket
import asyncio
import threading
from flask import request, Flask, Response

os.chdir("c:/")

#-------------------------------------------------------------------------------
async def proxy_impl(loop, client):
    msg = b""
    while not msg.endswith(b"\r\n\r\n"):
        msg += await loop.sock_recv(client, 2048)

    mode = "="
    min_trunc = 0
    disconnect = False
    stall = False
    tamper = 0

    line = next(iter(io.BytesIO(msg)))
    if b"?disconnect HTTP" in line:
        mode = "D"
        disconnect = True

    if b"?stall HTTP" in line:
        mode = "S"
        disconnect = True
        stall = True
        min_trunc = len(msg) + 16

    if m := re.search(b"\?tamper=(\d+) HTTP", line):
        mode = "T"
        tamper = int(m.group(1))
        tamper = tamper / 100.0

    send_time = 1.0 + (random.random() * 0.5)
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as httpd:
        httpd.connect(("127.0.0.1", 9493))
        await loop.sock_sendall(httpd, msg)
        data = bytes()
        while x := await loop.sock_recv(httpd, 128 << 10):
            data += x

        data_size = len(data)

        if disconnect:
            trunc = max(int(data_size * random.random()), min_trunc)
            if trunc >= data_size:
                trunc = data_size - 1
            print("trun:", data_size, "->", trunc)
            data = data[:trunc]

        if tamper:
            data = bytearray(data)
            for i in range(len(data)):
                c = data[i] if random.random() > tamper else (int(random.random() * 0x4567) & 0xff)
                data[i] = c

        try:
            while data:
                percent = 0.02 + (random.random() * 0.08)
                send_size = max(int(data_size * percent), 1)
                print("%s snd:%5d tot:%5d slp:%5f %%:%5f" % (mode, send_size, data_size, send_time * percent, percent), end="\r")
                await loop.sock_sendall(client, data[:send_size])
                data = data[send_size:]
                if not tamper:
                    await asyncio.sleep(send_time * percent)
        except ConnectionError:
            pass
        print("")


        if stall:
            print("...stalling", end="")
            await asyncio.sleep(2)
            print("")

    client.close()

#-------------------------------------------------------------------------------
async def proxy_loop():
    loop = asyncio.get_event_loop()
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("", 9494))
        sock.listen()
        sock.setblocking(True)
        while True:
            client, address = await loop.sock_accept(sock)
            loop.create_task(proxy_impl(loop, client))

#-------------------------------------------------------------------------------
def proxy():
    asyncio.run(proxy_loop())

thread = threading.Thread(target=proxy)
thread.start()



#-------------------------------------------------------------------------------
app = Flask(__name__)
payload_data = random.randbytes(2 << 20)

#-------------------------------------------------------------------------------
@app.route("/test")
def test():
    return "<html><head>test-server<title></title></head><body>hello world</body></html>"

#-------------------------------------------------------------------------------
@app.route("/seed/<int:value>")
def seed(value):
    random.seed(value)
    return { "_" : "ok" }

#-------------------------------------------------------------------------------
@app.route("/data", methods=("GET", "POST"))
@app.route("/data/<int:payload_size>", methods=("GET", "POST"))
@app.route("/data/[a-zA-Z]+", methods=("GET", "POST"))
def get(payload_size=-1):
    if request.method == "POST":
        abort(400)

    if payload_size <= 0:
        payload_size = int(random.random() * (8 << 10)) + 16

    payload_size = min(payload_size, len(payload_data))
    payload = payload_data[-payload_size:]

    payload_hash = 0x493
    for c in payload:
        payload_hash = ((payload_hash + c) * 0x493) & 0xffffffff

    mega_headers = {}
    mega_size = int(random.random() * (4 << 10))
    for i in range(1024):
        key = "X-MegaHeader-%04d" % i
        value = payload_data[:int(random.random() * 64)]
        value = base64.b64encode(value)
        mega_headers[key] = value.decode()

        mega_size -= len(key) + len(value) + 4
        if mega_size <= 0:
            break

    return (payload, {
        "X-TestServer-Hash" : payload_hash,
        **mega_headers,
        "Content-Type" : "application/octet-stream",
    })

app.run(host="0.0.0.0", port=9493)

# vim: et
