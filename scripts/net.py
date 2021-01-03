import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
sock.bind(("192.168.5.177", 50))
sock.sendto(b'hello world', ("255.255.255.255", 50))
