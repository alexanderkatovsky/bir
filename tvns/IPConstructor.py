from construct import *

ethernet_header = Struct(
    "ethernet_header",
    Bytes("dest",6),
    Bytes("src",6),
    Enum(
        UBInt16("type"),
        IP = 0x800,
        ARP = 0x806))

ip_header = Struct(
    "ip_header",
    Byte('vh'),
    Byte("tos"),
    UBInt16("len"),
    UBInt16("id"),
    UBInt16("offset"),
    Byte("ttl"),
    Enum(
        Byte("protocol"),
        ICMP = 1),
    UBInt16("cksum"),
    UBInt32("ip_src"),
    UBInt32("ip_dst"))

arp_header = Struct(
    "arp_header",
    UBInt16("htype"),
    UBInt16("ptype"),
    Byte("hlen"),
    Byte("plen"),
    UBInt16("code"),
    Bytes("src_mac",6),
    UBInt32("src_ip"),
    Bytes("dst_mac",6),
    UBInt32("dst_ip"))

icmp_header = Struct(
    "icmp_header",
    Byte("type"),
    Byte("code"),
    UBInt16("cksum"))

ip_packet = Struct(
    "ip_packet",
    ip_header,
    icmp_header)

eth_packet = Struct(
    "arp_packet",
    ethernet_header,
    Switch("next", lambda ctx: ctx["ethernet_header"]["type"],
           {"IP" : ip_packet, "ARP" : arp_header}),
    OptionalGreedyRepeater(UBInt8("data")))


