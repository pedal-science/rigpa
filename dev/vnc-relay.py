#!/usr/bin/env python3
"""dev/vnc-relay.py <listen port> <server port> — VNC from a Mac viewer to Qt's VNC server, with what
each side needs and the other doesn't give.

Qt 6.10's VNC platform (qvncclient.cpp) converts every pixel through the client's pixel format, which
it only ever learns from a SetPixelFormat message: it has no default. A viewer that is happy with the
server's own format never sends one, and Qt then converts through an uninitialised format and crashes
("Cannot handle 3080244 bpp client", then SIGSEGV or bad_alloc). So straight after the server's
ServerInit, this sends the server a SetPixelFormat holding the server's own format; then it copies
both directions until either closes. A SetPixelFormat the viewer sends later still goes through.

Qt's server speaks RFB 3.3 with no authentication; macOS Screen Sharing won't connect to a server that
asks for no password. So the viewer is offered RFB 3.8 with VNC authentication, and any answer is
accepted (dev/vip-vnc passes a password in the vnc:// URL, so nothing is asked): both ends are on
127.0.0.1, and the real fence is on vip (the server takes connections from its own localhost only,
which this reaches through ssh). The server is answered in 3.3. From ClientInit on, the two versions
are the same protocol.

It serves connections until none has been open for VNC_RELAY_IDLE seconds (default 3) after the first
(a viewer may connect more than once: a probe, then the session), then exits: dev/vip-vnc waits for
that.
"""
import os
import socket
import struct
import sys
import threading
import time

IDLE = float(os.environ.get("VNC_RELAY_IDLE", "3"))
DEBUG = bool(os.environ.get("VNC_RELAY_DEBUG"))


def log(msg):
    print(f"vnc-relay: {msg}", file=sys.stderr, flush=True)


def recv_exactly(sock, n):
    data = b""
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            raise ConnectionError("closed during the handshake")
        data += chunk
    return data


class ClientLog:
    """With VNC_RELAY_DEBUG=1, the viewer's messages, one line each, on stderr."""

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
                log(f"viewer message type {t}, {self.buf[:16].hex()}")
                self.buf = b""
                return
            if len(self.buf) < n:
                return
            msg, self.buf = self.buf[:n], self.buf[n:]
            if t == 5:
                log("pointer buttons=%d at %d,%d" % struct.unpack(">BHH", msg[1:6]))
            elif t == 4:
                log("key %s 0x%x" % ("down" if msg[1] else "up", struct.unpack(">I", msg[4:8])[0]))
            else:
                log(f"message type {t}, {n} bytes")


def pump(src, dst, client_log=None):
    try:
        while True:
            data = src.recv(65536)
            if not data:
                break
            if client_log:
                client_log.feed(data)
            dst.sendall(data)
    except OSError:
        pass
    finally:
        for s in (src, dst):
            try:
                s.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass


def handshake(viewer, server):
    # The server: RFB 3.3, whose security is a u32 the server chooses (Qt: 1, none).
    version = recv_exactly(server, 12)
    if not version.startswith(b"RFB 003.003"):
        raise ConnectionError(f"expected Qt's RFB 3.3 server, got {version!r}")
    server.sendall(b"RFB 003.003\n")
    if struct.unpack(">I", recv_exactly(server, 4))[0] != 1:
        raise ConnectionError("the server asks for authentication; only Qt's (none) is handled")

    # The viewer: RFB 3.8 (Screen Sharing answers 3.889, its own 3.8), VNC authentication.
    viewer.sendall(b"RFB 003.008\n")
    answer = recv_exactly(viewer, 12)
    if DEBUG:
        log(f"viewer speaks {answer!r}")
    minor = int(answer[8:11]) if answer[:8] == b"RFB 003." and answer[8:11].isdigit() else 3
    if minor >= 7:
        viewer.sendall(b"\x01\x02")                  # one security type: VNC authentication
        if recv_exactly(viewer, 1) != b"\x02":
            raise ConnectionError("the viewer chose a security type that wasn't offered")
    else:
        viewer.sendall(struct.pack(">I", 2))        # 3.3: the server says VNC authentication
    viewer.sendall(os.urandom(16))                  # the challenge
    recv_exactly(viewer, 16)                        # its answer: accepted, whatever it is
    viewer.sendall(struct.pack(">I", 0))            # SecurityResult: OK

    # The viewer's ClientInit (the shared flag) to the server, the server's ServerInit back.
    server.sendall(recv_exactly(viewer, 1))
    head = recv_exactly(server, 24)  # width, height, the 16-byte pixel format, the name's length
    name = recv_exactly(server, struct.unpack(">I", head[20:24])[0])
    viewer.sendall(head + name)
    # SetPixelFormat: type 0, three bytes of padding, the format — the server's own.
    server.sendall(b"\x00\x00\x00\x00" + head[4:20])


def serve(viewer, server_port, active):
    try:
        server = socket.create_connection(("127.0.0.1", server_port))
        try:
            handshake(viewer, server)
        except (ConnectionError, OSError) as e:
            if DEBUG:
                log(f"a connection ended in its handshake: {e}")
            server.close()
            return
        t = threading.Thread(target=pump, args=(server, viewer), daemon=True)
        t.start()
        pump(viewer, server, ClientLog() if DEBUG else None)
        t.join(timeout=2)
    finally:
        viewer.close()
        with active["lock"]:
            active["open"] -= 1
            active["last"] = time.monotonic()


def main():
    listen_port, server_port = int(sys.argv[1]), int(sys.argv[2])
    listener = socket.socket()
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", listen_port))
    listener.listen(4)
    listener.settimeout(0.5)
    print("ready", flush=True)
    active = {"lock": threading.Lock(), "open": 0, "seen": False, "last": 0.0}
    while True:
        try:
            viewer, _ = listener.accept()
        except socket.timeout:
            with active["lock"]:
                if active["seen"] and active["open"] == 0 and time.monotonic() - active["last"] > IDLE:
                    return
            continue
        with active["lock"]:
            active["open"] += 1
            active["seen"] = True
        threading.Thread(target=serve, args=(viewer, server_port, active), daemon=True).start()


if __name__ == "__main__":
    main()
