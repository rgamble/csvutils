CC=gcc
CFLAGS=-Wall -ansi -pedantic
DESTDIR=/usr/local/bin
MANDIR=/usr/share/man/man1
INCLUDES=-I include
CPPFLAGS=

VPATH=src

all: csvcut csvgrep csvcount csvcheck csvfix csvbreak

install: install_utils install_man

csvcut csvgrep csvcount csvcheck csvfix csvbreak: version.o
csvcut csvgrep csvcount csvcheck csvfix csvbreak: helper.o

csvcut: csvcut.c 
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $^ -o $@ -lcsv

csvgrep: csvgrep.c 
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $^ -o $@ -lcsv -lpcre

csvcount: csvcount.c 
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $^ -o $@ -lcsv

csvcheck: csvcheck.c 
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $^ -o $@ -lcsv

csvfix: csvfix.c 
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $^ -o $@ -lcsv

csvbreak: csvbreak.c 
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $^ -o $@ -lcsv

version.o: version.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -c $<

helper.o: helper.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -c $<

install_utils: csvcut csvgrep csvcount csvcheck csvfix csvbreak
	cp -f $^ $(DESTDIR)

install_man: csvutils.1.gz csvcut.1.gz csvgrep.1.gz csvcount.1.gz \
             csvcheck.1.gz csvfix.1.gz csvbreak.1.gz
	cp -f $^ $(MANDIR)

clean:
	rm *.o csvcut csvgrep csvcount csvcheck csvfix csvbreak
