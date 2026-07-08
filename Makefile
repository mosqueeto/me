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
# Dependencies:
#   * ncurses         -- terminal handling
#   * PCRE2 (or PCRE1 with USE_PCRE=1) -- regex search/replace
#   * OpenSSL libcrypto -- AES-256-GCM + scrypt for the #ME2.00$ encrypted
#     file format (crypt_buf.c).  Legacy #ME1.42$ files decrypt without it,
#     but the link still requires -lcrypto; it is not optional at build time.
#
# Install the dev packages:
#   Debian/Ubuntu (PCRE2, default):
#     sudo apt install libncurses5-dev libpcre2-dev libssl-dev
#   Debian/Ubuntu (PCRE1, build with USE_PCRE=1):
#     sudo apt install libncurses5-dev libpcre3-dev libssl-dev
#   Arch:
#     sudo pacman -S ncurses pcre2 openssl
#   Fedora/RHEL:
#     sudo dnf install ncurses-devel pcre2-devel openssl-devel

PLATFORM=linux
#PLATFORM=bsd
CC = cc

# Set USE_PCRE=1 to build against legacy libpcre (PCRE1); default is PCRE2.
# The *_STATIC vars auto-detect the static archive across common distro layouts
# (Arch /usr/lib, Debian /usr/lib/<triplet>, Fedora /usr/lib64); override on the
# command line if yours lives elsewhere.
USE_PCRE ?= 2
ifeq ($(USE_PCRE),2)
PCRE_CFLAGS = -DUSE_PCRE2
PCRE_LIBS   = -lpcre2-8
PCRE_STATIC = $(firstword $(wildcard /usr/lib/libpcre2-8.a /usr/lib/x86_64-linux-gnu/libpcre2-8.a /usr/lib64/libpcre2-8.a))
else
PCRE_CFLAGS =
PCRE_LIBS   = -lpcre
PCRE_STATIC = $(firstword $(wildcard /usr/lib/libpcre.a /usr/lib/x86_64-linux-gnu/libpcre.a /usr/lib64/libpcre.a))
endif

# Encryption: CRYPT_S=1 (default) links OpenSSL for the #ME2.00$ format;
# CRYPT_S=0 compiles encryption out entirely (no -lcrypto), which is what lets
# a fully static binary link on systems without libcrypto.a (e.g. Arch).
CRYPT_S ?= 1
ifeq ($(CRYPT_S),0)
CRYPTO_LIB =
else
CRYPTO_LIB = -lcrypto
endif

# static binary: use the `me.static' target below (needs static archives)
CFLAGS= -g -Wno-pedantic -fPIC -Wno-implicit -DCRYPT_S=$(CRYPT_S) $(PCRE_CFLAGS)
#LIBS= -lefence -ltermcap -lc -lncurses
# -lcrypto (OpenSSL) only when CRYPT_S != 0; see crypt_buf.c
LIBS= -lncurses $(PCRE_LIBS) $(CRYPTO_LIB)
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

# Fully static build.  Needs a static archive (.a) for EVERY dependency:
# ncurses (incl. its terminfo half -- tgoto/tputs), PCRE, and (unless CRYPT_S=0)
# OpenSSL libcrypto.  Many distros ship these only in a -static/-devel package,
# and Arch packages NONE of them as .a -- neither libcrypto.a nor a static
# ncurses/terminfo -- so a fully static build on Arch needs those archives
# built by hand.  CRYPT_S=0 removes only the libcrypto requirement.  Archives
# are auto-detected below; override any *_STATIC var to point at yours.
#   Debian/Ubuntu: apt install libssl-dev libncurses-dev libtinfo-dev libpcre2-dev
#   Fedora/RHEL:   dnf install openssl-static ncurses-static pcre2-static
# libcrypto is only needed (and only checked/linked) when CRYPT_S != 0.
ifeq ($(CRYPT_S),0)
NEED_CRYPTO =
CRYPTO_STATIC =
else
NEED_CRYPTO = 1
CRYPTO_STATIC = $(firstword $(wildcard /usr/lib/libcrypto.a /usr/lib/x86_64-linux-gnu/libcrypto.a /usr/lib64/libcrypto.a))
endif
CURSES_STATIC = $(firstword $(wildcard /usr/lib/libncursesw.a /usr/lib/libncurses.a /usr/lib/x86_64-linux-gnu/libncursesw.a /usr/lib64/libncursesw.a))
# extra archives libcrypto/ncurses/glibc pull in when linked statically.
# A static libcrypto may additionally need -lz (and sometimes -lzstd/-lbrotli),
# depending on how your OpenSSL was built -- append them via STATIC_EXTRA if the
# link reports undefined inflate/ZSTD_/Brotli symbols.
TINFO_STATIC  = $(wildcard /usr/lib/libtinfo.a /usr/lib/x86_64-linux-gnu/libtinfo.a /usr/lib64/libtinfo.a)
STATIC_EXTRA  = -lpthread -ldl

me.static: $(OFILES)
		@err=0; \
		if [ -n "$(NEED_CRYPTO)" ] && [ ! -f "$(CRYPTO_STATIC)" ]; then echo "  missing: libcrypto.a (OpenSSL) -- override CRYPTO_STATIC=/path, or build CRYPT_S=0"; err=1; fi; \
		if [ ! -f "$(CURSES_STATIC)" ]; then echo "  missing: libncurses(w).a -- override CURSES_STATIC=/path"; err=1; fi; \
		if [ ! -f "$(PCRE_STATIC)" ];   then echo "  missing: libpcre2-8.a -- override PCRE_STATIC=/path"; err=1; fi; \
		if [ $$err -ne 0 ]; then \
		    echo "me.static: cannot link statically -- static archive(s) above are absent."; \
		    echo "  Install the static/-dev package, or point the *_STATIC var at your .a."; \
		    echo "  (Arch does not package libcrypto.a; a fully static build with encryption"; \
		    echo "   is not possible there without a self-built static OpenSSL.)"; \
		    exit 1; \
		fi
		$(CC) -static $(CFLAGS) -L. $(OFILES) \
		    $(CURSES_STATIC) $(TINFO_STATIC) $(PCRE_STATIC) $(CRYPTO_STATIC) \
		    $(STATIC_EXTRA) -lc \
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
	rm -f *.o core ,,* *~ *.~[0-9]*~

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
