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

#ifndef XKEYS_H
#define XKEYS_H
#include <X11/Xlib.h>

void initMasks(Display* display);
void grabKey(Display* display, Window rootWindow, int keycode, int modifier);
Display* initX(const char* keyName, int keyModifier, int* keyCode);
void cleanupX(Display* display);
void fakeKeyRelease(int keyCode, int keyModifier);
char* mod2str(int modifier);
#endif
