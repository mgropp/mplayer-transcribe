.PHONY: clean

mplayer-transcribe: mplayer-transcribe.c xkeys.c xkeys.h
	gcc --std=c99 -D_XOPEN_SOURCE -Wall -Werror -o mplayer-transcribe xkeys.c mplayer-transcribe.c -lX11

clean:
	rm -f mplayer-transcribe
