if TEST_HAVE_WT
WTCF = -DHAVE_WT
WTLF = -lwt -lwthttp -lwtext
WTSRCS = server/server.cc
endif


AUTOMAKE_OPTIONS = foreign
SUBDIRS = cli lwtcp .
AM_CFLAGS= $(WTCF)  -Wall -D_GNU_SOURCE -I lwtcp -I cli -D_LINUX_ $(DEBUG_C_FLAGS) $(CPU_C_FLAGS)
AM_CXXFLAGS= $(AM_CFLAGS) -DCPP $(DEBUG_C_FLAGS)
AM_LDFLAGS= $(WTLF) $(DEBUG_C_FLAGS) -lpthread #-z muldefs

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

lib_LIBRARIES=libsr.a
libsr_a_SOURCES=$(SR_SRCS)

bin_PROGRAMS=sr
sr_SOURCES=$(WTSRCS) $(SR_SRCS_MAIN) $(SR_SRCS_BASE)
sr_LDADD=$(WTLIBS) cli/libcli.a lwtcp/liblwtcp.a libsr.a

