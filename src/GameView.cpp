/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "GameView.h"

#include <algorithm>

#include <Bitmap.h>
#include <Region.h>
#include <Window.h>

#include <Catalog.h>

#include "Core.h"
#include "Messages.h"
#include "Style.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Game"


using namespace Style;


GameView::GameView()
	:
	Screen("game"),
	fCore(NULL),
	fPaused(false),
	fChoice(0),
	fTouching(false)
{
	SetViewColor(B_TRANSPARENT_COLOR);
}


void
GameView::SetCore(Core* core)
{
	fCore = core;
	fPaused = false;
	Invalidate();
}


void
GameView::SetPaused(bool paused)
{
	fPaused = paused;
	fChoice = 0;
	Invalidate();
}


BRect
GameView::_VideoFrame() const
{
	BRect bounds = Bounds();
	float aspect = fCore != NULL ? fCore->AspectRatio() : 4.0f / 3;
	float width = bounds.Width() + 1;
	float height = width / aspect;
	if (height > bounds.Height() + 1) {
		height = bounds.Height() + 1;
		width = height * aspect;
	}
	BRect frame(0, 0, floorf(width) - 1, floorf(height) - 1);
	frame.OffsetTo(floorf((bounds.Width() + 1 - width) / 2),
		floorf((bounds.Height() + 1 - height) / 2));
	return frame;
}


void
GameView::FrameReady()
{
	if (!fPaused)
		Invalidate(_VideoFrame());
}


void
GameView::Draw(BRect updateRect)
{
	// the picture fills the screen, only the pause is laid out
	SetScale(1);
	BRect video = _VideoFrame();

	// the borders around the picture
	SetDrawingMode(B_OP_COPY);
	SetHighColor(0, 0, 0);
	BRegion borders(updateRect);
	borders.Exclude(video);
	FillRegion(&borders);

	if (fCore != NULL && updateRect.Intersects(video)) {
		BRect source;
		const BBitmap* frame = fCore->LockFrame(source);
		if (frame != NULL && source.IsValid() && source.Width() > 0)
			DrawBitmapAsync(frame, source, video);
		else
			FillRect(video & updateRect);
		fCore->UnlockFrame();
	}

	if (fPaused) {
		SetScale(LayoutScale());
		_DrawPause(LayoutBounds());
	}
}


BRect
GameView::_ButtonFrame(int32 button) const
{
	BRect bounds = LayoutBounds();
	float centerX = bounds.Width() / 2;
	float top = bounds.Height() / 2 - 60;
	float left = button == 0 ? centerX - 316 : centerX + 16;
	return BRect(left, top, left + 300, top + 190);
}


void
GameView::_DrawPause(BRect bounds)
{
	SetDrawingMode(B_OP_ALPHA);
	SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
	SetHighColor(12, 26, 44, 184);
	FillRect(bounds);

	float centerX = bounds.Width() / 2;
	BRect title(centerX - 150, bounds.Height() / 2 - 170, centerX + 150,
		bounds.Height() / 2 - 100);
	DrawPause(this, BRect(title.left, title.top + 14, title.left + 44,
		title.top + 58), kSun);
	DrawText(this, B_TRANSLATE("Pause"), BPoint(title.left + 64, title.top + 54), Font(52),
		kWhite);

	for (int32 button = 0; button < 2; button++) {
		BRect frame = _ButtonFrame(button);
		if (button == fChoice) {
			FillRound(this, frame.InsetByCopy(-10, -10), 40,
				(rgb_color){ 255, 201, 60, 255 });
		}
		if (button == 0) {
			FillButton(this, frame, 32, kGreen, kGreenShadow);
			DrawPlay(this, BRect(frame.left + 114, frame.top + 30,
				frame.left + 186, frame.top + 102), kWhite);
			DrawCentered(this, B_TRANSLATE("Continue"), BRect(frame.left, frame.top + 112,
				frame.right, frame.top + 170), Font(38), kWhite);
		} else {
			FillButton(this, frame, 32, kOrange, kOrangeShadow);
			DrawHouse(this, BRect(frame.left + 116, frame.top + 30,
				frame.left + 184, frame.top + 98), kWhite);
			DrawCentered(this, B_TRANSLATE("Other games"), BRect(frame.left,
				frame.top + 112, frame.right, frame.top + 170), Font(38),
				kWhite);
		}
	}

	DrawCentered(this,
		B_TRANSLATE("The game is saved: next time it goes on from here"),
		BRect(0, bounds.Height() / 2 + 160, bounds.right,
			bounds.Height() / 2 + 200), Font(20, false), kWhite);
}


void
GameView::MouseDown(BPoint where)
{
	if (!fPaused && fCore != NULL && fCore->WantsPointer()
		&& _VideoFrame().Contains(where)) {
		// a touch screen: the mouse is the stylus
		fTouching = true;
		SetMouseEventMask(B_POINTER_EVENTS, B_NO_POINTER_HISTORY);
		_Touch(where, true);
		return;
	}
	if (!fPaused) {
		Window()->PostMessage(kMsgPause);
		return;
	}
	where = ToLayout(where);
	for (int32 button = 0; button < 2; button++) {
		if (_ButtonFrame(button).Contains(where))
			_Choose(button);
	}
}


void
GameView::MouseMoved(BPoint where, uint32 transit,
	const BMessage* dragMessage)
{
	if (fTouching)
		_Touch(where, true);
}


void
GameView::MouseUp(BPoint where)
{
	if (fTouching) {
		fTouching = false;
		_Touch(where, false);
	}
}


void
GameView::_Touch(BPoint where, bool pressed)
{
	if (fCore == NULL)
		return;
	BRect video = _VideoFrame();
	float x = (where.x - video.left) / (video.Width() + 1);
	float y = (where.y - video.top) / (video.Height() + 1);
	x = std::min(std::max(x, 0.0f), 1.0f);
	y = std::min(std::max(y, 0.0f), 1.0f);
	fCore->SetPointer((int16)(x * 65534 - 32767), (int16)(y * 65534 - 32767),
		pressed);
}


void
GameView::HandleAction(Action action)
{
	if (!fPaused) {
		if (action == kActionBack)
			Window()->PostMessage(kMsgPause);
		return;
	}
	switch (action) {
		case kActionLeft:
		case kActionUp:
			fChoice = 0;
			Invalidate();
			break;
		case kActionRight:
		case kActionDown:
			fChoice = 1;
			Invalidate();
			break;
		case kActionConfirm:
			_Choose(fChoice);
			break;
		case kActionBack:
			_Choose(0);
			break;
	}
}


void
GameView::_Choose(int32 button)
{
	Window()->PostMessage(button == 0 ? kMsgResume : kMsgQuitGame);
}
