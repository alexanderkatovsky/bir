#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
# autoscan, aclocal, autoheader, automake --add-missing, autoconf

AC_INIT(sr, 1.0, apk32@cam.ac.uk)
AC_CONFIG_SRCDIR([arp_reply_waiting_list.c])
AC_CONFIG_HEADER([config.h])


AC_MSG_CHECKING(whether to enable optimization)
AC_ARG_ENABLE(release, [  --enable-release        Release build (default Debug build)], 
                      [optimize=yes], 
                      [optimize=no])
if test "x$optimize" = "xyes"; then
   AC_SUBST(DEBUG_C_FLAGS,"-D_N_DEBUG_ -O3")
   AC_MSG_RESULT(yes)
else
   AC_SUBST(DEBUG_C_FLAGS,"-g -D_DEBUG_ -O0")
   AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING(whether to build for CPU)
AC_ARG_ENABLE(CPU,      [  --enable-CPU            Build CPU binary (default VNS)], 
                      [build_cpu=yes], 
                      [build_cpu=no])

if test "x$build_cpu" = "xyes"; then
   AC_SUBST(CPU_C_FLAGS,"-D_CPUMODE_")
   AC_MSG_RESULT(yes)
else
   AC_SUBST(CPU_C_FLAGS,"-D_VNSMODE_")
   AC_MSG_RESULT(no)
fi

AC_LANG(C,C++)

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_YACC
AC_PROG_LEX
AC_PROG_RANLIB

AC_LANG_PUSH(C++)
# Checks for libraries.
have_wt=yes
AX_CXX_CHECK_LIB(wthttp, [Wt::WServer::start()],,[have_wt=no])
AC_CHECK_HEADERS([Wt/WApplication],,[have_wt=no])
AC_LANG_POP(C++)

AM_CONDITIONAL(TEST_HAVE_WT, [test "x$have_wt" = xyes])


# Checks for header files.
AC_FUNC_ALLOCA
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h inttypes.h libintl.h malloc.h netdb.h netinet/in.h stddef.h stdint.h stdlib.h string.h strings.h sys/ioctl.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT8_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_HEADER_TIME
AC_HEADER_STDBOOL
AC_TYPE_UID_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_C_VOLATILE


#run libtoolize
#AC_DISABLE_SHARED
#AC_PROG_LIBTOOL

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([bzero gethostbyname gethostname gettimeofday inet_ntoa memmove memset select socket strchr strstr strtol])

AM_INIT_AUTOMAKE(sr, 1.0)
AC_OUTPUT(Makefile cli/Makefile lwtcp/Makefile lwtcp/lwip/Makefile tvns/Makefile)


if test "x$have_wt" = "xno"; then
   echo ""
   echo "================================================"
   echo "     libwt has not been found.  "
   echo "     The HTTP Server cannot be built. "
   echo "     Install Wt from http://www.webtoolkit.eu/wt"
   echo "================================================"
   echo ""
fi