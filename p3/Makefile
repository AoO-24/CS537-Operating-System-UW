CC = gcc
CFLAGS = -Wall -Werror -pedantic -std=gnu18
LOGIN = wenpei
SUBMITPATH = ~cs537-1/handin/wenpei/P3

all: wsh

wsh: wsh.c wsh.h
	$(CC) $(CFLAGS) -o $@ $<

run: wsh
	./wsh

pack:
	tar -cvzf $(LOGIN).tar.gz wsh.c wsh.h Makefile README.md

submit: pack
	cp $(LOGIN).tar.gz $(SUBMITPATH)

clean: 
	rm -f wsh $(LOGIN).tar.gz

.PHONY: all