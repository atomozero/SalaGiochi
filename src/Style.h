/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef STYLE_H
#define STYLE_H


#include <Font.h>
#include <GraphicsDefs.h>
#include <Rect.h>


class BView;


namespace Style {

extern const rgb_color kNight;		// background
extern const rgb_color kNightLight;	// pills, cards
extern const rgb_color kNightDark;	// bottom bar
extern const rgb_color kSun;		// selection, accents
extern const rgb_color kGreen;
extern const rgb_color kGreenShadow;
extern const rgb_color kOrange;
extern const rgb_color kOrangeShadow;
extern const rgb_color kWhite;
extern const rgb_color kMist;		// secondary text on kNight
extern const rgb_color kPaper;		// parents' area
extern const rgb_color kInk;		// text on kPaper
extern const rgb_color kInkSoft;

// The screens are laid out for 1024x600 (the AC100) and scaled to the real
// size of the screen
extern const BSize kLayoutSize;
float		LayoutScale(BRect bounds);

BFont		Font(float size, bool bold = true);
void		DrawCentered(BView* view, const char* text, BRect frame,
				const BFont& font, rgb_color color);
void		DrawText(BView* view, const char* text, BPoint leftBaseline,
				const BFont& font, rgb_color color);
void		FillRound(BView* view, BRect frame, float radius,
				rgb_color color);
// a button with a darker band below it, like a key that can be pressed
void		FillButton(BView* view, BRect frame, float radius,
				rgb_color color, rgb_color shadow);

void		DrawArrow(BView* view, BRect frame, bool right, rgb_color color);
void		DrawPlay(BView* view, BRect frame, rgb_color color);
void		DrawHouse(BView* view, BRect frame, rgb_color color);
void		DrawClock(BView* view, BRect frame, rgb_color color);
void		DrawPause(BView* view, BRect frame, rgb_color color);
void		DrawMoon(BView* view, BRect frame, rgb_color color,
				rgb_color background);

}	// namespace Style


#endif	// STYLE_H
