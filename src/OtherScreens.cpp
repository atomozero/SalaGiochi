/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "OtherScreens.h"

#include <Window.h>

#include <Catalog.h>

#include "GameLibrary.h"
#include "Messages.h"
#include "Settings.h"
#include "Style.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Screens"


using namespace Style;


TimeUpView::TimeUpView()
	:
	Screen("time up")
{
	SetViewColor(B_TRANSPARENT_COLOR);
}


void
TimeUpView::Draw(BRect updateRect)
{
	BRect bounds = Bounds();
	SetHighColor(kNightLight);
	FillRect(updateRect);

	// a few stars
	static const BPoint kStars[] = {
		BPoint(0.11f, 0.11f), BPoint(0.23f, 0.23f), BPoint(0.83f, 0.16f),
		BPoint(0.91f, 0.35f), BPoint(0.16f, 0.72f), BPoint(0.86f, 0.8f)
	};
	for (const BPoint& star : kStars) {
		BPoint center(bounds.Width() * star.x, bounds.Height() * star.y);
		FillRound(this, BRect(center.x - 5, center.y - 5, center.x + 5,
			center.y + 5), 5, (rgb_color){ 255, 224, 138, 255 });
	}

	float centerX = bounds.Width() / 2;
	float top = bounds.Height() / 2 - 210;
	BRect circle(centerX - 90, top, centerX + 90, top + 180);
	FillRound(this, circle.InsetByCopy(-18, -18), 108,
		(rgb_color){ 255, 201, 60, 46 });
	FillRound(this, circle, 90, kSun);
	DrawMoon(this, circle.InsetByCopy(42, 42), kNight, kSun);

	DrawCentered(this, B_TRANSLATE("That's enough playing for today!"), BRect(0, top + 205,
		bounds.right, top + 275), Font(54), kWhite);
	DrawCentered(this,
		B_TRANSLATE("The games will wait for you tomorrow. The game is saved."),
		BRect(0, top + 280, bounds.right, top + 320), Font(26, false),
		(rgb_color){ 207, 224, 245, 255 });
}


//	#pragma mark -


static const float kHeader = 76;
static const float kLineHeight = 48;


ParentsView::ParentsView(GameLibrary& library, Settings& settings)
	:
	Screen("parents"),
	fLibrary(library),
	fSettings(settings),
	fLine(0)
{
	SetViewColor(B_TRANSPARENT_COLOR);
}


void
ParentsView::Reset()
{
	fLine = 0;
	Invalidate();
}


int32
ParentsView::_CountLines() const
{
	// time, volume, the games, back, exit
	return 2 + fLibrary.CountGames() + 2;
}


BRect
ParentsView::_LineFrame(int32 line) const
{
	BRect bounds = Bounds();
	int32 fitting = (int32)((bounds.Height() - kHeader - 20) / kLineHeight);
	int32 first = 0;
	if (fLine >= fitting)
		first = fLine - fitting + 1;
	float top = kHeader + (line - first) * kLineHeight;
	return BRect(32, top, bounds.right - 32, top + kLineHeight - 6);
}


void
ParentsView::Draw(BRect updateRect)
{
	BRect bounds = Bounds();
	SetHighColor(kPaper);
	FillRect(updateRect);

	DrawText(this, B_TRANSLATE("Parents' area"), BPoint(32, 50), Font(30),
		kInk);
	const char* help = B_TRANSLATE("Up and down choose, left and right "
		"change, Enter confirms");
	DrawText(this, help, BPoint(bounds.right - 32
		- Font(15, false).StringWidth(help), 48), Font(15, false), kInkSoft);

	int32 count = _CountLines();
	for (int32 line = 0; line < count; line++) {
		BRect frame = _LineFrame(line);
		if (frame.top < kHeader || frame.bottom > bounds.bottom)
			continue;

		bool current = line == fLine;
		FillRound(this, frame, 12, current
			? (rgb_color){ 255, 236, 179, 255 } : kWhite);
		if (current) {
			SetHighColor(kSun);
			SetPenSize(3);
			StrokeRoundRect(frame.InsetByCopy(1.5, 1.5), 12, 12);
			SetPenSize(1);
		}

		BPoint text(frame.left + 18, frame.top + 29);
		BString label;
		BString value;
		bool hidden = false;
		int32 game = line - 2;
		if (line == 0) {
			label = B_TRANSLATE("Play time per day");
			value = B_TRANSLATE("(%played% min used today)    ‹  %minutes% min  ›");
			BString number;
			number << (int32)fSettings.PlayedMinutes();
			value.ReplaceFirst("%played%", number.String());
			number.SetTo("");
			number << fSettings.DailyMinutes();
			value.ReplaceFirst("%minutes%", number.String());
		} else if (line == 1) {
			label = B_TRANSLATE("Loudest volume");
			value << "‹  " << fSettings.MaxVolume() << " %  ›";
		} else if (game < fLibrary.CountGames()) {
			const Game& entry = fLibrary.GameAt(game);
			label << entry.name << "  ·  " << entry.system;
			hidden = fSettings.IsHidden(entry.name.String());
			value = hidden ? B_TRANSLATE("hidden") : B_TRANSLATE("shown");
		} else if (game == fLibrary.CountGames()) {
			label = B_TRANSLATE("Back to the games");
		} else {
			label = B_TRANSLATE("Quit to the desktop");
		}

		DrawText(this, label.String(), text, Font(20, line < 2), kInk);
		if (!value.IsEmpty()) {
			BFont font = Font(20);
			DrawText(this, value.String(), BPoint(frame.right - 18
				- font.StringWidth(value.String()), text.y), font,
				hidden ? kInkSoft : kInk);
		}
	}
}


void
ParentsView::MouseDown(BPoint where)
{
	for (int32 line = 0; line < _CountLines(); line++) {
		BRect frame = _LineFrame(line);
		if (frame.top < kHeader || !frame.Contains(where))
			continue;
		fLine = line;
		if (line < 2)
			_Change(line, where.x > frame.left + frame.Width() / 2 ? 1 : -1);
		else
			_Activate(line);
		Invalidate();
		return;
	}
}


void
ParentsView::HandleAction(Action action)
{
	switch (action) {
		case kActionUp:
			if (fLine > 0)
				fLine--;
			break;
		case kActionDown:
			if (fLine < _CountLines() - 1)
				fLine++;
			break;
		case kActionLeft:
			_Change(fLine, -1);
			break;
		case kActionRight:
			_Change(fLine, 1);
			break;
		case kActionConfirm:
			_Activate(fLine);
			break;
		case kActionBack:
			Window()->PostMessage(kMsgCloseParents);
			break;
	}
	Invalidate();
}


void
ParentsView::_Change(int32 line, int32 direction)
{
	if (line == 0)
		fSettings.SetDailyMinutes(fSettings.DailyMinutes() + 5 * direction);
	else if (line == 1)
		fSettings.SetMaxVolume(fSettings.MaxVolume() + 10 * direction);
	else if (line - 2 < fLibrary.CountGames())
		_Activate(line);
	else
		return;
	Window()->PostMessage(kMsgSettingsChanged);
}


void
ParentsView::_Activate(int32 line)
{
	int32 game = line - 2;
	if (line < 2)
		return;
	if (game < fLibrary.CountGames()) {
		const char* name = fLibrary.GameAt(game).name.String();
		fSettings.SetHidden(name, !fSettings.IsHidden(name));
		Window()->PostMessage(kMsgSettingsChanged);
	} else if (game == fLibrary.CountGames())
		Window()->PostMessage(kMsgCloseParents);
	else
		Window()->PostMessage(kMsgExitApp);
}
