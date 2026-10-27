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
	Opened with the mouse or a gamepad, it first asks a multiplication that a
	small child can not answer.
*/
class ParentsView : public Screen {
public:
								ParentsView(GameLibrary& library,
									Settings& settings);

	virtual	void				Draw(BRect updateRect);
	virtual	void				MouseDown(BPoint where);
	virtual	void				HandleAction(Action action);

			void				Reset(bool askQuestion);

private:
			int32				_CountLines() const;
			BRect				_LineFrame(int32 line) const;
			void				_Change(int32 line, int32 direction);
			void				_Activate(int32 line);
			void				_DrawQuestion(BRect bounds);
			BRect				_AnswerFrame(int32 answer) const;
			void				_Answer(int32 answer);

			GameLibrary&		fLibrary;
			Settings&			fSettings;
			int32				fLine;
			bool				fAsking;
			int32				fQuestion[2];
			int32				fAnswers[3];
			int32				fChoice;
};


#endif	// OTHER_SCREENS_H
