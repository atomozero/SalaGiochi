/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H


#include <Window.h>

#include "GameLibrary.h"
#include "Screen.h"
#include "Settings.h"
#include "Sounds.h"


class BMessageRunner;
class Core;
class GameView;
class Gamepad;
class LauncherView;
class ParentsView;
class TimeUpView;


class MainWindow : public BWindow {
public:
								MainWindow();
	virtual						~MainWindow();

	virtual	void				DispatchMessage(BMessage* message,
									BHandler* target);
	virtual	void				MessageReceived(BMessage* message);
	virtual	bool				QuitRequested();

private:
			void				_Show(Screen* screen);
			Screen*				_Current() const { return fCurrent; }
			void				_StartGame(int32 index);
			void				_StopGame();
			void				_SetPaused(bool paused);
			void				_OpenParents(bool askQuestion);
			void				_CloseParents();
			void				_PollPad();
			void				_PlayTick();
			bool				_HandleKey(BMessage* message, bool down);
			void				_UpdateKeyButtons();

			GameLibrary			fLibrary;
			Settings			fSettings;
			Sounds				fSounds;
			Gamepad*			fGamepad;
			Core*				fCore;

			LauncherView*		fLauncher;
			GameView*			fGameView;
			TimeUpView*			fTimeUp;
			ParentsView*		fParents;
			Screen*				fCurrent;
			Screen*				fBeforeParents;

			BMessageRunner*		fPadRunner;
			BMessageRunner*		fPlayRunner;
			bigtime_t			fLastPlayTick;
			uint32				fLastPadButtons;
			bigtime_t			fPadRepeat;
			uint32				fKeysDown[8];	// by raw key code
			bigtime_t			fComboStart;
			bigtime_t			fCornerDown;	// mouse held on the sun
			BMessageRunner*		fCornerRunner;
			bigtime_t			fPadComboStart;
};


#endif	// MAIN_WINDOW_H
