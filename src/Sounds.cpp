/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "Sounds.h"

#include <math.h>
#include <stdlib.h>

#include <MediaDefs.h>
#include <SimpleGameSound.h>


static const float kRate = 44100;


Sounds::Sounds()
	:
	fMove(NULL),
	fStart(NULL),
	fBack(NULL),
	fVolume(0.7f)
{
	static const float kMove[] = { 880 };
	static const float kStart[] = { 659.25f, 783.99f, 1046.5f };
	static const float kBack[] = { 783.99f, 523.25f };
	fMove = _Make(kMove, 1, 0.045f);
	fStart = _Make(kStart, 3, 0.08f);
	fBack = _Make(kBack, 2, 0.07f);
}


Sounds::~Sounds()
{
	delete fMove;
	delete fStart;
	delete fBack;
}


void
Sounds::SetVolume(float volume)
{
	fVolume = volume;
}


void
Sounds::Move()
{
	_Play(fMove);
}


void
Sounds::Start()
{
	_Play(fStart);
}


void
Sounds::Back()
{
	_Play(fBack);
}


/*!	A soft square-ish tone per note, each fading out, like a little chime. */
BSimpleGameSound*
Sounds::_Make(const float* notes, int32 count, float noteLength)
{
	int32 noteFrames = (int32)(kRate * noteLength);
	int32 frames = noteFrames * count;
	int16* data = (int16*)malloc(frames * sizeof(int16));
	if (data == NULL)
		return NULL;

	for (int32 note = 0; note < count; note++) {
		for (int32 i = 0; i < noteFrames; i++) {
			float t = i / kRate;
			float envelope = expf(-t * 40 / (noteLength * 20));
			float wave = sinf(2 * M_PI * notes[note] * t)
				+ 0.3f * sinf(6 * M_PI * notes[note] * t);
			data[note * noteFrames + i] = (int16)(wave * envelope * 7000);
		}
	}

	gs_audio_format format;
	format.frame_rate = kRate;
	format.channel_count = 1;
	format.format = gs_audio_format::B_GS_S16;
	format.byte_order = B_MEDIA_HOST_ENDIAN;
	format.buffer_size = 0;

	BSimpleGameSound* sound = new BSimpleGameSound(data, frames, &format);
	free(data);
	if (sound->InitCheck() != B_OK) {
		delete sound;
		return NULL;
	}
	return sound;
}


void
Sounds::_Play(BSimpleGameSound* sound)
{
	if (sound == NULL || fVolume <= 0)
		return;
	sound->StopPlaying();
	sound->SetGain(fVolume);
	sound->StartPlaying();
}
