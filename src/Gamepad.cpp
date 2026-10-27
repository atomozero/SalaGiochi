/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "Gamepad.h"

#include <Joystick.h>

#include "libretro.h"


static const int16 kAxisThreshold = 16000;

// Generic USB pads differ; this order fits most cheap SNES style ones.
static const uint32 kButtonMap[] = {
	RETRO_DEVICE_ID_JOYPAD_A,
	RETRO_DEVICE_ID_JOYPAD_B,
	RETRO_DEVICE_ID_JOYPAD_X,
	RETRO_DEVICE_ID_JOYPAD_Y,
	RETRO_DEVICE_ID_JOYPAD_L,
	RETRO_DEVICE_ID_JOYPAD_R,
	RETRO_DEVICE_ID_JOYPAD_SELECT,
	RETRO_DEVICE_ID_JOYPAD_START,
	RETRO_DEVICE_ID_JOYPAD_SELECT,
	RETRO_DEVICE_ID_JOYPAD_START,
};


Gamepad::Gamepad()
	:
	fJoystick(NULL),
	fButtons(0),
	fQuit(false),
	fThread(-1)
{
	fThread = spawn_thread(_ThreadEntry, "gamepad", B_DISPLAY_PRIORITY,
		this);
	if (fThread >= 0)
		resume_thread(fThread);
}


Gamepad::~Gamepad()
{
	fQuit = true;
	if (fThread >= 0) {
		status_t result;
		wait_for_thread(fThread, &result);
	}
	delete fJoystick;
}


status_t
Gamepad::_ThreadEntry(void* data)
{
	((Gamepad*)data)->_Run();
	return B_OK;
}


bool
Gamepad::_Open()
{
	BJoystick* joystick = new BJoystick;
	int32 count = joystick->CountDevices();
	for (int32 i = 0; i < count; i++) {
		char name[B_OS_NAME_LENGTH];
		if (joystick->GetDeviceName(i, name, sizeof(name)) != B_OK)
			continue;
		if (joystick->Open(name, true) >= B_OK) {
			fJoystick = joystick;
			return true;
		}
	}
	delete joystick;
	return false;
}


void
Gamepad::_Run()
{
	bigtime_t lastLook = 0;
	while (!fQuit) {
		if (fJoystick == NULL) {
			// look for a controller plugged in now and then
			fButtons = 0;
			if (system_time() - lastLook > 2000000) {
				lastLook = system_time();
				if (_Open())
					continue;
			}
			snooze(100000);
			continue;
		}

		if (fJoystick->Update() != B_OK) {
			delete fJoystick;
			fJoystick = NULL;
			continue;
		}

		uint32 buttons = 0;
		uint32 values = fJoystick->ButtonValues();
		int32 buttonCount = fJoystick->CountButtons();
		for (int32 i = 0; i < buttonCount
				&& i < (int32)(sizeof(kButtonMap) / sizeof(kButtonMap[0]));
				i++) {
			if ((values & (1U << i)) != 0)
				buttons |= 1U << kButtonMap[i];
		}

		int16 axes[8] = {};
		if (fJoystick->CountAxes() >= 2 && fJoystick->GetAxisValues(axes) == B_OK) {
			if (axes[0] < -kAxisThreshold)
				buttons |= 1U << RETRO_DEVICE_ID_JOYPAD_LEFT;
			else if (axes[0] > kAxisThreshold)
				buttons |= 1U << RETRO_DEVICE_ID_JOYPAD_RIGHT;
			if (axes[1] < -kAxisThreshold)
				buttons |= 1U << RETRO_DEVICE_ID_JOYPAD_UP;
			else if (axes[1] > kAxisThreshold)
				buttons |= 1U << RETRO_DEVICE_ID_JOYPAD_DOWN;
		}

		uint8 hats[4] = {};
		if (fJoystick->CountHats() > 0 && fJoystick->GetHatValues(hats) == B_OK) {
			// 1 is up, then clockwise in steps of 45 degrees
			switch (hats[0]) {
				case 1: buttons |= 1U << RETRO_DEVICE_ID_JOYPAD_UP; break;
				case 2: buttons |= (1U << RETRO_DEVICE_ID_JOYPAD_UP)
					| (1U << RETRO_DEVICE_ID_JOYPAD_RIGHT); break;
				case 3: buttons |= 1U << RETRO_DEVICE_ID_JOYPAD_RIGHT; break;
				case 4: buttons |= (1U << RETRO_DEVICE_ID_JOYPAD_DOWN)
					| (1U << RETRO_DEVICE_ID_JOYPAD_RIGHT); break;
				case 5: buttons |= 1U << RETRO_DEVICE_ID_JOYPAD_DOWN; break;
				case 6: buttons |= (1U << RETRO_DEVICE_ID_JOYPAD_DOWN)
					| (1U << RETRO_DEVICE_ID_JOYPAD_LEFT); break;
				case 7: buttons |= 1U << RETRO_DEVICE_ID_JOYPAD_LEFT; break;
				case 8: buttons |= (1U << RETRO_DEVICE_ID_JOYPAD_UP)
					| (1U << RETRO_DEVICE_ID_JOYPAD_LEFT); break;
			}
		}

		fButtons = buttons;
		snooze(8000);
	}
}
