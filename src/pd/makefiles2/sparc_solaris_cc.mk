# Makefile with Genernal Options
#
# CVS Info : $Id: sparc_solaris_cc.mk 74060 2016-01-11 06:57:34Z gyeongsuk.eom $
#

# ���޵Ǵ� �ܺ� ������

# ID_DIR      : ID ���丮
# ID_ACE_ROOT : ���̺귯�� �н�
# compile64   : ������ ȯ��
# compat5     : CC 5.0 ����?
# compat4     :

# BUILD_MODE�� ����
#   debug		: Debug ���
#   prerelease      	: -DDEBUG(x) -g (o)
#   release		: release ����, ���� product�� �ش�
ifndef	BUILD_MODE
	@echo "ERROR BUILD_MODE!!!!"
	@exit
endif

.SUFFIXES: .cpp .$(OBJEXT) .a

CXX	= /opt/SUNWspro/bin/CC
CC	= /opt/SUNWspro/bin/cc
CXXLINT	= /usr/local/parasoft/bin.solaris/codewizard

CXXOPT_DEPENDANCY = -xM1

LD	= $(CXX)

AR	= ar
ARFLAGS	= -ruv

NM 	= /usr/ccs/bin/nm
NMFLAGS	= -t x

PURIFY		= purify -chain-length=100
QUANTIFY	= quantify
PURECOV		= purecov  -max_threads=512 -follow-child-processes
PURIFYCOV   = $(PURIFY) $(PURECOV)

# readline library ����
ifeq "$(USE_READLINE)" "1"
  READLINE_INCLUDES = -I/usr/local/include/readline
  READLINE_LIBRARY =  -lreadline -ltermcap
endif # use readline library

# LIBS_SHIP added : 20030811
# shared object�� ���� ����ϱ� ���� ������ static �ɼ���
# ����ϸ�, runtime������ �߻��Ѵ�. �̸� �����ϱ� ����
# Ŭ���̾�Ʈ�� ��ũ �ɼ��� ������ �����, cli.script���� ����ϵ��� ��.

ifeq "$(OS_MAJORVER).$(OS_MINORVER)" "2.5"
ifeq ($(compile64),1) # 64bit
LIBS = -xnolib -Bdynamic -lthread -lposix4 -ldl -lkvm -lkstat -lsocket -lintl -lnsl -lgen  -lm -lw -lc -Bstatic -liostream -lCrun
LIBS_ODBC = -xnolib -Bdynamic -lthread -lposix4 -ldl -lkvm -lkstat -lsocket -lintl -lnsl -lgen  -lm -lw -Bstatic -liostream -lCrun
LIBS_SHIP = -lthread -lposix4 -ldl -lkvm -lkstat  -lsocket -lnsl -lgen
else # 32bit release
ifeq ($(compat5),1)
LIBS = -xnolib -Bdynamic -lthread -lposix4 -ldl -lkvm -lelf -lkstat  -Bstatic -lsocket -lnsl -lintl -lgen -liostream -lCrun -lm -lw -Bdynamic -lthread -Bstatic -lcx -lc
LIBS_ODBC = -xnolib -Bdynamic -lthread -lposix4 -ldl -lkvm -lelf -lkstat  -Bstatic -lsocket -lnsl -lintl -lgen -liostream -lCrun -lm -lw
LIBS_SHIP = -lthread -lposix4 -ldl -lkvm -lkstat  -lsocket -lnsl -lgen
else # if compat4
LIBS = -xnolib -Bdynamic -lthread -lposix4 -ldl -lkvm -lkstat  -Bstatic  -lsocket -lintl -lnsl -lgen -lC -lm -lw -lcx -lc
LIBS_ODBC= -xnolib -Bdynamic -lthread -lposix4 -ldl -lkvm -lkstat  -Bstatic  -lsocket -lintl -lnsl -lgen -lC -lm -lw
LIBS_SHIP = -lthread -lposix4 -ldl -lkvm -lkstat  -lsocket -lnsl -lgen
endif # compat4
endif # compile64
else # "$(OS_MAJORVER).$(OS_MINORVER)" "2.5"
ifeq ($(compile64),1) # 64bit
LIBS = -xnolib -Bdynamic -lthread -lposix4 -ldl -lkvm -lkstat -lsocket -lnsl -lgen  -lm -lw -lc -Bstatic -liostream -lCrun -ldemangle
LIBS_ODBC = -xnolib -Bdynamic -lthread -lposix4 -ldl -lkvm -lkstat -lsocket -lnsl -lgen  -lm -lw -Bstatic -liostream -lCrun
LIBS_SHIP = -lthread -lposix4 -ldl -lkvm -lkstat  -lsocket -lnsl -lgen -ldemangle
else # 32bit release
ifeq ($(compat5),1)
LIBS = -xnolib -Bdynamic -lthread -lposix4 -ldl -lkvm -lkstat -lsocket -lnsl -lgen  -lm -lw -lc -Bstatic -liostream -lCrun -ldemangle
LIBS_ODBC = -xnolib -Bdynamic -lthread -lposix4 -ldl -lkvm -lkstat -lsocket -lnsl -lgen  -lm -lw -Bstatic -liostream -lCrun
LIBS_SHIP = -lthread -lposix4 -ldl -lkvm -lkstat  -lsocket -lnsl -lgen -ldemangle
else # if compat4
LIBS = -xnolib -Bdynamic -lthread -lposix4 -ldl -lkvm -lkstat  -Bstatic -lsocket -lnsl -lgen -lC -lm -lw -lcx -lc -ldemangle
LIBS_ODBC = -xnolib -Bdynamic -lthread -lposix4 -ldl -lkvm -lkstat  -Bstatic -lsocket -lnsl -lgen -lC -lm -lw
LIBS_SHIP = -lthread -lposix4 -ldl -lkvm -lkstat  -lsocket -lnsl -lgen -ldemangle
endif # compat4
endif # compile64
endif # "$(OS_MAJORVER).$(OS_MINORVER)" "2.5"

ifeq ($(sb),1)
  SOURCE_BROWSER = -xsb
endif

# ���� ������ �ɼ� ����
#
# compile mode �� ����
#	compat4		:
#       compat5		:
#	compile64	:
#
CLASSIC_LIB = -library=iostream,no%Cstd
LIB64_DIRS  = -L/opt/SUNWspro/SC5.0/lib/v9 -L/usr/lib/sparcv9
LIB32_DIRS  = -L/opt/SUNWspro/SC5.0/lib -L/usr/lib/lwp/32 -L/usr/lib

ODBC_LIBS = $(LIBS_ODBC)

# BUG-32589
# Fixing the position of -fast prior to any platform options
LFLAGS =
ifeq "$(BUILD_MODE)" "debug"
ifeq ($(compile64),1)
  LFLAGS += -xarch=v9
else
  LFLAGS += -xarch=v8plusa
endif
else
ifeq "$(BUILD_MODE)" "prerelease"
ifeq ($(compile64),1)
  LFLAGS += -fast -xprefetch=yes -xarch=v9
else
  LFLAGS += -fast -xprefetch=yes -xarch=v8plusa
endif
else
ifeq "$(BUILD_MODE)" "release"
ifeq ($(compile64),1)
  LFLAGS += -fast -xprefetch=yes -xarch=v9
else
  LFLAGS += -fast -xprefetch=yes -xarch=v8plusa
endif
else
error:
	@echo "ERROR!!!! UNKNOWN BUILD_MODE($(BUILD_MODE))";
	@exit;
endif	# release
endif   # prerelease
endif	# debug

#64bit ������ ���
ifeq ($(compile64),1)
  LFLAGS += -mt -xarch=v9 $(CLASSIC_LIB) $(LIB64_DIRS)
else
#32��Ʈ compat5 ���
ifeq ($(compat5),1)
  LFLAGS += -mt $(CLASSIC_LIB) $(LIB32_DIRS)
else
#32��Ʈ compat4 ���
  LFLAGS += -mt -compat=4
endif # 64 bit + compat=5
endif # 64 bit mode



# LINK MODE ����
#	purify		: purify version
#	quantify	: quantify version
#	purecov		: purecov version
#	purifycov   : purifycov version
############### LINK MODE #####################################3
ifeq "$(LINK_MODE)" "normal"
LD		:=  $(LD)
else
ifeq "$(LINK_MODE)" "purify"
LD		:= $(PURIFY) $(LD)
else
ifeq "$(LINK_MODE)" "quantify"
LD		:= $(QUANTIFY) $(LD)
else
ifeq "$(LINK_MODE)" "purecov"
LD		:= $(PURECOV) $(LD)
else
ifeq "$(LINK_MODE)" "purifycov"
LD		:= $(PURIFYCOV) $(LD)
else
error:
	@echo "ERROR!!!! UNKNOWN LINK_MODE($(LINK_MODE))";
	@exit;
endif	# purifycov
endif	# purecov
endif	# quantify
endif	# purify
endif	# normal



####
#### common rules
####
EFLAGS	= -E $(CCFLAGS)
SFLAGS	= -S $(CCFLAGS)

#############################
#  Choose Altibase Build    # 
#############################

# some platform like windows don;t have enough shell buffer for linking. 
# so use indirection file for linking.
NEED_INDIRECTION_BUILD = no

# some platform like aix 4.3 don't need to build shared library 
NEED_SHARED_LIBRARY = yes

# some platform like suse linux/windows have a problem for using libedit.
NEED_BUILD_LIBEDIT = yes

# some platform like a windows don;t have to build jdbc 
NEED_BUILD_JDBC = yes

#############################
# TASK-6469 SET MD5
##############################

CHECKSUM_MD5 = digest -a md5 -v
