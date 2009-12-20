import IPConstructor,socket
import array
import time

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


def dict2obj(d):
    class Dummy:
        pass
    c = Dummy
    for elem in d.keys():
        c.__dict__[elem] = d[elem]
    return c    

def calc_tcp_cksum(iph,tcph,data):
    tcph.cksum = 0
    ph = IPConstructor.ip_pseudo_header.build(dict2obj({
                "src_ip"   : iph.ip_src,
                "dst_ip"   : iph.ip_dst,
                "reserved" : 0,
                "protocol" : 6,
                "len"      : len(IPConstructor.tcp_header.build(tcph)) + len(data)
                }))
    return cksum(ph + IPConstructor.tcp_header.build(tcph) + "".join(map(chr,data)))

class HTTPHandler:
    '''
    A very stripped down BaseHTTPRequestHandler for use without sockets;
    raw ethernet packets are passed to process_packet, which
    responds to ARP requests, echo requests, and handles TCP packets,
    passing the resulting ethernet packet not to a file stream but to a
    specified function (send)
    '''
    def __init__(self, send):
        self.__send = send
    
    def process_packet(self, intf_ip, intf_mac, packet):
        """Replies to echo requests, arp requests, also TCP handshake, and simple HTTP Server"""
        def swap_src_dest(pkt):
            src = pkt.next.ip_header.ip_dst
            pkt.next.ip_header.ip_dst = pkt.next.ip_header.ip_src
            pkt.next.ip_header.ip_src = src
            src = pkt.ethernet_header.dest
            pkt.ethernet_header.dest = pkt.ethernet_header.src
            pkt.ethernet_header.src = src

        def swap_port(pkt):
            src = pkt.next.next.src_port
            pkt.next.next.src_port = pkt.next.next.dst_port
            pkt.next.next.dst_port = src

        def ip_ck(pkt):
            pkt.next.ip_header.cksum = 0
            ethsize = len(IPConstructor.ethernet_header.build(pkt.ethernet_header))
            ipsize = len(IPConstructor.ip_header.build(pkt.next.ip_header))
            pkt.next.ip_header.cksum = socket.htons(cksum(IPConstructor.eth_packet.build(pkt)[ethsize:ethsize+ipsize]))

        def icmp_ck(pkt):
            pkt.next.next.cksum = 0
            ethsize = len(IPConstructor.ethernet_header.build(pkt.ethernet_header))
            ipsize = len(IPConstructor.ip_header.build(pkt.next.ip_header))
            pkt.next.next.cksum = socket.htons(cksum(IPConstructor.eth_packet.build(pkt)[ethsize + ipsize:]))

        def tcp_ck(pkt):
            res = socket.htons(calc_tcp_cksum(pkt.next.ip_header,pkt.next.next,pkt.data))
            pkt.next.next.cksum = res
            
        try:
            pkt = IPConstructor.eth_packet.parse(packet)
            if pkt.ethernet_header.type == 'ARP':
                print "ARP"
                pkt.ethernet_header.dest = pkt.ethernet_header.src
                pkt.ethernet_header.src = intf_mac
                pkt.next.dst_ip = pkt.next.src_ip
                pkt.next.src_ip = intf_ip
                pkt.next.dst_mac = pkt.next.src_mac
                pkt.next.src_mac = intf_mac
                pkt.next.code = 2
                self.__send(IPConstructor.eth_packet.build(pkt))
            elif pkt.ethernet_header.type == 'IP':
                if pkt.next.ip_header.protocol == 'ICMP':
                    pkt.next.next.type = 0
                    swap_src_dest(pkt)
                    icmp_ck(pkt)
                    ip_ck(pkt)
                    self.__send(IPConstructor.eth_packet.build(pkt))
                elif pkt.next.ip_header.protocol == 'TCP':
                    start_data = 4 * pkt.next.next.bs.offset - len(IPConstructor.tcp_header.build(pkt.next.next))
                    message = "".join(map(chr,pkt.data[start_data:]))
                    message = message.split('\n')
                    swap_src_dest(pkt)
                    swap_port(pkt)
                    if pkt.next.next.bs.SYN:                        
                        pkt.next.next.bs.ACK = True
                        pkt.next.next.ack_num = pkt.next.next.seq_num + 1
                        pkt.next.next.seq_num = 10000
                        tcp_ck(pkt)
                        ip_ck(pkt)
                        self.__send(IPConstructor.eth_packet.build(pkt))
                    elif pkt.next.next.bs.ACK and message[0][0:3] == 'GET':
                        content = "Hello World.\n"
                        headers = '''HTTP/1.1 200 OK
Date: %s
Content-Type: text/plain
Content-Length: %d
Connection: close
Accept-Ranges: bytes

%s'''%(time.strftime("%a, %d %b %Y %H:%M:%S GMT",time.gmtime()),len(content),content)
                        seq = pkt.next.next.seq_num
                        pkt.next.next.seq_num = pkt.next.next.ack_num
                        pkt.next.next.ack_num = seq + len(pkt.data[start_data:])
                        pkt.next.next.bs.PSH = False
                        pkt.next.ip_header.len = len(IPConstructor.eth_packet.build(pkt))
                        tcp_ck(pkt)
                        ip_ck(pkt)
                        self.__send(IPConstructor.eth_packet.build(pkt))
                        pkt.data[start_data:] = map(ord,headers)
                        pkt.next.next.bs.PSH = True
                        pkt.next.ip_header.len = len(IPConstructor.eth_packet.build(pkt)) - 14
                        tcp_ck(pkt)
                        ip_ck(pkt)
                        self.__send(IPConstructor.eth_packet.build(pkt))
                        
        except IPConstructor.ConstructError, e:
            pass

