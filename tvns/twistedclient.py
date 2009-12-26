import ltprotocol
from VNSProtocol import VNSOpen, VNS_PROTOCOL, VNSInterface, VNSPacket
import VNS
from HTTPHandler import HTTPHandler

class Client:
    def __init__(self):
        self.__interface = None
        self.__mac = None
        self.__ip  = None
        self.__mask = None
        self.__http = HTTPHandler(self.send)

    def send(self,what):
        print "send " + repr(what)
        self.__prot.send(VNSPacket(self.__interface, what))
        
    def callback(self, prot, msg):
        self.__prot = prot
        if self.__interface is None:
            for m in msg:
                for i in range(0,len(m)):
        #print dict(zip(m[::2],m[1::2]))
                    if m[i] == VNSInterface.HWINTERFACE:
                        self.__interface = m[i+1]
                        print "interface: " + self.__interface
                        i += 1
                    elif m[i] == VNSInterface.HWSPEED:
                        i += 2
                    elif m[i] == VNSInterface.HWETHER:
                        self.__mac = m[i + 1][0:6]
                        print "MAC      : " + VNS.MACToString(self.__mac)
                        i += 1
                    elif m[i] == VNSInterface.HWETHIP:
                        self.__ip = m[i + 1]
                        print "ip       : " + VNS.SubnetToString(self.__ip)
                        i += 2
                    elif m[i] == VNSInterface.HWSUBNET:
                        i += 2
                    elif m[i] == VNSInterface.HWMASK:
                        self.__mask = m[i + 1]
                        print "mask     : " + VNS.SubnetToString(self.__mask)
        else:
            print msg
            self.__http.process_packet(self.__ip, self.__mac, msg.ethernet_frame)

v = 'r2'

import sys

if len(sys.argv) > 0:
    v = sys.argv[1]

def conncallback(prot):
    print "connect callback"
    prot.send(VNSOpen(0,v,'',''))

client = Client()
c = ltprotocol.LTTwistedClient(VNS_PROTOCOL, client.callback, conncallback)
c.connect('',12345)

