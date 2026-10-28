/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "Style.h"

#include <algorithm>

#include <Shape.h>
#include <View.h>


namespace Style {

const rgb_color kNight = { 23, 48, 79, 255 };
const rgb_color kNightLight = { 35, 66, 107, 255 };
const rgb_color kNightDark = { 18, 39, 63, 255 };
const rgb_color kSun = { 255, 201, 60, 255 };
const rgb_color kGreen = { 63, 185, 80, 255 };
const rgb_color kGreenShadow = { 42, 138, 58, 255 };
const rgb_color kOrange = { 255, 138, 91, 255 };
const rgb_color kOrangeShadow = { 201, 90, 48, 255 };
const rgb_color kWhite = { 255, 255, 255, 255 };
const rgb_color kMist = { 185, 203, 228, 255 };
const rgb_color kPaper = { 238, 242, 247, 255 };
const rgb_color kInk = { 23, 48, 79, 255 };
const rgb_color kInkSoft = { 74, 91, 112, 255 };


const BSize kLayoutSize(1024, 600);


float
LayoutScale(BRect bounds)
{
	float scale = std::min((bounds.Width() + 1) / kLayoutSize.width,
		(bounds.Height() + 1) / kLayoutSize.height);
	return std::max(0.5f, scale);
}


// every drawing blends by the alpha of its color
static void
_UseAlpha(BView* view)
{
	view->SetDrawingMode(B_OP_ALPHA);
	view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
}


BFont
Font(float size, bool bold)
{
	BFont font(bold ? be_bold_font : be_plain_font);
	font.SetSize(size);
	return font;
}


void
DrawCentered(BView* view, const char* text, BRect frame, const BFont& font,
	rgb_color color)
{
	view->SetFont(&font);
	view->SetHighColor(color);
	font_height height;
	font.GetHeight(&height);
	float width = font.StringWidth(text);
	float x = frame.left + (frame.Width() - width) / 2;
	float y = frame.top + (frame.Height() + height.ascent - height.descent) / 2;
	_UseAlpha(view);
	view->DrawString(text, BPoint(x, y));
}


void
DrawText(BView* view, const char* text, BPoint leftBaseline,
	const BFont& font, rgb_color color)
{
	view->SetFont(&font);
	view->SetHighColor(color);
	_UseAlpha(view);
	view->DrawString(text, leftBaseline);
}


void
FillRound(BView* view, BRect frame, float radius, rgb_color color)
{
	_UseAlpha(view);
	view->SetHighColor(color);
	view->FillRoundRect(frame, radius, radius);
}


void
FillButton(BView* view, BRect frame, float radius, rgb_color color,
	rgb_color shadow)
{
	FillRound(view, frame.OffsetByCopy(0, 8), radius, shadow);
	FillRound(view, frame, radius, color);
}


void
DrawArrow(BView* view, BRect frame, bool right, rgb_color color)
{
	float midY = frame.top + frame.Height() / 2;
	BPoint tip(right ? frame.right : frame.left, midY);
	BPoint back(right ? frame.left + frame.Width() * 0.4f
		: frame.right - frame.Width() * 0.4f, 0);
	view->SetHighColor(color);
	view->SetPenSize(frame.Width() / 6);
	view->SetLineMode(B_ROUND_CAP, B_ROUND_JOIN);
	view->StrokeLine(BPoint(back.x, frame.top), tip);
	view->StrokeLine(tip, BPoint(back.x, frame.bottom));
	view->SetPenSize(1);
}


void
DrawPlay(BView* view, BRect frame, rgb_color color)
{
	view->SetHighColor(color);
	view->FillTriangle(BPoint(frame.left + frame.Width() * 0.2f, frame.top),
		BPoint(frame.left + frame.Width() * 0.2f, frame.bottom),
		BPoint(frame.right, frame.top + frame.Height() / 2));
}


void
DrawHouse(BView* view, BRect frame, rgb_color color)
{
	float w = frame.Width();
	float h = frame.Height();
	view->SetHighColor(color);
	view->SetPenSize(w / 10);
	view->SetLineMode(B_ROUND_CAP, B_ROUND_JOIN);
	BPoint roof[3] = {
		BPoint(frame.left, frame.top + h * 0.45f),
		BPoint(frame.left + w / 2, frame.top),
		BPoint(frame.right, frame.top + h * 0.45f)
	};
	view->StrokePolygon(roof, 3, false);
	view->StrokeRect(BRect(frame.left + w * 0.15f, frame.top + h * 0.4f,
		frame.right - w * 0.15f, frame.bottom));
	view->FillRect(BRect(frame.left + w * 0.42f, frame.top + h * 0.65f,
		frame.right - w * 0.42f, frame.bottom));
	view->SetPenSize(1);
}


void
DrawClock(BView* view, BRect frame, rgb_color color)
{
	view->SetHighColor(color);
	view->SetPenSize(frame.Width() / 9);
	view->SetLineMode(B_ROUND_CAP, B_ROUND_JOIN);
	view->StrokeEllipse(frame.InsetByCopy(frame.Width() * 0.06f,
		frame.Height() * 0.06f));
	BPoint center(frame.left + frame.Width() / 2,
		frame.top + frame.Height() / 2);
	view->StrokeLine(center, BPoint(center.x, frame.top + frame.Height() * 0.25f));
	view->StrokeLine(center, BPoint(center.x + frame.Width() * 0.2f,
		center.y + frame.Height() * 0.12f));
	view->SetPenSize(1);
}


void
DrawPause(BView* view, BRect frame, rgb_color color)
{
	float w = frame.Width();
	FillRound(view, BRect(frame.left + w * 0.2f, frame.top,
		frame.left + w * 0.4f, frame.bottom), w * 0.06f, color);
	FillRound(view, BRect(frame.right - w * 0.4f, frame.top,
		frame.right - w * 0.2f, frame.bottom), w * 0.06f, color);
}


void
DrawMoon(BView* view, BRect frame, rgb_color color, rgb_color background)
{
	view->SetHighColor(color);
	view->FillEllipse(frame);
	BRect cut = frame.OffsetByCopy(frame.Width() * 0.3f,
		-frame.Height() * 0.2f);
	view->SetHighColor(background);
	view->FillEllipse(cut);
}

}	// namespace Style
