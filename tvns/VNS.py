"""Defines a simple VNS-like simulation."""

import struct

from twisted.internet import reactor

from VNSProtocol import VNS_DEFAULT_PORT, create_vns_server
from VNSProtocol import VNSOpen, VNSClose, VNSPacket, VNSInterface, VNSHardwareInfo

from IPMaker import IPMakerWithBase, IP, SubnetToString
import csv,array
import os,fcntl

zeromac = struct.pack('> 6B', 0xff, 0xff, 0xff, 0xff, 0xff, 0xff)

class Node:
    """A node in a topology"""
    def __init__(self, name, interfaces):
        self.name = name
        self.interfaces = interfaces
        for intf in self.interfaces:
            intf.owner = self

    def has_connection(self, _):
        return False

    def send_packet(self, departing_intf, packet):
        for intf in departing_intf.neighboring_interfaces:
            intf.owner.handle_packet(intf, packet)
                
class VirtualNode(Node):
    """A node which a user can take control of (i.e., handle packets for)"""
    def __init__(self, name, interfaces):
        Node.__init__(self, name, interfaces)
        self.conn = None  # connection to the virtual host, if any

    def connect(self, conn):
        if self.conn is not None:
            print 'Terminating the old control connection to %s - reconnected' % self.name
        self.conn = conn
        return True

    def has_connection(self, conn):
        return self.conn == conn

    def disconnect(self, conn):
        self.conn = None
        conn.loseConnection()
        print '%s is now free - client disconnected with VNSClose message' % self.name

    def handle_packet(self, intf, packet):
        """Forwards to the user responsible for handling packets for this virtual node"""
        #if (intf.mac == packet[0:6]) or (packet[0:6] == zeromac):
        if self.conn is not None:
            self.conn.send(VNSPacket(intf.name, packet))


def cksum(s):
    if len(s) & 1:
        s = s + '\0'
    words = array.array('h', s)
    sum = 0
    for word in words:
        sum = sum + (word & 0xffff)
    hi = sum >> 16
    lo = sum & 0xffff
    sum = hi + lo
    sum = sum + (sum >> 16)
    return (~sum) & 0xffff

from threading import Thread

class Host(Node):
    def __init__(self, name, interfaces):
        Node.__init__(self, name, interfaces)
        self.conn = None

    def connect(self, _):
        print 'Rejecting connection to %s - may not control a Host node' % self.name
        return False
        
    def handle_packet(self, intf, packet):
        """Replies to echo requests"""
        eth_type = struct.unpack('> H', packet[12:14])[0]
        if eth_type == 0x0800:
            ip_proto = struct.unpack('> B', packet[23:24])[0]
            if ip_proto == 1:
                icmp_type = struct.unpack('> B', packet[34:35])[0]
                if icmp_type == 8:
                    eth = packet[6:12] + packet[0:6] + packet[12:14]   # reverse MAC SA, DA
                    ip = packet[14:26] + packet[30:34] + packet[26:30] # reverse IP SA, DA
                    ip = ip[0:10] + struct.pack('> H', 0) + ip[12:]
                    ip = ip[0:10] + struct.pack('< H', cksum(ip)) + ip[12:]
                    icmp = struct.pack('> H', 0) + packet[31:]         # change to echo reply type
                    icmp = icmp[0:2] + struct.pack('> H', 0) + icmp[4:]
                    icmp = icmp[0:2] + struct.pack('< H', cksum(icmp)) + icmp[4:]
                    echo_reply = eth + ip + icmp
                    self.send_packet(intf, echo_reply)
        elif eth_type == 0x806:
            eth = packet[6:12] + intf.mac + packet[12:14]
            arp = packet[14:20] + struct.pack('> H',2) + \
                intf.mac + struct.pack('> I', intf.ip) + packet[6:12] + packet[28:32]
            self.send_packet(intf, eth + arp)

            
class TunHost(Node, Thread):
    """A tunnel to a linux network interface -- requires superuser privilages"""
    def __init__(self, name, interfaces):
        Thread.__init__(self)
        Node.__init__(self, name, interfaces)
        self.__fd = os.open("/dev/net/tun", os.O_RDWR)
        fcntl.ioctl(self.__fd, 0x400454ca, struct.pack("16sH", name, 2))
        self.conn = None
        if len(interfaces) > 0:
            import commands
            commands.getoutput("ifconfig %s up"%(name))
            commands.getoutput("ifconfig %s %s netmask %s"%(name, SubnetToString(interfaces[0].ip),
                                                            SubnetToString(interfaces[0].mask)))
            commands.getoutput("route add -net 192.168.0.0 netmask 255.255.0.0 dev %s"%name)
        self.start()

    def run(self):
        while 1:
            buf = os.read(self.__fd,1500)
            for intf in self.interfaces:
                self.send_packet(intf,buf[4:])

    def connect(self, _):
        print 'Rejecting connection to %s - may not control a Host node' % self.name
        return False

    def handle_packet(self,intf,packet):
        os.write(self.__fd, struct.pack("l",0) + packet);

            

class Hub(Node):
    """A hub"""
    def __init__(self, name, interfaces):
        Node.__init__(self, name, interfaces)
        self.conn = None

    def connect(self, _):
        print 'Rejecting connection to %s - may not control a Hub node' % self.name
        return False

    def handle_packet(self, incoming_intf, packet):
        """Forward each received packet to every interface except the one it was received on"""
        for intf in self.interfaces:
            if intf.name != incoming_intf.name:
                self.send_packet(intf, packet)

def make_ip(a):
    """Creates an IP from a string representation"""
    octets = a.split('.')
    return int(octets[0]) << 24 |\
           int(octets[1]) << 16 |\
           int(octets[2]) <<  8 |\
           int(octets[3])

def make_mac(i, intf_num):
    """Creates the MAC address 00:00:00:00:i:intf_num"""
    return struct.pack('> 6B', 0, 0, 0, 0, i, intf_num)

def MACToString(mac):
    return reduce(lambda x,y: x + ":" + "%02x"%y, struct.unpack("> 6B",mac), "")[1:]

class MACMaker:
    def __init__(self):
        self.__i = 0
        self.__j = 1
    def NextMAC(self):
        if self.__j > 255:
            self.__j = 0
            self.__i += 1
        if self.__i > 255:
            raise Exception("ran out of MAC addresses")
        ret = make_mac(self.__i, self.__j)
        self.__j += 1
        return ret

class TVNSNode:
    SERVER = 0
    ROUTER = 1
    TUNNEL = 2
    def __init__(self,n):
        self.__n = n
        self.__interfaces = []
        self.__type = self.SERVER
        self.__node = None
        if n[0] == 'r':
            self.__type = self.ROUTER
        if n[0] == 't':
            self.__type = self.TUNNEL
    def AddInterface(self,ip,mask,mac):
        eth = "eth%u"%len(self.__interfaces)
        ret = VNSInterface(eth,mac,ip,mask)
        self.__interfaces.append(ret)
        return ret
    def Node(self):
        if self.__node is None:
            if self.__type == self.ROUTER:
                self.__node = VirtualNode(self.__n, self.__interfaces)
            elif self.__type == self.TUNNEL:
                self.__node = TunHost(self.__n, self.__interfaces)
            else:
                self.__node = Host(self.__n, self.__interfaces)
        return self.__node
    def Type(self):
        return self.__type
            

class TVNSTopology:
    def __init__(self, top_file):
        masks = {}
        masks[0] = 0xffffffff
        n = 1
        for i in range(1,32):
            masks[i] = (masks[i-1] & ~n)
            n = (n << 1) + 1
        masks[32] = 0
        
        def connect_intfs(intf1, intf2):
            intf1.neighboring_interfaces.append(intf2)
            intf2.neighboring_interfaces.append(intf1)
            
        self.__hubs = []
        self.__nodes = {}
        self.__routers = {}
        self.__servers = {}
        self.nodes = []
        ipm = IPMakerWithBase(make_ip("192.168.0.0"),16)
        macm = MACMaker()
        for row in csv.reader(open(top_file)):
            intfs = []
            nintfs = len(row)
            ip  = ipm.NextIP(nintfs)
            i = 0
            for n in row:
                i += 1
                if n not in self.__nodes.keys():
                    nd = TVNSNode(n)
                    self.__nodes[n] = nd
                    if nd.Type() == TVNSNode.SERVER:
                        self.__servers[n] = nd
                    else:
                        self.__routers[n] = nd
                mac = macm.NextMAC()
                intfs.append(self.__nodes[n].AddInterface(ip.Subnet() + i, masks[ip.NBits()], mac))
            if nintfs > 2: 
                hub_interfaces = [VNSInterface('eth%u' % i, '000000', 0, 0) for i in range(0,nintfs)]
                hub = Hub('hub%u'%len(self.__hubs), hub_interfaces)
                self.__hubs.append(hub)
                for i in range(0,nintfs):
                    connect_intfs(hub_interfaces[i], intfs[i])
            elif len(intfs) == 2:
                connect_intfs(intfs[0], intfs[1])

    def Hubs(self):
        return self.__hubs
    def Nodes(self):
        return [n.Node() for n in self.__nodes.values()] + self.__hubs
    def Routers(self):
        return [n.Node() for n in self.__routers.values()]
    def Servers(self):
        return [n.Node() for n in self.__servers.values()]
     
class Topology:
    """Builds and stores a topology
    """
    def __init__(self,top_file):            
        self.tf = TVNSTopology(top_file)
        self.nodes = self.tf.Nodes()
        self.CreateHWFile()
        self.GenerateGVFile()
        
    def CreateHWFile(self):
        fd = open("topology.generated","w")
        for n in self.tf.Routers() + self.tf.Servers():
            fd.write("%s:\n"%n.name)
            for intf in n.interfaces:
                fd.write("  %s %s %s %s\n"%(intf.name,SubnetToString(intf.ip),
                                          SubnetToString(intf.mask), MACToString(intf.mac)))
            fd.write("\n")
        fd.close()
        
    def GenerateGVFile(self):
        fd = open("topology.gv","w")
        pre = """// compile with: neato topology.gv -Tpng > topology.png
graph G {
edge [len=2, fontsize=8, style=dotted];
node [shape=circle, width=.1, fontsize=10];
"""
        fd.write(pre)
        done = []
        for n in self.tf.Nodes():
            fd.write("%s [label=\"%s\"];\n"%(n.name,n.name))
        for n in self.tf.Routers() + self.tf.Servers() + self.tf.Hubs():
            for intf in n.interfaces:
                for n_intf in intf.neighboring_interfaces:
                    if n_intf not in self.tf.Hubs():
                        if (intf.ip, n_intf.ip) not in done:
                            fd.write("%s -- %s [headlabel=\"%s\", taillabel=\"%s\"];\n"
                                     %(intf.owner.name,n_intf.owner.name,SubnetToString(n_intf.ip)[8:],
                                       SubnetToString(intf.ip)[8:]))
                            done.append((intf.ip,n_intf.ip))
                            done.append((n_intf.ip,intf.ip))
        fd.write("}")
        fd.close()


                
class SimpleVNS:
    """Handles incoming messages from each client"""
    def __init__(self,top_file):
        self.topo = Topology(top_file)
        self.server = create_vns_server(VNS_DEFAULT_PORT, self.handle_recv_msg)

    def handle_recv_msg(self, conn, vns_msg):
        if vns_msg is not None:
            print 'recv: %s' % str(vns_msg)
            if vns_msg.get_type() == VNSOpen.get_type():
                self.handle_open_msg(conn, vns_msg)
            elif vns_msg.get_type() == VNSClose.get_type():
                self.handle_close_msg(conn)
            elif vns_msg.get_type() == VNSPacket.get_type():
                self.handle_packet_msg(conn, vns_msg)

    def handle_open_msg(self, conn, open_msg):
        requested_name = open_msg.vhost.replace('\x00', '')
        for n in self.topo.nodes:
            if n.name == requested_name:
                if n.connect(conn):
                    conn.send(VNSHardwareInfo(n.interfaces))
                else:
                    conn.loseConnection()  # failed to connect
                return


        # failed to find the requested node
        print 'unknown node name requested: %s' % requested_name
        conn.loseConnection()

    def handle_close_msg(self, conn):
        for n in self.topo.nodes:
            if n.has_connection(conn):
                n.disconnect()
                return

    def handle_packet_msg(self, conn, pkt_msg):
        for n in self.topo.nodes:
            if n.conn == conn:
                departure_intf_name = pkt_msg.intf_name.replace('\x00', '')
                for intf in n.interfaces:
                    if intf.name == departure_intf_name:
                        n.send_packet(intf, pkt_msg.ethernet_frame)
                        return

                # failed to find the specified interface
                print 'bad packet request on node %s: invalid interface: %s' % (n.name, departure_intf_name)
                return

        # failed to find the specified connection?!?
        print 'Unable to find the node associated with this connection??  Disconnecting it: %s' % str(conn)
        conn.loseConnection()


def main():
    import sys
    if len(sys.argv) < 2:
        top_file = "tvns.top"
    else:
        top_file = sys.argv[1]
    SimpleVNS(top_file)
    reactor.run()

if __name__ == "__main__":
    main()
