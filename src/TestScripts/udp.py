import socket
import time
import math 

UDP_IP = "127.0.0.1"
UDP_PORT = 31000
MESSAGE = b""

print("UDP target IP: %s" % UDP_IP)
print("UDP target port: %s" % UDP_PORT)

sock = socket.socket(socket.AF_INET, # Internet
                     socket.SOCK_DGRAM) # UDP

while(True):
   t = time. time()
   y = math.sin(t)
   msg = str(y)
   print("msg: %s" % msg)
   sock.sendto(bytearray(msg, 'ascii'), (UDP_IP, UDP_PORT))
   time.sleep(0.1)
   