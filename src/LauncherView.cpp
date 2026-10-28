/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "LauncherView.h"

#include <math.h>

#include <algorithm>

#include <Bitmap.h>
#include <Catalog.h>
#include <Message.h>
#include <MessageRunner.h>
#include <Region.h>
#include <Window.h>

#include "GameLibrary.h"
#include "Messages.h"
#include "Settings.h"
#include "Sounds.h"
#include "Style.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Launcher"


using namespace Style;


static const float kTopBar = 72;
static const float kBottomBar = 84;
static const float kTitleArea = 104;
static const float kGap = 22;
static const BSize kBigTile(236, 300);
static const BSize kSmallTile(150, 196);
static const uint32 kMsgAnimate = 'anim';


LauncherView::LauncherView(GameLibrary& library, Settings& settings,
	Sounds& sounds)
	:
	Screen("launcher"),
	fLibrary(library),
	fSettings(settings),
	fSounds(sounds),
	fSelected(0),
	fScroll(0),
	fAnimation(NULL)
{
	SetViewColor(B_TRANSPARENT_COLOR);
}


LauncherView::~LauncherView()
{
	delete fAnimation;
}


void
LauncherView::GamesChanged()
{
	// the console chosen may have no games left
	std::vector<BString> categories = _Categories();
	if (std::find(categories.begin(), categories.end(), fCategory)
			== categories.end())
		fCategory = "";

	int32 count = _CountVisible();
	if (fSelected >= count)
		fSelected = count > 0 ? count - 1 : 0;
	fScroll = fSelected;
	Invalidate();
}


bool
LauncherView::_IsVisible(int32 index) const
{
	const Game& game = fLibrary.GameAt(index);
	return !fSettings.IsHidden(game.name.String())
		&& (fCategory.IsEmpty() || game.system == fCategory);
}


int32
LauncherView::_CountVisible() const
{
	int32 count = 0;
	for (int32 i = 0; i < fLibrary.CountGames(); i++) {
		if (_IsVisible(i))
			count++;
	}
	return count;
}


int32
LauncherView::_VisibleIndex(int32 visible) const
{
	if (visible < 0)
		return -1;
	for (int32 i = 0; i < fLibrary.CountGames(); i++) {
		if (!_IsVisible(i))
			continue;
		if (visible-- == 0)
			return i;
	}
	return -1;
}


/*!	"All games" (empty), then the consoles that have games to show, in
	alphabetical order.
*/
std::vector<BString>
LauncherView::_Categories() const
{
	std::vector<BString> categories;
	for (int32 i = 0; i < fLibrary.CountGames(); i++) {
		const Game& game = fLibrary.GameAt(i);
		if (fSettings.IsHidden(game.name.String()))
			continue;
		if (std::find(categories.begin(), categories.end(), game.system)
				== categories.end())
			categories.push_back(game.system);
	}
	std::sort(categories.begin(), categories.end(),
		[](const BString& a, const BString& b) { return a.ICompare(b) < 0; });
	categories.insert(categories.begin(), BString());
	return categories;
}


void
LauncherView::_ChangeCategory(int32 step)
{
	std::vector<BString> categories = _Categories();
	if (categories.size() <= 2)
		return;
	int32 index = std::find(categories.begin(), categories.end(), fCategory)
		- categories.begin();
	int32 count = categories.size();
	fCategory = categories[((index + step) % count + count) % count];
	fSelected = 0;
	fScroll = 0;
	fSounds.Move();
	Invalidate();
}


/*!	The frame of a tile \a distance places away from the middle; tiles grow
	as they come to the middle.
*/
BRect
LauncherView::_TileFrame(float distance) const
{
	BRect bounds = LayoutBounds();
	float centerX = bounds.left + bounds.Width() / 2;
	float centerY = kTopBar
		+ (bounds.Height() - kTopBar - kBottomBar - kTitleArea) / 2 + 6;

	float away = fabsf(distance);
	float closeness = std::max(0.0f, 1 - away);
	float width = kSmallTile.width
		+ (kBigTile.width - kSmallTile.width) * closeness;
	float height = kSmallTile.height
		+ (kBigTile.height - kSmallTile.height) * closeness;

	const float firstStep = kBigTile.width / 2 + kGap + kSmallTile.width / 2;
	float offset = away <= 1 ? away * firstStep
		: firstStep + (away - 1) * (kSmallTile.width + kGap);
	float x = centerX + (distance < 0 ? -offset : offset);
	return BRect(x - width / 2, centerY - height / 2, x + width / 2,
		centerY + height / 2);
}


void
LauncherView::Draw(BRect updateRect)
{
	SetScale(LayoutScale());
	BRect bounds = LayoutBounds();
	SetDrawingMode(B_OP_COPY);
	SetHighColor(kNight);
	FillRect(bounds);

	_DrawTopBar(bounds);
	_DrawCategory(bounds);

	int32 count = _CountVisible();
	if (count == 0) {
		DrawCentered(this, B_TRANSLATE("There are no games here yet"),
			BRect(0, bounds.Height() / 2 - 60, bounds.right,
				bounds.Height() / 2 - 10), Font(36), kWhite);
		BString hint(B_TRANSLATE("Parents can copy them to %folder%"));
		hint.ReplaceFirst("%folder%", fLibrary.Directory());
		DrawCentered(this, hint.String(), BRect(0, bounds.Height() / 2,
			bounds.right, bounds.Height() / 2 + 40), Font(20, false), kMist);
		_DrawBottomBar(bounds);
		return;
	}

	// the far tiles first, so the nearer ones come over them
	int32 first = std::max<int32>(0, (int32)floorf(fScroll) - 4);
	int32 last = std::min<int32>(count - 1, (int32)ceilf(fScroll) + 4);
	for (int32 away = 4; away >= 0; away--) {
		for (int32 visible = first; visible <= last; visible++) {
			float distance = visible - fScroll;
			if ((int32)roundf(fabsf(distance)) != away)
				continue;
			BRect frame = _TileFrame(distance);
			if (frame.right < 0 || frame.left > bounds.right)
				continue;
			_DrawTile(_VisibleIndex(visible), frame,
				std::max(0.0f, 1 - fabsf(distance)));
		}
	}

	const Game& game = fLibrary.GameAt(_VisibleIndex(fSelected));
	float titleTop = bounds.bottom - kBottomBar - kTitleArea;
	DrawCentered(this, game.name.String(),
		BRect(0, titleTop, bounds.right, titleTop + 54), Font(44), kWhite);
	DrawCentered(this, game.system.String(),
		BRect(0, titleTop + 52, bounds.right, titleTop + 78), Font(20, false),
		kMist);
	_DrawDots(bounds, titleTop + 88);

	_DrawBottomBar(bounds);
}


void
LauncherView::_DrawTopBar(BRect bounds)
{
	// a smiling sun and a greeting
	SetDrawingMode(B_OP_ALPHA);
	SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
	BRect sun(32, 12, 80, 60);
	SetHighColor(kSun);
	FillEllipse(sun);
	SetHighColor(kNight);
	FillEllipse(BRect(47, 28, 52, 33));
	FillEllipse(BRect(60, 28, 65, 33));
	SetPenSize(3);
	StrokeArc(BRect(46, 30, 66, 48), 200, 140);
	SetPenSize(1);
	DrawText(this, B_TRANSLATE("Hello!"), BPoint(96, 47), Font(30), kWhite);

	// the time left today
	BString minutes(B_TRANSLATE("%minutes% min"));
	BString number;
	number << fSettings.MinutesLeft();
	minutes.ReplaceFirst("%minutes%", number.String());
	BFont font = Font(22);
	float width = font.StringWidth(minutes.String()) + 70;
	BRect pill(bounds.right - 32 - width, 14, bounds.right - 32, 58);
	FillRound(this, pill, 22, kNightLight);
	DrawClock(this, BRect(pill.left + 16, pill.top + 10, pill.left + 40,
		pill.top + 34), kSun);
	DrawText(this, minutes.String(), BPoint(pill.left + 50, pill.top + 30),
		font, kWhite);
}


BRect
LauncherView::_CategoryFrame(BRect bounds) const
{
	BString name = fCategory.IsEmpty()
		? BString(B_TRANSLATE("All games")) : fCategory;
	float width = std::max(220.0f, Font(22).StringWidth(name.String()) + 96);
	float centerX = bounds.Width() / 2;
	return BRect(centerX - width / 2, 14, centerX + width / 2, 58);
}


/*!	The console shown, in the middle of the top bar, with the arrows that
	change it.
*/
void
LauncherView::_DrawCategory(BRect bounds)
{
	if (_Categories().size() <= 2)
		return;

	BRect pill = _CategoryFrame(bounds);
	FillRound(this, pill, 22, kNightLight);
	BString name = fCategory.IsEmpty()
		? BString(B_TRANSLATE("All games")) : fCategory;
	DrawCentered(this, name.String(), pill, Font(22), kWhite);

	// up and down arrows on the sides
	SetHighColor(kSun);
	float y = pill.top + pill.Height() / 2;
	for (float x : { pill.left + 26, pill.right - 26 }) {
		FillTriangle(BPoint(x - 8, y - 3), BPoint(x + 8, y - 3),
			BPoint(x, y - 13));
		FillTriangle(BPoint(x - 8, y + 3), BPoint(x + 8, y + 3),
			BPoint(x, y + 13));
	}
}


/*!	Fills \a frame with the cover, cut to its shape. Small pictures (pixel
	art) are enlarged without smoothing, so that they stay sharp.
*/
void
LauncherView::_DrawCover(const BBitmap* cover, BRect frame)
{
	BRect source = cover->Bounds();
	float sourceAspect = (source.Width() + 1) / (source.Height() + 1);
	float frameAspect = (frame.Width() + 1) / (frame.Height() + 1);
	if (sourceAspect > frameAspect) {
		float width = (source.Height() + 1) * frameAspect;
		source.left += floorf((source.Width() + 1 - width) / 2);
		source.right = source.left + width - 1;
	} else {
		float height = (source.Width() + 1) / frameAspect;
		source.top += floorf((source.Height() + 1 - height) / 2);
		source.bottom = source.top + height - 1;
	}
	float scale = (frame.Width() + 1) / (source.Width() + 1);
	uint32 options = scale >= 1.9f ? 0 : B_FILTER_BITMAP_BILINEAR;

	// round the corners: clip to the rows of a rounded rectangle
	const float radius = 16;
	BRegion clip;
	clip.Include(frame.InsetByCopy(0, radius));
	for (float row = 0; row < radius; row++) {
		float dy = radius - row - 0.5f;
		float inset = floorf(radius - sqrtf(radius * radius - dy * dy));
		clip.Include(BRect(frame.left + inset, frame.top + row,
			frame.right - inset, frame.top + row));
		clip.Include(BRect(frame.left + inset, frame.bottom - row,
			frame.right - inset, frame.bottom - row));
	}
	ConstrainClippingRegion(&clip);
	SetDrawingMode(B_OP_COPY);
	DrawBitmap(cover, source, frame, options);
	ConstrainClippingRegion(NULL);
}


void
LauncherView::_DrawTile(int32 index, BRect frame, float closeness)
{
	const Game& game = fLibrary.GameAt(index);
	bool selected = closeness > 0.5f;
	float radius = 20 + 8 * closeness;

	if (selected) {
		FillRound(this, frame.OffsetByCopy(0, 10), radius,
			(rgb_color){ 14, 30, 51, 255 });
		FillRound(this, frame.InsetByCopy(-6, -6), radius + 6,
			(rgb_color){ 255, 201, 60, (uint8)(80 * closeness) });
		FillRound(this, frame, radius, kSun);
	} else
		FillRound(this, frame, radius, kWhite);

	BRect inner = frame.InsetByCopy(6, 6);
	if (game.cover != NULL)
		_DrawCover(game.cover, inner);
	else {
		FillRound(this, inner, radius - 6, game.color);
		float size = inner.Width() * 0.56f;
		BRect circle(inner.left + (inner.Width() - size) / 2,
			inner.top + (inner.Height() - size) / 2, 0, 0);
		circle.right = circle.left + size;
		circle.bottom = circle.top + size;
		FillRound(this, circle, size / 2, (rgb_color){ 255, 255, 255, 216 });
		char initial[2] = { game.name.ByteAt(0), 0 };
		rgb_color ink = { (uint8)(game.color.red / 3),
			(uint8)(game.color.green / 3), (uint8)(game.color.blue / 3), 255 };
		DrawCentered(this, initial, circle, Font(size * 0.58f), ink);
	}

	// the system, small in a corner
	BFont font = Font(13);
	BString system(game.system);
	font.TruncateString(&system, B_TRUNCATE_END, inner.Width() - 36);
	float width = font.StringWidth(system.String()) + 20;
	BRect badge(inner.left + 8, inner.top + 8, inner.left + 8 + width,
		inner.top + 30);
	FillRound(this, badge, 10, (rgb_color){ 0, 0, 0, 120 });
	DrawCentered(this, system.String(), badge, font, kWhite);

	// the other games stay a bit in the background
	uint8 shade = (uint8)(90 * (1 - closeness));
	if (shade > 0)
		FillRound(this, frame, radius, (rgb_color){ 23, 48, 79, shade });
}


void
LauncherView::_DrawDots(BRect bounds, float top)
{
	int32 count = _CountVisible();
	if (count < 2 || count > 24)
		return;
	const float size = 10;
	const float gap = 8;
	float width = count * size + (count - 1) * gap;
	float left = (bounds.Width() - width) / 2;
	for (int32 i = 0; i < count; i++) {
		BRect dot(left + i * (size + gap), top, left + i * (size + gap) + size,
			top + size);
		if (i == fSelected)
			FillRound(this, dot.InsetByCopy(-2, -2), 7, kSun);
		else
			FillRound(this, dot, 5, kNightLight);
	}
}


void
LauncherView::_DrawBottomBar(BRect bounds)
{
	BRect bar(0, bounds.bottom - kBottomBar + 1, bounds.right, bounds.bottom);
	SetDrawingMode(B_OP_COPY);
	SetHighColor(kNightDark);
	FillRect(bar);
	SetDrawingMode(B_OP_ALPHA);

	float centerY = bar.top + kBottomBar / 2;
	float left = bounds.Width() / 2 - 230;

	SetHighColor(kWhite);
	SetPenSize(3);
	BRect back(left, centerY - 26, left + 52, centerY + 26);
	StrokeEllipse(back.InsetByCopy(1.5, 1.5));
	DrawArrow(this, back.InsetByCopy(18, 15), false, kWhite);
	SetHighColor(kWhite);
	SetPenSize(3);
	BRect forward = back.OffsetByCopy(64, 0);
	StrokeEllipse(forward.InsetByCopy(1.5, 1.5));
	DrawArrow(this, forward.InsetByCopy(18, 15), true, kWhite);
	SetPenSize(1);
	DrawText(this, B_TRANSLATE("Choose"), BPoint(forward.right + 14,
		centerY + 9), Font(24, false), kWhite);

	BRect a(bounds.Width() / 2 + 60, centerY - 30, bounds.Width() / 2 + 120,
		centerY + 26);
	FillRound(this, a.OffsetByCopy(0, 4), 30, kGreenShadow);
	FillRound(this, a, 30, kGreen);
	DrawCentered(this, "A", a, Font(30), kWhite);
	DrawText(this, B_TRANSLATE("Play!"), BPoint(a.right + 14, centerY + 10),
		Font(30), kWhite);
}


void
LauncherView::MessageReceived(BMessage* message)
{
	if (message->what != kMsgAnimate) {
		Screen::MessageReceived(message);
		return;
	}

	float difference = fSelected - fScroll;
	if (fabsf(difference) < 0.02f) {
		fScroll = fSelected;
		delete fAnimation;
		fAnimation = NULL;
	} else
		fScroll += difference * 0.35f;
	Invalidate();
}


void
LauncherView::MouseDown(BPoint where)
{
	where = ToLayout(where);
	if (_Categories().size() > 2
		&& _CategoryFrame(LayoutBounds()).Contains(where)) {
		_ChangeCategory(where.y < _CategoryFrame(LayoutBounds()).top + 22
			? -1 : 1);
		return;
	}

	int32 count = _CountVisible();
	// the middle tile is on top: look at it first
	for (int32 away = 0; away <= 4; away++) {
		for (int32 visible = 0; visible < count; visible++) {
			float distance = visible - fScroll;
			if ((int32)roundf(fabsf(distance)) != away
				|| !_TileFrame(distance).Contains(where))
				continue;
			if (visible == fSelected)
				_Start();
			else
				_Select(visible);
			return;
		}
	}
}


void
LauncherView::HandleAction(Action action)
{
	switch (action) {
		case kActionLeft:
			_Select(fSelected - 1);
			break;
		case kActionRight:
			_Select(fSelected + 1);
			break;
		case kActionUp:
			_ChangeCategory(-1);
			break;
		case kActionDown:
			_ChangeCategory(1);
			break;
		case kActionConfirm:
			_Start();
			break;
		case kActionBack:
			break;
	}
}


void
LauncherView::_Select(int32 visible)
{
	if (visible < 0 || visible >= _CountVisible())
		return;
	fSelected = visible;
	fSounds.Move();
	if (fAnimation == NULL) {
		fAnimation = new BMessageRunner(BMessenger(this),
			new BMessage(kMsgAnimate), 16000);
	}
}


void
LauncherView::_Start()
{
	int32 index = _VisibleIndex(fSelected);
	if (index < 0)
		return;
	fSounds.Start();
	BMessage message(kMsgStartGame);
	message.AddInt32("index", index);
	Window()->PostMessage(&message);
}
