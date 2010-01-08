import socket,sys
from construct import *

def dict2obj(d):
    class Dummy:
        pass
    c = Dummy
    for elem in d.keys():
        c.__dict__[elem] = d[elem]
    return c    

class RCPsocket:
    def __init__(self, sock=None):
        if sock is None:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        else:
            self.sock = sock
    def connect(self, host, port):
        self.sock.connect((host, port))
    def __send(self, data):
        totalsent = 0
        while totalsent < len(data):
            sent = self.sock.send(data[totalsent:])
            if sent == 0:
                raise RuntimeError, "socket connection broken"
            totalsent = totalsent + sent        
    def send(self, code, data):
        self.__send(ULInt32("int").build(code))
        self.__send(ULInt32("int").build(len(data)))
        if len(data) > 0: 
            self.__send(data);
    def __receive(self,datalen):
        msg = ''
        while len(msg) < datalen:
            chunk = self.sock.recv(datalen-len(msg))
            if chunk == '':
                raise RuntimeError, "socket connection broken"
            msg = msg + chunk
        return msg
    def receive(self):
        code    = ULInt32("int").parse(self.__receive(4))
        datalen = ULInt32("int").parse(self.__receive(4))
        data    = self.__receive(datalen)
        return (code,data)

class IP:
    @staticmethod
    def ToString(ip):
        return repr(int((ip >> 24) & 0xff)) + "." +\
            repr(int((ip >> 16) & 0xff)) + "." +\
            repr(int((ip >> 8) & 0xff)) + "." +\
            repr(int((ip) & 0xff))
    
    @staticmethod
    def FromString(ip):
        octets = a.split('.')
        return int(octets[0]) << 24 |\
            int(octets[1]) << 16 |\
            int(octets[2]) <<  8 |\
            int(octets[3])

class ForwardingTable:
    entry = Struct("ForwardingTableEntry",
                   UBInt32("subnet"),
                   UBInt32("mask"),
                   UBInt32("next_hop"),
                   CString("interface"))
    struct = OptionalGreedyRepeater(entry)
    def __conv_ip(self,el,m):
        for e in el:
            e.subnet    = m(e.subnet)
            e.mask      = m(e.mask)
            e.next_hop  = m(e.next_hop)
        return el
    def build(self,obj):
        return ForwardingTable.struct.build(self.__conv_ip(map(dict2obj, obj), IP.FromString))
    def parse(self,obj):
        ret = self.__conv_ip(ForwardingTable.struct.parse(obj), IP.ToString)
        return map(lambda x: x.__dict__, ret)
        

class RouterEx(Exception):
    def __init__(self, msg):
        self.__msg = msg
    def __repr__(self):
        return "Router Exception: " + self.__msg

class Router:
    RCP_CODE_ERROR                   =  0x0
    RCP_CODE_SUCCESS                 =  0x1
    RCP_CODE_GET_ROUTER_ID           =  0x10
    RCP_CODE_GET_FORWARDING_TABLE    =  0x11
    def __init__(self, host, port):
        self.__sock = RCPsocket()
        self.__sock.connect(host,port)
        
    def __result(self, scode, rcode, rawdata, c):
        if rcode == scode:
            return c.parse(rawdata)
        elif rcode == self.RCP_CODE_ERROR:
            raise RouterEx(CString("").parse(rawdata))
        else:
            raise RouterEx("Unexpected response; sent code %x, expected %x"%(scode,rcode))

    def __get(self, scode, c):
        self.__sock.send(scode, [])
        rcode, rawdata = self.__sock.receive()
        return self.__result(scode, rcode, rawdata, c)
        
    def GetRouterID(self):
        return self.__get(self.RCP_CODE_GET_ROUTER_ID, UBInt32(""))

    def GetForwardingTable(self):
        return self.__get(self.RCP_CODE_GET_FORWARDING_TABLE, ForwardingTable())

port = 3333
if len(sys.argv) > 1:
    port = int(sys.argv[1])
        
router = Router("127.0.0.1",port)
try:
    print IP.ToString(router.GetRouterID())
    for e in router.GetForwardingTable():
        print e
except RouterEx, e:
    print repr(e)
