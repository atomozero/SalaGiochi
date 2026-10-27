/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef CORE_H
#define CORE_H


#include <Bitmap.h>
#include <Locker.h>
#include <MediaDefs.h>
#include <Messenger.h>
#include <OS.h>
#include <String.h>

#include <atomic>
#include <map>
#include <string>

#include "libretro.h"


class BSoundPlayer;
class Gamepad;


/*!	Runs one libretro core with one game: the core runs on its own thread,
	paced by the audio output (or by the clock when the core makes no sound).
	Frames are converted to 32 bit; the target is sent kMsgNewFrame when one
	is ready, and draws it with LockFrame()/UnlockFrame().
*/
class Core {
public:
								Core(const BMessenger& target,
									Gamepad* gamepad);
								~Core();

			status_t			Load(const char* corePath,
									const char* gamePath,
									const char* saveBase);
			void				Start();
			void				Stop();
									// saves the game, then unloads it

			void				SetPaused(bool paused);
			bool				IsPaused() const { return fPaused; }
			void				SetVolume(float volume);

			void				SetKeyButtons(uint32 buttons)
									{ fKeyButtons = buttons; }
			// Start puts a coin in first, as arcade games want
			void				SetCoinOnStart(bool coin)
									{ fCoinOnStart = coin; }

			const BBitmap*		LockFrame(BRect& source);
			void				UnlockFrame();
			float				AspectRatio() const;

private:
	static	bool				_Environment(unsigned command, void* data);
	static	void				_VideoRefresh(const void* data,
									unsigned width, unsigned height,
									size_t pitch);
	static	void				_AudioSample(int16_t left, int16_t right);
	static	size_t				_AudioSampleBatch(const int16_t* data,
									size_t frames);
	static	void				_InputPoll();
	static	int16_t				_InputState(unsigned port, unsigned device,
									unsigned index, unsigned id);
	static	void				_PlayAudio(void* cookie, void* buffer,
									size_t size,
									const media_raw_audio_format& format);
	static	status_t			_ThreadEntry(void* data);

			bool				_HandleEnvironment(unsigned command,
									void* data);
			void				_SetVariables(const retro_variable* variables);
			void				_SetOptionsV1(
									const retro_core_option_definition*
										definitions);
			void				_SetOptionsV2(
									const retro_core_options_v2* options);
			void				_ConvertFrame(const void* data,
									unsigned width, unsigned height,
									size_t pitch);
			void				_PushAudio(const int16_t* data, size_t frames);
			void				_Run();
			void				_LoadMemory();
			void				_SaveMemory();
			void				_LoadState();
			void				_SaveState();
			void				_SetupBitmaps(unsigned width,
									unsigned height);
			void				_StartAudio();
			void				_CoinOnStart(uint32& buttons);

			BMessenger			fTarget;
			Gamepad*			fGamepad;

			void*				fHandle;
			void				(*fRun)();
			void				(*fUnloadGame)();
			void				(*fDeinit)();
			size_t				(*fSerializeSize)();
			bool				(*fSerialize)(void*, size_t);
			bool				(*fUnserialize)(const void*, size_t);
			void*				(*fGetMemoryData)(unsigned);
			size_t				(*fGetMemorySize)(unsigned);

			BString				fSaveBase;
			BString				fSystemDirectory;
			BString				fSaveDirectory;
			void*				fGameData;
			retro_system_av_info fAVInfo;
			retro_pixel_format	fPixelFormat;
			std::map<std::string, std::string> fVariables;

			BLocker				fFrameLock;
			BBitmap*			fFront;
			BBitmap*			fBack;
			BRect				fFrontSource;
			BRect				fBackSource;
			std::atomic<bool>	fFramePending;

			BSoundPlayer*		fSoundPlayer;
			int16*				fAudioRing;
			uint32				fAudioCapacity;	// in frames, power of 2
			std::atomic<uint32>	fAudioRead;
			std::atomic<uint32>	fAudioWrite;
			sem_id				fAudioSem;
			uint32				fAudioTarget;	// frames to keep queued
			float				fVolume;

			std::atomic<uint32>	fKeyButtons;
			uint32				fPolledButtons;
			bool				fCoinOnStart;
			bool				fStartWasDown;
			int32				fCoinFrames;	// of the coin sequence left

			thread_id			fThread;
			sem_id				fPauseSem;
			std::atomic<bool>	fQuit;
			std::atomic<bool>	fPaused;
			bool				fLoaded;

	static	Core*				sCurrent;
};


#endif	// CORE_H
