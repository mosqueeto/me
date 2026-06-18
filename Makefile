#Build your code with -Wall -Werror (or your compiler's equivalent). Once you 
#clean up all the crud, that pops up, crank it up with -W 
#-Wno-unused-parameter -Wstrict-prototypes -Wmissing-prototypes 
#-Wpointer-arith. Once there -- add -Wreturn-type -Wcast-qual -Wswitch 
#-Wshadow -Wcast-align and tighten up by removing the no in 
#-Wno-unused-parameter. The -Wwrite-strings is essential, if you wish your 
#code to be compiled with a C++ compiler some day (hint: the correct type for 
#static strings is " const char *").
#
#For truly clean code, add -Wchar-subscripts -Winline -Wnested-externs 
#-Wredundant-decls. 
#
# Dependencies -- PCRE2 (default, Ubuntu 22.04+):
#   sudo apt install libncurses5-dev libpcre2-dev
# Dependencies -- PCRE1 (legacy, USE_PCRE=1):
#   sudo apt install libncurses5-dev libpcre3-dev
# for arch: sudo pacman -S ncurses pcre2

PLATFORM=linux
#PLATFORM=bsd
CC = cc

# Set USE_PCRE=1 to build against legacy libpcre (PCRE1); default is PCRE2.
USE_PCRE ?= 2
ifeq ($(USE_PCRE),2)
PCRE_CFLAGS = -DUSE_PCRE2
PCRE_LIBS   = -lpcre2-8
PCRE_STATIC = /usr/lib/x86_64-linux-gnu/libpcre2-8.a
else
PCRE_CFLAGS =
PCRE_LIBS   = -lpcre
PCRE_STATIC = /usr/lib/x86_64-linux-gnu/libpcre.a
endif

# don't know how to make a static binary any more
#CFLAGS=		-g -static
CFLAGS= -g -Wno-pedantic -fPIC -Wno-implicit $(PCRE_CFLAGS)
#LIBS= -lefence -ltermcap -lc -lncurses
LIBS= -lncurses $(PCRE_LIBS)
#for mac mini
#LIBS= -ltermcap -lc -lncurses


OFILES=		basic.o bf.o buffer.o crypt_buf.o display.o file.o  \
		init.o keys.o line.o main.o md5.o mouse.o random.o region.o \
		search.o spawn.o tcap.o termio.o window.o word.o

CFILES=		basic.c bf.c buffer.c crypt_buf.c display.c file.c  \
		init.c keys.c line.c main.c md5.o random.c region.c \
		search.c spawn.c tcap.c termio.c window.c word.c

HFILES=	 ed.h search.h crypt.h

me:	$(OFILES)
		$(CC) $(CFLAGS) -L. $(OFILES) $(LIBS) -o me

me.static: $(OFILES)
		$(CC) -static $(CFLAGS) -L. $(OFILES) -lc \
		    /usr/lib/x86_64-linux-gnu/libtinfo.a \
		    $(PCRE_STATIC) \
		    -Wl,--allow-multiple-definition \
		    -o me.static

efme:	$(OFILES)
		$(CC) $(CFLAGS) $(OFILES) -lefence $(LIBS) -o efme

mcrypt: mcrypt.o bf.o crypt_buf.o md5.o crypt.h ed.h
		$(CC) $(CFLAGS) crypt.o bf.o crypt_buf.o md5.o -o mcrypt

songbird:	me
	sudo cp me /bin; \
	sudo rm -f /bin/v; \
	sudo ln -f /bin/me /bin/v; \
	sudo cp me.1 /usr/share/man/man1/me.1

clean:
	rm -f *.o core ,,*

backup:
	mkdir .old >& /dev/null || true ; \
	if [ -e me.tar ] ; then \
            mv me.tar .old/me.tar.`date +%Y%m%d%H%M` ; \
        fi

#tar:	clean backup
tar:	clean
	rm -f me efme ; \
	tar cf me.tar *.c *.h Makefile

	

$(OFILES):	$(HFILES)
