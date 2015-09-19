LIB_NAME = akdtree
LIB_VERSION_MAJOR = 0
LIB_VERSION_MINOR = 1
LIB_VERSION = $(LIB_VERSION_MAJOR).$(LIB_VERSION_MINOR)

LTOFLAGS = -flto
OPTFLAGS = -O2 -pipe -mtune=generic
WARNFLAGS = -Wall -Wextra -Wconversion
HARDFLAGS = -Wp,-D_FORTIFY_SOURCE=2 -fstack-protector-strong --param=ssp-buffer-size=4
DBGFLAGS = -g -grecord-gcc-switches
FLAGS = $(DBGFLAGS) $(WARNFLAGS) $(OPTFLAGS) $(HARDFLAGS) $(LTOFLAGS) -fvisibility=hidden

HDRS = akdtree.h
SRCS = akdtree.c
OBJS = $(SRCS:.c=.o)

lib$(LIB_NAME).so.$(LIB_VERSION): $(OBJS) $(HDRS)
	$(CC) -shared -Wl,-soname,lib$(LIB_NAME).so.$(LIB_VERSION) \
		$(FLAGS) -o lib$(LIB_NAME).so.$(LIB_VERSION) $(OBJS)

%.o: %.c
	$(CC) -c -fpic -g $(FLAGS) $<

TAGS: $(SRCS)
	ctags -e -R .
