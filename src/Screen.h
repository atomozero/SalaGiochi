/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef SCREEN_H
#define SCREEN_H


#include <View.h>


enum Action {
	kActionLeft,
	kActionRight,
	kActionUp,
	kActionDown,
	kActionConfirm,
	kActionBack,
};


/*!	One full screen of the application. The window turns keys and gamepad
	buttons into actions for the screen on top.
*/
class Screen : public BView {
public:
								Screen(const char* name)
									:
									BView(name, B_WILL_DRAW
										| B_FULL_UPDATE_ON_RESIZE)
								{
								}

	virtual	void				HandleAction(Action action) = 0;
};


#endif	// SCREEN_H
