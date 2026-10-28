/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef LAUNCHER_VIEW_H
#define LAUNCHER_VIEW_H


#include <String.h>

#include <vector>

#include "Screen.h"


class BMessageRunner;
class GameLibrary;
class Settings;
class Sounds;


/*!	The games in a row, the chosen one in the middle and bigger; the row
	glides to the next game.
*/
class LauncherView : public Screen {
public:
								LauncherView(GameLibrary& library,
									Settings& settings, Sounds& sounds);
	virtual						~LauncherView();

	virtual	void				Draw(BRect updateRect);
	virtual	void				MouseDown(BPoint where);
	virtual	void				MessageReceived(BMessage* message);
	virtual	void				HandleAction(Action action);

			void				GamesChanged();

private:
			bool				_IsVisible(int32 index) const;
			int32				_CountVisible() const;
			int32				_VisibleIndex(int32 visible) const;
			std::vector<BString> _Categories() const;
			void				_ChangeCategory(int32 step);
			BRect				_CategoryFrame(BRect bounds) const;
			void				_DrawCategory(BRect bounds);
			BRect				_TileFrame(float distance) const;
			void				_DrawTopBar(BRect bounds);
			void				_DrawTile(int32 index, BRect frame,
									float closeness);
			void				_DrawCover(const BBitmap* cover, BRect frame);
			void				_DrawDots(BRect bounds, float top);
			void				_DrawBottomBar(BRect bounds);
			void				_Select(int32 visible);
			void				_Start();

			GameLibrary&		fLibrary;
			Settings&			fSettings;
			Sounds&				fSounds;
			BString				fCategory;	// a system, empty for all
			int32				fSelected;	// index into the visible games
			float				fScroll;	// glides towards fSelected
			BMessageRunner*		fAnimation;
};


#endif	// LAUNCHER_VIEW_H
