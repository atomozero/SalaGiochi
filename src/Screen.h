/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef SCREEN_H
#define SCREEN_H


#include <View.h>

#include "Style.h"


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

protected:
	// The screens draw in the units of Style::kLayoutSize: Draw() starts with
	// SetScale(LayoutScale()), and LayoutBounds() is Bounds() in these units.
			float				LayoutScale() const
									{ return Style::LayoutScale(Bounds()); }
			BRect				LayoutBounds() const
								{
									float scale = LayoutScale();
									BRect bounds = Bounds();
									return BRect(0, 0,
										(bounds.Width() + 1) / scale - 1,
										(bounds.Height() + 1) / scale - 1);
								}
			BPoint				ToLayout(BPoint where) const
								{
									float scale = LayoutScale();
									return BPoint(where.x / scale,
										where.y / scale);
								}
};


#endif	// SCREEN_H
