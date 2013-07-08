/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/**
 * @author Martin Gropp
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>

#include "xkeys.h"

#define VERSION "0.0.1 (2013-07-08)"
#define MPLAYER "/usr/bin/mplayer"
#define SEEK_SECONDS "-4"
#define MPLAYER_PAUSE "pause\n"
#define MPLAYER_RESUME "seek " SEEK_SECONDS " 0\n"

// This variable is set to != 0 to quit after receiving the next X event.
volatile static sig_atomic_t quitRequest = 0;

static int keyCode = 0;
static int keyModifier = 0;

/**
 * Handler for SIGINT.
 * Set quitRequest variable and fake a key release --
 * this seems to be the only way to cancel an XNextEvent call.
 */
static void sigIntHandler(int signum) {
	if (quitRequest) {
		exit(0x80 | signum);
	} else {
		quitRequest = signum;
		fakeKeyRelease(keyCode, keyModifier);
	}
}

pid_t pipe_fork(int* fd_read, int* fd_write) {
	int pipefd[2];
	if (pipe(pipefd) != 0) {
		perror("pipe");
		exit(1);
	}
	
	*fd_read = pipefd[0];
	*fd_write = pipefd[1];
	
	pid_t fpid = fork();
	if (fpid == -1) {
		perror("fork");
		exit(1);
	}
	
	return fpid;
}

/**
 * @param key
 *   Either a single character or an ANSI escape sequence
 *   (padded with 0 from the 'left').
 */
static int key_command(int key, FILE* mplayer_pipe) {
	switch (key) {
		case 'q':
			printf("Quitting...\n");
			fprintf(mplayer_pipe, "quit\n");
			fflush(mplayer_pipe);
			return 1;
		
		case ' ':
			fprintf(mplayer_pipe, "pause\n");
			fflush(mplayer_pipe);
			return 0;
		
		case 0x1b5b41:
			// up
			fprintf(mplayer_pipe, "seek 60 0\n");
			fflush(mplayer_pipe);
			return 0;
			
		case 0x1b5b42:
			// down
			fprintf(mplayer_pipe, "seek -60 0\n");
			fflush(mplayer_pipe);
			return 0;
			
		case 0x1b5b43:
			// right
			fprintf(mplayer_pipe, "seek 10 0\n");
			fflush(mplayer_pipe);
			return 0;
			
		case 0x1b5b44:
			// left
			fprintf(mplayer_pipe, "seek -10 0\n");
			fflush(mplayer_pipe);
			return 0;
		
		case 0x1b5b357e:
			// page up
			fprintf(mplayer_pipe, "seek 600 0\n");
			fflush(mplayer_pipe);
			return 0;
		
		case 0x1b5b367e:
			// page down
			fprintf(mplayer_pipe, "seek -600 0\n");
			fflush(mplayer_pipe);
			return 0;
		
		default:
			return 0;
	}
}

// monstrosity
int main(int argc, char** argv) {
	// Parse command line arguments
	char* keyName = "F4";
	int c;
	while ((c = getopt(argc, argv, "k:casv?")) != -1) {
		switch (c) {
			case 'k':
				// Key name
				keyName = optarg;
				break;

			case 'c':
				// Control
				keyModifier |= ControlMask;
				break;

			case 'a':
				// Alt
				keyModifier |= Mod1Mask;
				break;

			case 's':
				// Shift
				keyModifier |= ShiftMask;
				break;

			case 'v':
				printf("%s\nVersion: %s\n", argv[0], VERSION);
				return 0;

			default:
				fprintf(stderr, "%s %s\n", argv[0], VERSION);
				fprintf(stderr, "\nSyntax:\n%s [-c] [-a] [-s] [-k <keysym>] <media file>\n", argv[0]);
				fprintf(stderr, "\nUse xev to find the keysym for a key.\n");
				fprintf(stderr, "\nExample:\n`%s -c -s -k F12 audio.wav` grabs Ctrl+Shift+F12\n\n", argv[0]);
				return 1;
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "Missing argument: media file.\n");
		exit(1);
	}

	char* mediafile = argv[optind];

	if ((keyModifier == ControlMask) && (strlen(keyName) == 1) && (keyName[0] == 'c' || keyName[0] == 'C')) {		
		fprintf(stderr, "Grabbing Ctrl+C is a bad idea. I won't do that.\n");
		return 10;
	}
	
	printf("==========================================================================\n");
	printf("Global pause/resume key: %s%s\n", mod2str(keyModifier), keyName);
	printf("==========================================================================\n");

	// Prepare pipe
	int pipefd[2];
	pid_t mplayer_pid = pipe_fork(pipefd, pipefd+1);	

	// === mplayer (child) ===
	if (mplayer_pid == 0) {
		// stdin = pipe
		close(pipefd[1]);
		dup2(pipefd[0], STDIN_FILENO);
		close(pipefd[0]);

		execl(MPLAYER, MPLAYER, "-slave", mediafile, NULL);

		perror("exec");
		return 1;
	}

	// === parent ===
	close(pipefd[0]);
	FILE* mplayer_pipe = fdopen(pipefd[1], "w");
	
	pid_t x_pid = fork();
	if (x_pid == -1) {
		perror("fork");
		exit(1);
	}
	
	// === global X key (child) ===
	if (x_pid == 0) {
		// Register signal handler for cleaning up
		signal(SIGINT, sigIntHandler);
		
		Display* display = initX(keyName, keyModifier, &keyCode);
		
		int state = 0;
		XEvent event;
		while (!quitRequest) {
			XNextEvent(display, &event);
			if (event.type == KeyPress) {
				if (state == 0) {
					fprintf(mplayer_pipe, MPLAYER_PAUSE);
					fflush(mplayer_pipe);
					state = 1;
				} else {
					fprintf(mplayer_pipe, MPLAYER_RESUME);
					fflush(mplayer_pipe);
					state = 0;
				}
			}
		}
	
		// Clean up
		printf("\nQuitting...\n");
		cleanupX(display);
		
		fclose(mplayer_pipe);
		return 0;
	}
	
	// === old stdin (parent) ===
	// disable stdio buffering
	struct termios old_tio;
	struct termios new_tio;
	tcgetattr(STDIN_FILENO, &old_tio);
	new_tio = old_tio;
	new_tio.c_lflag &= (~ICANON & ~ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
	
	// TODO: signal handler to restore settings

	// mplayer control via terminal (non-global)
	printf("Ready.\n");	
	int quit = 0;		
	while (!quit) {
		// wait for data on stdin
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(0, &rfds);
		
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 500000;
		
		int selected = select(1, &rfds, NULL, NULL, &tv);
		if (selected < 0) {
			perror("select");
			exit(1);
		}
		
		// check if mplayer is still running
		int r = waitpid(mplayer_pid, NULL, WNOHANG);
		if (r == -1) {
			perror("waitpid");
			exit(1);
		} else if (r > 0) {
			printf("\nmplayer quit.\n");
			break;
		}
		
		// timeout?
		if (selected == 0) {
			continue;
		}
		
		// finally read data
		int ch = getchar();
		if (ch == EOF) {
			break;
		}
		
		if (ch == 0x1b) {
			// ansi/vt100 escape
			// we need that to support cursor keys
			int ch2 = getchar();
			fprintf(stderr, "\n0x%.2x\n", ch2);
			if (ch2 != 0x5b) {
				// strange sequence?!
				fprintf(stderr, "\nWARNING: strange escape sequence from terminal!\n");
				break;
			}
			
			ch2 = getchar();
			fprintf(stderr, "\n0x%.2x\n", ch2);
			if ((0x40 <= ch2) && (ch2 <= 0x7e)) {
				// end of 3 byte sequence
				quit = key_command(0x1b5b00 | (ch2 & 0xff), mplayer_pipe);
			} else {
				int ch3 = getchar();
				if ((0x40 <= ch3) && (ch3 <= 0x7e)) {
					// end of 4 byte sequence
					quit = key_command(0x1b5b0000 | ((ch2 & 0xff) << 8) | (ch3 & 0xff), mplayer_pipe);
				} else {
					// not supported -- skip rest of escape sequence
					while (EOF != (ch3 = getchar())) {
						if ((0x40 <= ch3) && (ch3 <= 0x7e)) {
							break;
						}
					}
				}
			}

		} else {
			quit = key_command(ch, mplayer_pipe);
		}
	}
	
	// restore terminal settings
	tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
	
	// clean up
	fclose(mplayer_pipe);
	
	waitpid(mplayer_pid, NULL, 0);
	kill(x_pid, SIGINT);
	waitpid(x_pid, NULL, 0);

	printf("Good bye!\n");
	return 0;
}
