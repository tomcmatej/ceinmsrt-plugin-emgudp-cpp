import socket
import time
import struct

UDP_PORT = 31000

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind(('', UDP_PORT))

data = [0.1, 0.2, 0.3, 0.4]
#https://stackoverflow.com/questions/48867997/sending-data-using-socket-from-pythonclient-to-cserver-arrays
#https://docs.python.org/3/library/struct.html
byteData = struct.pack("dddd", *data)

dataRec, addrRec = sock.recvfrom(1024)

if int(dataRec[0]) != 4:
	print( "ERROR: Number of Channel needed different from 2.")

print("Data is being send: IP:%s" % str(addrRec))

while True:
	sock.sendto(byteData, addrRec)
	print(byteData[0])
	time.sleep(0.01) #10 ms