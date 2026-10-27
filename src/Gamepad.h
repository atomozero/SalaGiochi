/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef GAMEPAD_H
#define GAMEPAD_H


#include <OS.h>

#include <atomic>


class BJoystick;


/*!	The first game controller found, read on a thread of its own: Buttons()
	returns its state as a mask of RETRO_DEVICE_ID_JOYPAD_* bits.
*/
class Gamepad {
public:
								Gamepad();
								~Gamepad();

			uint32				Buttons() const { return fButtons; }

private:
	static	status_t			_ThreadEntry(void* data);
			void				_Run();
			bool				_Open();

			BJoystick*			fJoystick;
			std::atomic<uint32>	fButtons;
			std::atomic<bool>	fQuit;
			thread_id			fThread;
};


#endif	// GAMEPAD_H
