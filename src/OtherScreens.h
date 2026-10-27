/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef OTHER_SCREENS_H
#define OTHER_SCREENS_H


#include "Screen.h"


class GameLibrary;
class Settings;


/*!	"Enough for today": shown once the time of the day is used up, until
	the parents give more.
*/
class TimeUpView : public Screen {
public:
								TimeUpView();

	virtual	void				Draw(BRect updateRect);
	virtual	void				HandleAction(Action action) {}
};


/*!	The parents' settings: time per day, the loudest volume, the games to
	show. Kept simple: up and down choose a line, left and right change it.
*/
class ParentsView : public Screen {
public:
								ParentsView(GameLibrary& library,
									Settings& settings);

	virtual	void				Draw(BRect updateRect);
	virtual	void				MouseDown(BPoint where);
	virtual	void				HandleAction(Action action);

			void				Reset();

private:
			int32				_CountLines() const;
			BRect				_LineFrame(int32 line) const;
			void				_Change(int32 line, int32 direction);
			void				_Activate(int32 line);

			GameLibrary&		fLibrary;
			Settings&			fSettings;
			int32				fLine;
};


#endif	// OTHER_SCREENS_H
