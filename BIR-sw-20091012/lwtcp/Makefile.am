
LWTCP_C_SRCS = tcp.c tcp_input.c tcp_output.c \
             mem.c memp.c stats.c sys.c \
             inet.c pbuf.c sys_arch.c \
             sockets.c api_lib.c api_msg.c \
             transport_subsys.c udp.c icmp.c ip_addr.c \
             err.c

LWTCP_H_SRCS = lwtcp_sr_integration.h
LWTCP_SRCS = $(LWTCP_H_SRCS) $(LWTCP_C_SRCS)
#LWTCP_SRCS = $(patsubst %,lwtcp/%,$(LWTCP_C_SRCS) $(LWTCP_H_SRCS))

LWTCP_OBJS = $(patsubst %.c, %.o, $(LWTCP_SRCS))
liblwtcp.a : $(LWTCP_OBJS)
	ar rcu liblwtcp.a $(LWTCP_OBJS)

bin_PROGRAMS=liblwtcp.a
