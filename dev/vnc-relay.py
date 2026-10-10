#!/usr/bin/env python3
"""dev/vnc-relay.py <listen port> <server port> — one VNC connection, from a viewer to Qt's VNC server,
with the one message Qt needs and a viewer may never send.

Qt 6.10's VNC platform (qvncclient.cpp) converts every pixel through the client's pixel format, which
it only ever learns from a SetPixelFormat message: it has no default. A viewer that is happy with the
server's own format (TigerVNC is) never sends one, and Qt then converts through an uninitialised
format and crashes ("Cannot handle 3080244 bpp client", then SIGSEGV or bad_alloc). This relays the
RFB 3.3 handshake unchanged and, straight after the server's ServerInit, sends the server a
SetPixelFormat holding the server's own format; then it copies both directions until either closes.
A SetPixelFormat the viewer sends later still goes through and wins.

Both ends are on 127.0.0.1 (dev/vip-vnc runs it in front of an ssh tunnel). It serves one connection
and exits.
"""
import os
import socket
import struct
import sys
import threading


def recv_exactly(sock, n):
    data = b""
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            raise ConnectionError("closed during the handshake")
        data += chunk
    return data


class ClientLog:
    """With VNC_RELAY_DEBUG=1, the viewer's input messages, one line each, on stderr."""

    SIZES = {0: 20, 3: 10, 4: 8, 5: 6}

    def __init__(self):
        self.buf = b""

    def feed(self, data):
        self.buf += data
        while self.buf:
            t = self.buf[0]
            if t == 2:
                if len(self.buf) < 4:
                    return
                n = 4 + 4 * struct.unpack(">H", self.buf[2:4])[0]
            elif t == 6:
                if len(self.buf) < 8:
                    return
                n = 8 + struct.unpack(">I", self.buf[4:8])[0]
            elif t in self.SIZES:
                n = self.SIZES[t]
            else:
                print(f"vnc-relay: viewer message type {t}?", file=sys.stderr, flush=True)
                self.buf = b""
                return
            if len(self.buf) < n:
                return
            msg, self.buf = self.buf[:n], self.buf[n:]
            if t == 5:
                print("vnc-relay: pointer buttons=%d at %d,%d" % struct.unpack(">BHH", msg[1:6]), file=sys.stderr, flush=True)
            elif t == 4:
                print("vnc-relay: key %s 0x%x" % ("down" if msg[1] else "up", struct.unpack(">I", msg[4:8])[0]), file=sys.stderr, flush=True)


def pump(src, dst, log=None):
    try:
        while True:
            data = src.recv(65536)
            if not data:
                break
            if log:
                log.feed(data)
            dst.sendall(data)
    except OSError:
        pass
    finally:
        for s in (src, dst):
            try:
                s.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass


def main():
    listen_port, server_port = int(sys.argv[1]), int(sys.argv[2])
    listener = socket.socket()
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", listen_port))
    listener.listen(1)
    print("ready", flush=True)
    viewer, _ = listener.accept()
    listener.close()
    server = socket.create_connection(("127.0.0.1", server_port))

    # ProtocolVersion both ways, then the server's security (Qt: RFB 3.3, a u32 type, 1 = none),
    # the viewer's ClientInit (the shared flag), and the server's ServerInit.
    viewer.sendall(recv_exactly(server, 12))
    server.sendall(recv_exactly(viewer, 12))
    security = recv_exactly(server, 4)
    viewer.sendall(security)
    if struct.unpack(">I", security)[0] != 1:
        raise SystemExit("vnc-relay: the server asks for authentication; only Qt's (none) is handled")
    server.sendall(recv_exactly(viewer, 1))
    head = recv_exactly(server, 24)  # width, height, the 16-byte pixel format, the name's length
    name = recv_exactly(server, struct.unpack(">I", head[20:24])[0])
    viewer.sendall(head + name)
    # SetPixelFormat: type 0, three bytes of padding, the format — the server's own.
    server.sendall(b"\x00\x00\x00\x00" + head[4:20])

    t = threading.Thread(target=pump, args=(server, viewer), daemon=True)
    t.start()
    pump(viewer, server, ClientLog() if os.environ.get("VNC_RELAY_DEBUG") else None)
    t.join(timeout=2)


if __name__ == "__main__":
    main()
