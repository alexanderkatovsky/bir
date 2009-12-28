AUTOMAKE_OPTIONS = foreign
SUBDIRS = cli lwtcp .

SR_C_SRCS = sr_base.c sr_dumper.c sr_integration.c sr_lwtcp_glue.c \
               sr_vns.c sr_cpu_extension_nf2.c real_socket_helper.c sr_rt.c \
               debug.c interface_list.c router.c assoc_array.c bi_assoc_array.c arp.c checksum.c ip.c \
               forwarding_table.c arp_cache.c arp_reply_waiting_list.c fifo.c icmp_handle.c ospf.c \
               neighbour_list.c mutex.c link_state_graph.c linked_list.c RCP.c RCPHandleRequest.c srtcp.c nat.c \
dhcp.c

SR_H_SRCS = arp_cache.h debug.h linked_list.h nf2.h real_socket_helper.h sr_cpu_extension_nf2.h sr_vns.h \
arp_reply_waiting_list.h  eth_headers.h link_state_graph.h nf2util.h reg_defines.h sr_dumper.h vnscommand.h \
assoc_array.h fifo.h mutex.h pwospf_protocol.h router.h sr_ifsys.h bi_assoc_array.h forwarding_table.h  nat.h \
RCPCodes.h sr_base.h sr_integration.h common.h interface_list.h neighbour_list.h RCP.h sr_base_internal.h sr_rt.h \
dhcp.h

SR_SRCS = $(SR_H_SRCS) $(SR_C_SRCS)

SR_SRCS_MAIN = sr_main.c
SR_SRCS_BASE = nf2util.c

SR_OBJS = $(patsubst %.c, %.o, $(SR_SRCS))
libsr.a : $(SR_OBJS)
	ar rcu libsr.a $(SR_OBJS)

AM_CFLAGS=-Wall -D_GNU_SOURCE -I lwtcp -I cli -D_LINUX_ 
AM_LDFLAGS=-lpthread #-z muldefs
bin_PROGRAMS=sr
sr_SOURCES=$(SR_SRCS_MAIN) $(SR_SRCS_BASE)
sr_LDADD=cli/libcli.a lwtcp/liblwtcp.a libsr.a

