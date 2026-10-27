/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef SOUNDS_H
#define SOUNDS_H


#include <SupportDefs.h>


class BSimpleGameSound;


/*!	Short sounds of the menus, made at start: no files needed. */
class Sounds {
public:
								Sounds();
								~Sounds();

			void				SetVolume(float volume);
			void				Move();
			void				Start();
			void				Back();

private:
			BSimpleGameSound*	_Make(const float* notes, int32 count,
									float noteLength);
			void				_Play(BSimpleGameSound* sound);

			BSimpleGameSound*	fMove;
			BSimpleGameSound*	fStart;
			BSimpleGameSound*	fBack;
			float				fVolume;
};


#endif	// SOUNDS_H
