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
 * @note Contains code from xbindkeys, (C) 2001 by Philippe Brochard
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>

#include "xkeys.h"

static unsigned int numLockMask = 0;
static unsigned int scrollLockMask = 0;
static unsigned int capsLockMask = 0;

/**
 * "Find out the masks for the NumLock and ScrollLock modifiers,
 * so that we can bind the grabs for when they are enabled too."
 */
void initMasks(Display* display) {
	KeyCode nlock = XKeysymToKeycode(display, XK_Num_Lock);
	KeyCode slock = XKeysymToKeycode(display, XK_Scroll_Lock);
	static int mask_table[8] = {
		ShiftMask, LockMask, ControlMask, Mod1Mask,
		Mod2Mask, Mod3Mask, Mod4Mask, Mod5Mask
	};

	XModifierKeymap* modmap = XGetModifierMapping(display);
	if (modmap != NULL && modmap->max_keypermod > 0) {
		for (int i = 0; i < 8 * modmap->max_keypermod; i++) {
			if (modmap->modifiermap[i] == nlock && nlock != 0) {
				numLockMask = mask_table[i / modmap->max_keypermod];
			} else if (modmap->modifiermap[i] == slock && slock != 0) {
				scrollLockMask = mask_table[i / modmap->max_keypermod];
			}
		}
	}

	capsLockMask = LockMask;

	if (modmap) {
		XFreeModifiermap(modmap);
	}
}

/**
 * Actually grab a key (and its *Lock combinations).
 */
void grabKey(Display* display, Window rootWindow, int keycode, int modifier) {
	XGrabKey(display, keycode, modifier, rootWindow, False, GrabModeAsync, GrabModeAsync);

	if (modifier != AnyModifier) {
		// Also grab with NumLock, CapsLock, ScrollLock
		if (numLockMask) {
			XGrabKey(display, keycode, modifier | numLockMask, rootWindow, False, GrabModeAsync, GrabModeAsync);
		}
		if (capsLockMask) {
			XGrabKey(display, keycode, modifier | capsLockMask, rootWindow, False, GrabModeAsync, GrabModeAsync);
		}
		if (scrollLockMask) {
			XGrabKey(display, keycode, modifier | scrollLockMask, rootWindow, False, GrabModeAsync, GrabModeAsync);
		}
		if (numLockMask && capsLockMask) {
			XGrabKey(display, keycode, modifier | numLockMask | capsLockMask, rootWindow, False, GrabModeAsync, GrabModeAsync);
		}
		if (numLockMask && scrollLockMask) {
			XGrabKey(display, keycode, modifier | numLockMask | scrollLockMask, rootWindow, False, GrabModeAsync, GrabModeAsync);
		}
		if (capsLockMask && scrollLockMask) {
			XGrabKey(display, keycode, modifier | capsLockMask | scrollLockMask, rootWindow, False, GrabModeAsync, GrabModeAsync);
		}
		if (numLockMask && capsLockMask && scrollLockMask) {
			XGrabKey(display, keycode, modifier | numLockMask | capsLockMask | scrollLockMask, rootWindow, False, GrabModeAsync, GrabModeAsync);
		}
	}
}

/**
 * Initialize X connection, establish grab.
 */
Display* initX(const char* keyName, int keyModifier, int* keyCode) {
	KeySym sym = XStringToKeysym(keyName);
	if (sym == NoSymbol) {
		fprintf(stderr, "ERROR: Unknown key: %s\n", keyName);
		exit(2);
	}

	Display* display = XOpenDisplay(NULL);
	if (display == NULL) {
		fprintf(stderr, "ERROR: Could not open display.\n");
		exit(3);
	}
	
	*keyCode = XKeysymToKeycode(display, sym);

	Window rootWindow = DefaultRootWindow(display);

	initMasks(display);
	Bool detectableAutoRepeatSupported;
	XkbSetDetectableAutoRepeat(display, True, &detectableAutoRepeatSupported);
	if (!detectableAutoRepeatSupported) {
		fprintf(stderr, "ERROR: Detectable auto repeat is not supported.\n");
		exit(4);
	}

	// Grab
	grabKey(display, rootWindow, *keyCode, keyModifier);

	// Enable events
	XAllowEvents(display, AsyncBoth, CurrentTime);
	XSelectInput(display, rootWindow, KeyPressMask | KeyReleaseMask);

	return display;
}

/**
 * Ungrab key and close display.
 */
void cleanupX(Display* display) {
	if (display != NULL) {
		XUngrabKey(display, AnyKey, AnyModifier, DefaultRootWindow(display));
		XCloseDisplay(display);
	}
}

/**
 * Fake a key release to cancel a blocking XNextEvent.
 */
void fakeKeyRelease(int keyCode, int keyModifier) {
	Display* display = XOpenDisplay(NULL);
	Window rootWindow = DefaultRootWindow(display);

	XKeyEvent event;
	memset(&event, 0, sizeof(XKeyEvent));
	event.type = KeyRelease;
	event.time = CurrentTime;
	event.send_event = True;
	event.display = display;
	event.window = rootWindow;
	event.root = rootWindow;
	event.state = keyModifier;
	event.keycode = keyCode;
	event.same_screen = True;

	XSendEvent(
		display,
		rootWindow,
		True,
		KeyReleaseMask,
		(XEvent*)&event
	);

	XCloseDisplay(display);
}

/**
 * Translate a key modifier bitmap to a char*.
 */
char* mod2str(int modifier) {
	// data segment is better than dynamic allocation
	switch (modifier & (ControlMask | Mod1Mask | ShiftMask)) {
		case 0:
			return "";
		case ControlMask:
			return "Ctrl+";
		case Mod1Mask:
			return "Alt+";
		case ShiftMask:
			return "Shift+";
		case ControlMask | Mod1Mask:
			return "Ctrl+Alt+";
		case ControlMask | ShiftMask:
			return "Ctrl+Shift+";
		case Mod1Mask | ShiftMask:
			return "Alt+Shift+";
		case ControlMask | Mod1Mask | ShiftMask:
			return "Ctrl+Alt+Shift+";
		default:
			return "???";
	}
}
