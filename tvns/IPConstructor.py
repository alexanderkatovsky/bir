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
        ICMP = 1,
        TCP  = 6),
    UBInt16("cksum"),
    UBInt32("ip_src"),
    UBInt32("ip_dst"))

ip_pseudo_header = Struct(
    "ip_pseudo_header",
    UBInt32("src_ip"),
    UBInt32("dst_ip"),
    Byte("reserved"),
    Byte("protocol"),
    UBInt16("len"))

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

tcp_header = Struct(
    "tcp_header",
    UBInt16("src_port"),
    UBInt16("dst_port"),
    UBInt32("seq_num"),
    UBInt32("ack_num"),
    BitStruct("bs",
              Bits("offset",4),
              Bits("reserved",3),
              Bits("ecn",3),
              Flag("URG"),
              Flag("ACK"),
              Flag("PSH"),
              Flag("RST"),
              Flag("SYN"),
              Flag("FIN")),
    UBInt16("window"),
    UBInt16("cksum"),
    UBInt16("ubp"))

ip_packet = Struct(
    "ip_packet",
    ip_header,
    Switch("next", lambda ctx: ctx["ip_header"]["protocol"],
           {"ICMP" : icmp_header, "TCP" : tcp_header}))

eth_packet = Struct(
    "arp_packet",
    ethernet_header,
    Switch("next", lambda ctx: ctx["ethernet_header"]["type"],
           {"IP" : ip_packet, "ARP" : arp_header}),
    OptionalGreedyRepeater(UBInt8("data")))


