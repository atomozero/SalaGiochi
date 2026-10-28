/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "MainWindow.h"

#include <Application.h>
#include <Beep.h>
#include <InterfaceDefs.h>
#include <MessageRunner.h>
#include <Screen.h>
#include <StringList.h>

#include <stdio.h>
#include <unistd.h>

#include <algorithm>

#include "Core.h"
#include "GameView.h"
#include "Gamepad.h"
#include "LauncherView.h"
#include "Messages.h"
#include "OtherScreens.h"
#include "Style.h"
#include "libretro.h"


static const bigtime_t kPadInterval = 40000;
static const bigtime_t kPlayInterval = 5000000;
static const bigtime_t kParentsHold = 3000000;
static const bigtime_t kPadParentsHold = 5000000;
static const BRect kCorner(0, 0, 110, 72);
	// the sun and "Hello!", in the units of Style::kLayoutSize
static const uint32 kMsgCornerCheck = 'corn';

// raw key codes of a Haiku keyboard
enum {
	kKeyEscape = 0x01,
	kKeyBackspace = 0x1e,
	kKeyQ = 0x27,
	kKeyW = 0x28,
	kKeyEnter = 0x47,
	kKeyA = 0x3c,
	kKeyS = 0x3d,
	kKeyG = 0x3f,
	kKeyZ = 0x4c,
	kKeyX = 0x4d,
	kKeyRightShift = 0x56,
	kKeyUp = 0x57,
	kKeySpace = 0x5e,
	kKeyLeft = 0x61,
	kKeyDown = 0x62,
	kKeyRight = 0x63,
	kKeyPadEnter = 0x5b,
};


struct KeyButton {
	uint32	key;
	uint32	button;
};


// arrows move, X and Space are A, Z is B, Enter is Start
static const KeyButton kKeyButtons[] = {
	{ kKeyUp, RETRO_DEVICE_ID_JOYPAD_UP },
	{ kKeyDown, RETRO_DEVICE_ID_JOYPAD_DOWN },
	{ kKeyLeft, RETRO_DEVICE_ID_JOYPAD_LEFT },
	{ kKeyRight, RETRO_DEVICE_ID_JOYPAD_RIGHT },
	{ kKeyX, RETRO_DEVICE_ID_JOYPAD_A },
	{ kKeySpace, RETRO_DEVICE_ID_JOYPAD_A },
	{ kKeyZ, RETRO_DEVICE_ID_JOYPAD_B },
	{ kKeyS, RETRO_DEVICE_ID_JOYPAD_X },
	{ kKeyA, RETRO_DEVICE_ID_JOYPAD_Y },
	{ kKeyQ, RETRO_DEVICE_ID_JOYPAD_L },
	{ kKeyW, RETRO_DEVICE_ID_JOYPAD_R },
	{ kKeyEnter, RETRO_DEVICE_ID_JOYPAD_START },
	{ kKeyPadEnter, RETRO_DEVICE_ID_JOYPAD_START },
	{ kKeyBackspace, RETRO_DEVICE_ID_JOYPAD_SELECT },
	{ kKeyRightShift, RETRO_DEVICE_ID_JOYPAD_SELECT },
};


static inline uint32
bit(uint32 button)
{
	return 1U << button;
}


MainWindow::MainWindow()
	:
	BWindow(BScreen().Frame(), "SalaGiochi", B_NO_BORDER_WINDOW_LOOK,
		B_NORMAL_WINDOW_FEEL, B_NOT_MOVABLE | B_NOT_RESIZABLE
			| B_NOT_ZOOMABLE | B_NOT_MINIMIZABLE | B_QUIT_ON_WINDOW_CLOSE),
	fGamepad(new Gamepad),
	fCore(NULL),
	fCurrent(NULL),
	fBeforeParents(NULL),
	fPadRunner(NULL),
	fPlayRunner(NULL),
	fLastPlayTick(0),
	fLastPadButtons(0),
	fPadRepeat(0),
	fComboStart(0),
	fCornerDown(0),
	fCornerRunner(NULL),
	fPadComboStart(0),
	fNativeTeam(-1),
	fRescan(false)
{
	memset(fKeysDown, 0, sizeof(fKeysDown));

	// the children leave through the parents' area only
	RemoveShortcut('Q', B_COMMAND_KEY);
	RemoveShortcut('W', B_COMMAND_KEY);

	fSettings.Load();
	fSounds.SetVolume(fSettings.MaxVolume() / 100.0f);
	fLibrary.Scan();

	BRect bounds = Bounds();
	fLauncher = new LauncherView(fLibrary, fSettings, fSounds);
	fGameView = new GameView;
	fTimeUp = new TimeUpView;
	fParents = new ParentsView(fLibrary, fSettings);
	Screen* screens[] = { fLauncher, fGameView, fTimeUp, fParents };
	for (Screen* screen : screens) {
		screen->MoveTo(bounds.LeftTop());
		screen->ResizeTo(bounds.Width(), bounds.Height());
		screen->SetResizingMode(B_FOLLOW_ALL);
		AddChild(screen);
		screen->Hide();
	}
	fLauncher->GamesChanged();
	_Show(fSettings.MinutesLeft() > 0 ? (Screen*)fLauncher : fTimeUp);

	fPadRunner = new BMessageRunner(BMessenger(this),
		new BMessage(kMsgPollPad), kPadInterval);
}


MainWindow::~MainWindow()
{
	delete fPadRunner;
	delete fPlayRunner;
	delete fCornerRunner;
	_StopGame();
	fSettings.Save();
	delete fGamepad;
}


void
MainWindow::ScreenChanged(BRect frame, color_space mode)
{
	// another resolution, or the AC100 moved to the HDMI output: cover the
	// whole screen again, the screens follow and scale themselves
	MoveTo(frame.LeftTop());
	ResizeTo(frame.Width(), frame.Height());
}


bool
MainWindow::QuitRequested()
{
	_StopNative();
	_StopGame();
	fSettings.Save();
	return true;
}


void
MainWindow::_Show(Screen* screen)
{
	if (fCurrent == screen)
		return;
	if (fCurrent != NULL)
		fCurrent->Hide();
	fCurrent = screen;
	fCurrent->Show();
	fCurrent->MakeFocus(true);
	fCurrent->Invalidate();
}


//	#pragma mark - input


void
MainWindow::DispatchMessage(BMessage* message, BHandler* target)
{
	// the mouse or touchpad held three seconds on the sun, top left
	if (message->what == B_MOUSE_DOWN) {
		BPoint where;
		if (message->FindPoint("screen_where", &where) == B_OK) {
			where = ConvertFromScreen(where);
			float scale = Style::LayoutScale(Bounds());
			where.Set(where.x / scale, where.y / scale);
		}
		if (message->HasPoint("screen_where") && kCorner.Contains(where)) {
			fCornerDown = system_time();
			delete fCornerRunner;
			fCornerRunner = new BMessageRunner(BMessenger(this),
				new BMessage(kMsgCornerCheck), kParentsHold, 1);
		}
	} else if (message->what == B_MOUSE_UP)
		fCornerDown = 0;

	if (message->what == B_KEY_DOWN || message->what == B_UNMAPPED_KEY_DOWN) {
		if (_HandleKey(message, true))
			return;
	} else if (message->what == B_KEY_UP
		|| message->what == B_UNMAPPED_KEY_UP) {
		if (_HandleKey(message, false))
			return;
	}
	BWindow::DispatchMessage(message, target);
}


bool
MainWindow::_HandleKey(BMessage* message, bool down)
{
	int32 key = message->GetInt32("key", 0);
	int32 modifiers = message->GetInt32("modifiers", 0);
	bool repeat = message->GetInt32("be:key_repeat", 0) > 0;

	if (key >= 0 && key < 256) {
		if (down)
			fKeysDown[key / 32] |= 1U << (key % 32);
		else
			fKeysDown[key / 32] &= ~(1U << (key % 32));
	}

	// Ctrl + Alt + G held for three seconds opens the parents' area
	if (key == kKeyG && down && (modifiers & B_CONTROL_KEY) != 0
		&& (modifiers & B_COMMAND_KEY) != 0) {
		if (!repeat || fComboStart == 0)
			fComboStart = system_time();
		else if (system_time() - fComboStart >= kParentsHold) {
			fComboStart = 0;
			if (_Current() != fParents)
				_OpenParents(false);
		}
		return true;
	}
	if (key == kKeyG && !down)
		fComboStart = 0;

	bool playing = _Current() == fGameView && fCore != NULL
		&& !fGameView->IsPaused();
	if (playing) {
		if (key == kKeyEscape && down) {
			_SetPaused(true);
			return true;
		}
		_UpdateKeyButtons();
		return true;
	}

	if (!down)
		return true;

	const char* bytes = message->GetString("bytes", "");
	Action action;
	switch (bytes[0]) {
		case B_LEFT_ARROW:
			action = kActionLeft;
			break;
		case B_RIGHT_ARROW:
			action = kActionRight;
			break;
		case B_UP_ARROW:
			action = kActionUp;
			break;
		case B_DOWN_ARROW:
			action = kActionDown;
			break;
		case B_ENTER:
		case B_SPACE:
		case 'a':
		case 'A':
			// the button on the screen says "A"
		case 'x':
		case 'X':
			action = kActionConfirm;
			break;
		case B_ESCAPE:
		case 'z':
		case 'Z':
			action = kActionBack;
			break;
		default:
			return true;
	}
	if (_Current() != NULL)
		_Current()->HandleAction(action);
	return true;
}


void
MainWindow::_UpdateKeyButtons()
{
	uint32 buttons = 0;
	for (const KeyButton& map : kKeyButtons) {
		if ((fKeysDown[map.key / 32] & (1U << (map.key % 32))) != 0)
			buttons |= bit(map.button);
	}
	if (fCore != NULL)
		fCore->SetKeyButtons(buttons);
}


void
MainWindow::_PollPad()
{
	uint32 buttons = fGamepad->Buttons();
	uint32 pressed = buttons & ~fLastPadButtons;
	bool held = buttons != 0 && buttons == fLastPadButtons;
	fLastPadButtons = buttons;

	// Select and Start held five seconds open the parents' area
	uint32 both = bit(RETRO_DEVICE_ID_JOYPAD_SELECT)
		| bit(RETRO_DEVICE_ID_JOYPAD_START);
	if ((buttons & both) == both) {
		if (fPadComboStart == 0)
			fPadComboStart = system_time();
		else if (system_time() - fPadComboStart >= kPadParentsHold) {
			fPadComboStart = 0;
			if (_Current() != fParents)
				_OpenParents(true);
			fLastPadButtons = buttons;
			return;
		}
	} else
		fPadComboStart = 0;

	bool playing = _Current() == fGameView && fCore != NULL
		&& !fGameView->IsPaused();
	if (playing) {
		// Select and Start together pause the game
		uint32 pause = bit(RETRO_DEVICE_ID_JOYPAD_SELECT)
			| bit(RETRO_DEVICE_ID_JOYPAD_START);
		if ((buttons & pause) == pause && (pressed & pause) != 0)
			_SetPaused(true);
		return;
	}

	// in the menus, a button held down repeats after a moment
	if (pressed != 0)
		fPadRepeat = system_time() + 400000;
	else if (held && system_time() > fPadRepeat) {
		pressed = buttons & (bit(RETRO_DEVICE_ID_JOYPAD_LEFT)
			| bit(RETRO_DEVICE_ID_JOYPAD_RIGHT)
			| bit(RETRO_DEVICE_ID_JOYPAD_UP)
			| bit(RETRO_DEVICE_ID_JOYPAD_DOWN));
		fPadRepeat = system_time() + 150000;
	}
	if (pressed == 0 || _Current() == NULL)
		return;

	if (pressed & bit(RETRO_DEVICE_ID_JOYPAD_LEFT))
		_Current()->HandleAction(kActionLeft);
	if (pressed & bit(RETRO_DEVICE_ID_JOYPAD_RIGHT))
		_Current()->HandleAction(kActionRight);
	if (pressed & bit(RETRO_DEVICE_ID_JOYPAD_UP))
		_Current()->HandleAction(kActionUp);
	if (pressed & bit(RETRO_DEVICE_ID_JOYPAD_DOWN))
		_Current()->HandleAction(kActionDown);
	if (pressed & (bit(RETRO_DEVICE_ID_JOYPAD_A)
			| bit(RETRO_DEVICE_ID_JOYPAD_START)))
		_Current()->HandleAction(kActionConfirm);
	else if (pressed & bit(RETRO_DEVICE_ID_JOYPAD_B))
		_Current()->HandleAction(kActionBack);
}


//	#pragma mark - games


void
MainWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgNewFrame:
			if (_Current() == fGameView)
				fGameView->FrameReady();
			break;
		case kMsgPollPad:
			_PollPad();
			break;
		case kMsgPlayTick:
			_PlayTick();
			break;
		case kMsgStartGame:
			_StartGame(message->GetInt32("index", -1));
			break;
		case kMsgPause:
			_SetPaused(true);
			break;
		case kMsgResume:
			_SetPaused(false);
			break;
		case kMsgQuitGame:
			fSounds.Back();
			_StopGame();
			_Rescan();
			_Show(fLauncher);
			break;
		case kMsgOpenParents:
			_OpenParents(true);
			break;
		case kMsgCornerCheck:
			if (fCornerDown != 0
				&& system_time() - fCornerDown >= kParentsHold - 100000
				&& _Current() != fParents)
				_OpenParents(true);
			fCornerDown = 0;
			break;
		case kMsgCloseParents:
			_CloseParents();
			break;
		case kMsgSettingsChanged:
			fSettings.Save();
			fLauncher->GamesChanged();
			fSounds.SetVolume(fSettings.MaxVolume() / 100.0f);
			if (fCore != NULL)
				fCore->SetVolume(fSettings.MaxVolume() / 100.0f);
			break;
		case kMsgNativeEnded:
			fNativeTeam = -1;
			delete fPlayRunner;
			fPlayRunner = NULL;
			fSettings.Save();
			_Rescan();
			if (IsHidden())
				Show();
			_Show(fSettings.MinutesLeft() > 0 ? (Screen*)fLauncher : fTimeUp);
			Activate();
			break;
		case kMsgRescan:
			fRescan = true;
			_Rescan();
			break;
		case kMsgExitApp:
			_StopGame();
			fSettings.Save();
			be_app->PostMessage(B_QUIT_REQUESTED);
			break;
		default:
			BWindow::MessageReceived(message);
	}
}


void
MainWindow::_StartGame(int32 index)
{
	if (index < 0 || index >= fLibrary.CountGames() || fCore != NULL)
		return;
	if (fSettings.MinutesLeft() <= 0) {
		_Show(fTimeUp);
		return;
	}

	const Game& game = fLibrary.GameAt(index);
	if (game.native) {
		if (!_StartNative(game.path.String()))
			beep();
		return;
	}

	fCore = new Core(BMessenger(this), fGamepad);
	if (fCore->Load(game.core.String(), game.path.String(),
			game.saveBase.String()) != B_OK) {
		delete fCore;
		fCore = NULL;
		beep();
		return;
	}
	fCore->SetVolume(fSettings.MaxVolume() / 100.0f);
	fCore->SetCoinOnStart(game.arcade);
	memset(fKeysDown, 0, sizeof(fKeysDown));
	fCore->SetKeyButtons(0);

	fGameView->SetCore(fCore);
	_Show(fGameView);
	be_app->ObscureCursor();
	fCore->Start();

	fLastPlayTick = system_time();
	delete fPlayRunner;
	fPlayRunner = new BMessageRunner(BMessenger(this),
		new BMessage(kMsgPlayTick), kPlayInterval);
}


/*!	Starts the program a .gioco file names, with its arguments, and hides
	while it runs; the time played counts as for the other games.
*/
bool
MainWindow::_StartNative(const char* path)
{
	FILE* file = fopen(path, "r");
	if (file == NULL)
		return false;
	BStringList arguments;
	char line[B_PATH_NAME_LENGTH];
	while (fgets(line, sizeof(line), file) != NULL) {
		BString argument(line);
		argument.RemoveAll("\n").RemoveAll("\r").Trim();
		if (!argument.IsEmpty() && argument.ByteAt(0) != '#')
			arguments.Add(argument);
	}
	fclose(file);
	if (arguments.IsEmpty())
		return false;

	const char* argv[64];
	int32 count = std::min<int32>(arguments.CountStrings(), 63);
	for (int32 i = 0; i < count; i++)
		argv[i] = arguments.StringAt(i).String();
	argv[count] = NULL;

	fNativeTeam = load_image(count, argv, (const char**)environ);
	if (fNativeTeam < 0) {
		fNativeTeam = -1;
		return false;
	}
	resume_thread(fNativeTeam);
	thread_id waiter = spawn_thread(_WaitForNative, "wait for game",
		B_NORMAL_PRIORITY, this);
	resume_thread(waiter);

	fLastPlayTick = system_time();
	delete fPlayRunner;
	fPlayRunner = new BMessageRunner(BMessenger(this),
		new BMessage(kMsgPlayTick), kPlayInterval);
	Hide();
	return true;
}


status_t
MainWindow::_WaitForNative(void* data)
{
	MainWindow* window = (MainWindow*)data;
	status_t result;
	// the main thread of a team has the team's id
	wait_for_thread(window->fNativeTeam, &result);
	BMessenger(window).SendMessage(kMsgNativeEnded);
	return B_OK;
}


void
MainWindow::_StopNative()
{
	if (fNativeTeam < 0)
		return;
	BMessenger game(NULL, fNativeTeam);
	game.SendMessage(B_QUIT_REQUESTED);
	// ask nicely, then insist
	for (int32 i = 0; i < 20; i++) {
		team_info info;
		if (get_team_info(fNativeTeam, &info) != B_OK)
			return;
		snooze(100000);
	}
	kill_team(fNativeTeam);
}


void
MainWindow::_StopGame()
{
	delete fPlayRunner;
	fPlayRunner = NULL;
	if (fCore == NULL)
		return;

	fGameView->SetCore(NULL);
	fCore->Stop();
	delete fCore;
	fCore = NULL;
	fSettings.Save();
}


void
MainWindow::_SetPaused(bool paused)
{
	if (fCore == NULL)
		return;
	fCore->SetPaused(paused);
	fGameView->SetPaused(paused);
	if (paused)
		fSounds.Back();
	if (!paused) {
		memset(fKeysDown, 0, sizeof(fKeysDown));
		fCore->SetKeyButtons(0);
		fLastPlayTick = system_time();
		be_app->ObscureCursor();
	}
}


void
MainWindow::_PlayTick()
{
	bigtime_t now = system_time();
	if ((fCore != NULL && !fCore->IsPaused() && _Current() == fGameView)
		|| fNativeTeam >= 0)
		fSettings.AddPlayed((now - fLastPlayTick) / 60000000.0f);
	fLastPlayTick = now;

	if (fNativeTeam >= 0 && fSettings.MinutesLeft() <= 0) {
		_StopNative();
		return;
	}

	if (fCore != NULL && fSettings.MinutesLeft() <= 0) {
		_StopGame();
		_Show(fTimeUp);
	}
}


//	#pragma mark - parents


void
MainWindow::_OpenParents(bool askQuestion)
{
	if (fCore != NULL && !fGameView->IsPaused())
		_SetPaused(true);
	fBeforeParents = _Current();
	fParents->Reset(askQuestion);
	_Show(fParents);
}


/*!	Reads the games again when the installer added some, but not while one
	is running: the game has its entry in the library.
*/
void
MainWindow::_Rescan()
{
	if (!fRescan || fCore != NULL || fNativeTeam >= 0)
		return;
	fRescan = false;
	fLibrary.Scan();
	fLauncher->GamesChanged();
	fCurrent->Invalidate();
}


void
MainWindow::_CloseParents()
{
	fSettings.Save();
	fLauncher->GamesChanged();

	Screen* next = fBeforeParents;
	if (next == NULL || next == fParents)
		next = fLauncher;
	if (next == fTimeUp && fSettings.MinutesLeft() > 0)
		next = fLauncher;
	if (next == fLauncher && fSettings.MinutesLeft() <= 0)
		next = fTimeUp;
	_Show(next);
}
