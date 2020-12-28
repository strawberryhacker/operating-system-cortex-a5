import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
sock.bind(("192.168.0.177", 117))
sock.sendto(b'this is a little test', ("255.255.255.255", 117))
