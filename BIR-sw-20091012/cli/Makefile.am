BUILT_SOURCES=lex.yy.c y.tab.h y.tab.c 
AM_CFLAGS=-Wall -D_GNU_SOURCE  -I ../lwtcp -I .. -I . -D_LINUX_ $(DEBUG_C_FLAGS)

lex.yy.c : cli_scanner.lex
	$(LEX) -d cli_scanner.lex

y.tab.h y.tab.c : cli.h cli_help.h cli_scanner.h cli_parser.h helper.h cli_parser.y
	$(YACC) --debug -d -y cli_parser.y 

CLI_LEX_SRCS = $(BUILT_SOURCES) cli_parser.h cli_scanner.h

CLI_H_SRCS = cli.h  cli_help.h  cli_local.h  cli_main.h  cli_network.h  cli_parser.h  \
cli_scanner.h cli_stubs.h  helper.h  search_state.h  socket_helper.h

CLI_C_SRCS = cli.c  cli_help.c  cli_local.c  cli_main.c  cli_test.c  helper.c \
search_state.c  socket_helper.c  

CLI_SRCS = $(CLI_C_SRCS) $(CLI_H_SRCS) $(CLI_LEX_SRCS)

#lib_LTLIBRARIES=libcli.la
#libcli_la_SOURCES=$(CLI_SRCS)

lib_LIBRARIES=libcli.a
libcli_a_SOURCES=$(CLI_SRCS)

EXTRA_DIST=cli_parser.y cli_scanner.lex

clean-local : 
	-rm -rf $(BUILT_SOURCES) 
