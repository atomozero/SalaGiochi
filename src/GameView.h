/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef GAME_VIEW_H
#define GAME_VIEW_H


#include "Screen.h"


class Core;


/*!	The game, as big as the screen allows with its own shape, and the pause
	menu over it.
*/
class GameView : public Screen {
public:
								GameView();

			void				SetCore(Core* core);
			void				SetPaused(bool paused);
			bool				IsPaused() const { return fPaused; }

	virtual	void				Draw(BRect updateRect);
	virtual	void				MouseDown(BPoint where);
	virtual	void				HandleAction(Action action);

			void				FrameReady();

private:
			BRect				_VideoFrame() const;
			BRect				_ButtonFrame(int32 button) const;
			void				_DrawPause(BRect bounds);
			void				_Choose(int32 button);

			Core*				fCore;
			bool				fPaused;
			int32				fChoice;	// 0 continue, 1 other games
};


#endif	// GAME_VIEW_H
