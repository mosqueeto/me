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
# libtermcap: sudo apt install libncurses5-dev

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
#
LIBS= -lc -ltermcap  -lpcre
#LIBS= -lc -lncurses
#
#for cray 
#CFLAGS=		O2 -D_CRAY_
#
#for k2
#CC = gcc
#LIBS= -ltermcap -lc


OFILES=		basic.o bf.o buffer.o crypt_buf.o display.o file.o  \
		keys.o line.o main.o md5.o random.o region.o \
		search.o spawn.o tcap.o termio.o window.o word.o 

CFILES=		basic.c bf.c buffer.c crypt_buf.c display.c file.c  \
		keys.c line.c main.c md5.o random.c region.c \
		search.c spawn.c tcap.c termio.c window.c word.c

HFILES=	 ed.h search.h crypt.h

me:	$(OFILES)
		$(CC) $(CFLAGS) -L. $(OFILES) $(LIBS) -o me

efme:	$(OFILES)
		$(CC) $(CFLAGS) $(OFILES) -lefence $(LIBS) -o efme

crypt: crypt.o bf.o crypt.h ed.h
		$(CC) $(CFLAGS) crypt.o bf.o -o crypt

k2:	me
	cp me /g/g1/kent/bin/me; \
	rm -f /g/g1/kent/bin/v; \
	ln /g/g1/kent/bin/me /g/g1/kent/bin/v

songbird:	me
	cp me /usr/local/bin; \
	rm -f /usr/local/bin/v; \
	ln -f /usr/local/bin/me /usr/local/bin/v

pecos: me
	cp me /home/kent/bin/me; \
	rm -f /home/kent/bin/v; \
	ln -f /home/kent/bin/me /home/kent/bin/v

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

ftp:	tar
	cp me.tar /home/ftp/pub/me.tar
	
	

$(OFILES):	$(HFILES)
