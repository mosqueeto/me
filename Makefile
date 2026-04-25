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
# Dependencies (dynamic build):
#   sudo apt install libncurses5-dev libpcre3-dev
#
# Dependencies (static build):
#   sudo apt install libncurses-dev libpcre3-dev
#   provides: /usr/lib/x86_64-linux-gnu/libtinfo.a
#             /usr/lib/x86_64-linux-gnu/libpcre.a

PLATFORM=linux
#PLATFORM=bsd
CC = cc
# don't know how to make a static binary any more
#CFLAGS=		-g -static
CFLAGS= -g -Wno-pedantic -fPIC -Wno-implicit
#LIBS= -lefence -ltermcap -lc -lncurses 
#LIBS= -ltermcap -lc -lncurses -lpcre
#
#for mac mini
#LIBS= -ltermcap -lc -lncurses
#sudo apt install libncurses5-dev
#sudo apt install libpcre3 libpcre3-dev
#
LIBS= -lc /usr/lib/x86_64-linux-gnu/libtinfo.so.6 -lpcre
#LIBS= -lc -lncurses


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
		    /usr/lib/x86_64-linux-gnu/libpcre.a \
		    -Wl,--allow-multiple-definition \
		    -o me.static

efme:	$(OFILES)
		$(CC) $(CFLAGS) $(OFILES) -lefence $(LIBS) -o efme

crypt: crypt.o bf.o crypt_buf.o md5.o crypt.h ed.h
		$(CC) $(CFLAGS) crypt.o bf.o crypt_buf.o md5.o -o crypt

songbird:	me
	sudo cp me /bin; \
	sudo rm -f /bin/v; \
	sudo ln -f /bin/me /bin/v

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
