/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "Core.h"

#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Autolock.h>
#include <File.h>
#include <SoundPlayer.h>

#include "Gamepad.h"
#include "Messages.h"


Core* Core::sCurrent = NULL;


static void
log_printf(enum retro_log_level level, const char* format, ...)
{
	if (level < RETRO_LOG_WARN)
		return;
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}


template<typename Function>
static Function
core_symbol(void* handle, const char* name)
{
	return (Function)dlsym(handle, name);
}


static bool
read_file(const char* path, void** data, size_t* size)
{
	BFile file(path, B_READ_ONLY);
	off_t fileSize;
	if (file.InitCheck() != B_OK || file.GetSize(&fileSize) != B_OK
		|| fileSize <= 0)
		return false;
	*data = malloc(fileSize);
	if (*data == NULL)
		return false;
	if (file.Read(*data, fileSize) != fileSize) {
		free(*data);
		*data = NULL;
		return false;
	}
	*size = fileSize;
	return true;
}


static bool
write_file(const char* path, const void* data, size_t size)
{
	// write a new file and move it over the old one, so that a crash or a
	// power cut can not leave a truncated save behind
	BString temp(path);
	temp << ".tmp";
	BFile file(temp.String(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK
		|| file.Write(data, size) != (ssize_t)size || file.Sync() != B_OK) {
		unlink(temp.String());
		return false;
	}
	file.Unset();
	return rename(temp.String(), path) == 0;
}


//	#pragma mark -


Core::Core(const BMessenger& target, Gamepad* gamepad)
	:
	fTarget(target),
	fGamepad(gamepad),
	fHandle(NULL),
	fRun(NULL),
	fUnloadGame(NULL),
	fDeinit(NULL),
	fSerializeSize(NULL),
	fSerialize(NULL),
	fUnserialize(NULL),
	fGetMemoryData(NULL),
	fGetMemorySize(NULL),
	fGameData(NULL),
	fPixelFormat(RETRO_PIXEL_FORMAT_0RGB1555),
	fFrameLock("frame"),
	fFront(NULL),
	fBack(NULL),
	fFramePending(false),
	fSoundPlayer(NULL),
	fAudioRing(NULL),
	fAudioCapacity(0),
	fAudioRead(0),
	fAudioWrite(0),
	fAudioSem(-1),
	fAudioTarget(0),
	fVolume(1.0f),
	fKeyButtons(0),
	fPolledButtons(0),
	fCoinOnStart(false),
	fStartWasDown(false),
	fCoinFrames(0),
	fThread(-1),
	fPauseSem(-1),
	fQuit(false),
	fPaused(false),
	fLoaded(false)
{
	memset(&fAVInfo, 0, sizeof(fAVInfo));
}


Core::~Core()
{
	Stop();
}


status_t
Core::Load(const char* corePath, const char* gamePath, const char* saveBase)
{
	if (sCurrent != NULL)
		return B_BUSY;

	fHandle = dlopen(corePath, RTLD_NOW | RTLD_LOCAL);
	if (fHandle == NULL) {
		fprintf(stderr, "SalaGiochi: %s: %s\n", corePath, dlerror());
		return B_ERROR;
	}

	fRun = core_symbol<void (*)()>(fHandle, "retro_run");
	fUnloadGame = core_symbol<void (*)()>(fHandle, "retro_unload_game");
	fDeinit = core_symbol<void (*)()>(fHandle, "retro_deinit");
	fSerializeSize = core_symbol<size_t (*)()>(fHandle,
		"retro_serialize_size");
	fSerialize = core_symbol<bool (*)(void*, size_t)>(fHandle,
		"retro_serialize");
	fUnserialize = core_symbol<bool (*)(const void*, size_t)>(fHandle,
		"retro_unserialize");
	fGetMemoryData = core_symbol<void* (*)(unsigned)>(fHandle,
		"retro_get_memory_data");
	fGetMemorySize = core_symbol<size_t (*)(unsigned)>(fHandle,
		"retro_get_memory_size");

	auto setEnvironment = core_symbol<void (*)(retro_environment_t)>(fHandle,
		"retro_set_environment");
	auto setVideo = core_symbol<void (*)(retro_video_refresh_t)>(fHandle,
		"retro_set_video_refresh");
	auto setAudio = core_symbol<void (*)(retro_audio_sample_t)>(fHandle,
		"retro_set_audio_sample");
	auto setAudioBatch = core_symbol<void (*)(retro_audio_sample_batch_t)>(
		fHandle, "retro_set_audio_sample_batch");
	auto setInputPoll = core_symbol<void (*)(retro_input_poll_t)>(fHandle,
		"retro_set_input_poll");
	auto setInputState = core_symbol<void (*)(retro_input_state_t)>(fHandle,
		"retro_set_input_state");
	auto init = core_symbol<void (*)()>(fHandle, "retro_init");
	auto getSystemInfo = core_symbol<void (*)(retro_system_info*)>(fHandle,
		"retro_get_system_info");
	auto getAVInfo = core_symbol<void (*)(retro_system_av_info*)>(fHandle,
		"retro_get_system_av_info");
	auto loadGame = core_symbol<bool (*)(const retro_game_info*)>(fHandle,
		"retro_load_game");

	if (fRun == NULL || fUnloadGame == NULL || fDeinit == NULL
		|| setEnvironment == NULL || setVideo == NULL || setAudio == NULL
		|| setAudioBatch == NULL || setInputPoll == NULL
		|| setInputState == NULL || init == NULL || getSystemInfo == NULL
		|| getAVInfo == NULL || loadGame == NULL) {
		fprintf(stderr, "SalaGiochi: %s is not a libretro core\n", corePath);
		dlclose(fHandle);
		fHandle = NULL;
		return B_ERROR;
	}

	sCurrent = this;
	fSaveBase = saveBase;
	fSaveDirectory = saveBase;
	int32 slash = fSaveDirectory.FindLast('/');
	if (slash > 0)
		fSaveDirectory.Truncate(slash);
	fSystemDirectory = fSaveDirectory;

	setEnvironment(_Environment);
	setVideo(_VideoRefresh);
	setAudio(_AudioSample);
	setAudioBatch(_AudioSampleBatch);
	setInputPoll(_InputPoll);
	setInputState(_InputState);
	init();

	retro_system_info system;
	memset(&system, 0, sizeof(system));
	getSystemInfo(&system);

	retro_game_info game;
	memset(&game, 0, sizeof(game));
	game.path = gamePath;
	if (!system.need_fullpath) {
		size_t size = 0;
		if (!read_file(gamePath, &fGameData, &size)) {
			fprintf(stderr, "SalaGiochi: can't read %s\n", gamePath);
			fDeinit();
			dlclose(fHandle);
			fHandle = NULL;
			sCurrent = NULL;
			return B_ERROR;
		}
		game.data = fGameData;
		game.size = size;
	}

	if (!loadGame(&game)) {
		fprintf(stderr, "SalaGiochi: the core did not load %s\n", gamePath);
		fDeinit();
		dlclose(fHandle);
		fHandle = NULL;
		free(fGameData);
		fGameData = NULL;
		sCurrent = NULL;
		return B_ERROR;
	}

	getAVInfo(&fAVInfo);
	if (fAVInfo.timing.fps <= 0)
		fAVInfo.timing.fps = 60;
	_SetupBitmaps(fAVInfo.geometry.max_width, fAVInfo.geometry.max_height);

	_LoadMemory();
	_LoadState();
	fLoaded = true;
	return B_OK;
}


void
Core::Start()
{
	if (!fLoaded || fThread >= 0)
		return;

	fQuit = false;
	fPaused = false;
	fPauseSem = create_sem(0, "game pause");
	_StartAudio();

	fThread = spawn_thread(_ThreadEntry, "game", B_DISPLAY_PRIORITY, this);
	if (fThread >= 0)
		resume_thread(fThread);
}


void
Core::Stop()
{
	if (fThread >= 0) {
		fQuit = true;
		release_sem(fPauseSem);
		release_sem(fAudioSem);
		status_t result;
		wait_for_thread(fThread, &result);
		fThread = -1;
	}

	if (fSoundPlayer != NULL) {
		fSoundPlayer->Stop();
		delete fSoundPlayer;
		fSoundPlayer = NULL;
	}
	if (fAudioSem >= 0) {
		delete_sem(fAudioSem);
		fAudioSem = -1;
	}
	if (fPauseSem >= 0) {
		delete_sem(fPauseSem);
		fPauseSem = -1;
	}

	if (fLoaded) {
		_SaveState();
		_SaveMemory();
		fUnloadGame();
		fLoaded = false;
	}
	if (fHandle != NULL) {
		fDeinit();
		dlclose(fHandle);
		fHandle = NULL;
	}
	if (sCurrent == this)
		sCurrent = NULL;

	free(fGameData);
	fGameData = NULL;
	free(fAudioRing);
	fAudioRing = NULL;

	BAutolock _(fFrameLock);
	delete fFront;
	delete fBack;
	fFront = fBack = NULL;
}


void
Core::SetPaused(bool paused)
{
	if (fPaused == paused)
		return;
	fPaused = paused;
	if (fSoundPlayer != NULL)
		fSoundPlayer->SetHasData(!paused);
	if (!paused)
		release_sem(fPauseSem);
}


void
Core::SetVolume(float volume)
{
	fVolume = volume;
	if (fSoundPlayer != NULL)
		fSoundPlayer->SetVolume(volume);
}


const BBitmap*
Core::LockFrame(BRect& source)
{
	fFrameLock.Lock();
	fFramePending = false;
	source = fFrontSource;
	return fFront;
}


void
Core::UnlockFrame()
{
	fFrameLock.Unlock();
}


float
Core::AspectRatio() const
{
	if (fAVInfo.geometry.aspect_ratio > 0)
		return fAVInfo.geometry.aspect_ratio;
	if (fAVInfo.geometry.base_height == 0)
		return 4.0f / 3;
	return (float)fAVInfo.geometry.base_width / fAVInfo.geometry.base_height;
}


//	#pragma mark - callbacks


bool
Core::_Environment(unsigned command, void* data)
{
	return sCurrent != NULL && sCurrent->_HandleEnvironment(command, data);
}


bool
Core::_HandleEnvironment(unsigned command, void* data)
{
	switch (command) {
		case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
		{
			retro_pixel_format format = *(const retro_pixel_format*)data;
			if (format > RETRO_PIXEL_FORMAT_RGB565)
				return false;
			fPixelFormat = format;
			return true;
		}
		case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
			((retro_log_callback*)data)->log = log_printf;
			return true;
		case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
			*(const char**)data = fSystemDirectory.String();
			return true;
		case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
			*(const char**)data = fSaveDirectory.String();
			return true;
		case RETRO_ENVIRONMENT_GET_CAN_DUPE:
			*(bool*)data = true;
			return true;
		case RETRO_ENVIRONMENT_GET_LANGUAGE:
			*(unsigned*)data = RETRO_LANGUAGE_ITALIAN;
			return true;
		case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
			return true;
		case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
			if (data != NULL)
				*(int*)data = 3;
			return true;
		case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
			*(bool*)data = false;
			return true;
		case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
			*(unsigned*)data = 2;
			return true;
		case RETRO_ENVIRONMENT_SET_VARIABLES:
			_SetVariables((const retro_variable*)data);
			return true;
		case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
			_SetOptionsV1((const retro_core_option_definition*)data);
			return true;
		case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
			_SetOptionsV1(((const retro_core_options_intl*)data)->us);
			return true;
		case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
			_SetOptionsV2((const retro_core_options_v2*)data);
			return true;
		case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
			_SetOptionsV2(((const retro_core_options_v2_intl*)data)->us);
			return true;
		case RETRO_ENVIRONMENT_GET_VARIABLE:
		{
			retro_variable* variable = (retro_variable*)data;
			auto found = fVariables.find(variable->key);
			if (found == fVariables.end()) {
				variable->value = NULL;
				return false;
			}
			variable->value = found->second.c_str();
			return true;
		}
		case RETRO_ENVIRONMENT_SET_GEOMETRY:
		{
			const retro_game_geometry* geometry
				= (const retro_game_geometry*)data;
			fAVInfo.geometry.base_width = geometry->base_width;
			fAVInfo.geometry.base_height = geometry->base_height;
			fAVInfo.geometry.aspect_ratio = geometry->aspect_ratio;
			return true;
		}
		case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
		{
			const retro_system_av_info* info
				= (const retro_system_av_info*)data;
			fAVInfo.geometry = info->geometry;
			if (info->geometry.max_width > fAVInfo.geometry.max_width
				|| info->geometry.max_height > fAVInfo.geometry.max_height)
				_SetupBitmaps(info->geometry.max_width,
					info->geometry.max_height);
			return true;
		}
		case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
		case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
		case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
		case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
		case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
		case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
		case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
			return true;
		default:
			return false;
	}
}


void
Core::_SetVariables(const retro_variable* variables)
{
	// "Description; first|second|...": the first value is the default
	for (; variables != NULL && variables->key != NULL; variables++) {
		if (variables->value == NULL)
			continue;
		const char* values = strstr(variables->value, "; ");
		if (values == NULL)
			continue;
		values += 2;
		const char* end = strchr(values, '|');
		fVariables[variables->key] = end != NULL
			? std::string(values, end - values) : std::string(values);
	}
}


void
Core::_SetOptionsV1(const retro_core_option_definition* definitions)
{
	for (; definitions != NULL && definitions->key != NULL; definitions++) {
		const char* value = definitions->default_value;
		if (value == NULL)
			value = definitions->values[0].value;
		if (value != NULL)
			fVariables[definitions->key] = value;
	}
}


void
Core::_SetOptionsV2(const retro_core_options_v2* options)
{
	if (options == NULL)
		return;
	const retro_core_option_v2_definition* definitions = options->definitions;
	for (; definitions != NULL && definitions->key != NULL; definitions++) {
		const char* value = definitions->default_value;
		if (value == NULL)
			value = definitions->values[0].value;
		if (value != NULL)
			fVariables[definitions->key] = value;
	}
}


void
Core::_VideoRefresh(const void* data, unsigned width, unsigned height,
	size_t pitch)
{
	if (sCurrent != NULL && data != NULL)
		sCurrent->_ConvertFrame(data, width, height, pitch);
}


void
Core::_ConvertFrame(const void* data, unsigned width, unsigned height,
	size_t pitch)
{
	if (fBack == NULL)
		return;
	BRect bounds = fBack->Bounds();
	if (width > (unsigned)bounds.IntegerWidth() + 1
		|| height > (unsigned)bounds.IntegerHeight() + 1)
		_SetupBitmaps(width, height);

	uint8* bits = (uint8*)fBack->Bits();
	int32 bytesPerRow = fBack->BytesPerRow();
	for (unsigned y = 0; y < height; y++) {
		const uint8* line = (const uint8*)data + y * pitch;
		uint32* out = (uint32*)(bits + y * bytesPerRow);
		if (fPixelFormat == RETRO_PIXEL_FORMAT_XRGB8888) {
			memcpy(out, line, width * 4);
		} else if (fPixelFormat == RETRO_PIXEL_FORMAT_RGB565) {
			const uint16* in = (const uint16*)line;
			for (unsigned x = 0; x < width; x++) {
				uint32 p = in[x];
				out[x] = 0xff000000 | ((p & 0xf800) << 8) | ((p & 0xe000) << 3)
					| ((p & 0x07e0) << 5) | ((p & 0x0600) >> 1)
					| ((p & 0x001f) << 3) | ((p & 0x001c) >> 2);
			}
		} else {
			const uint16* in = (const uint16*)line;
			for (unsigned x = 0; x < width; x++) {
				uint32 p = in[x];
				out[x] = 0xff000000 | ((p & 0x7c00) << 9) | ((p & 0x7000) << 4)
					| ((p & 0x03e0) << 6) | ((p & 0x0380) << 1)
					| ((p & 0x001f) << 3) | ((p & 0x001c) >> 2);
			}
		}
	}

	fFrameLock.Lock();
	std::swap(fFront, fBack);
	fFrontSource.Set(0, 0, width - 1, height - 1);
	fFrameLock.Unlock();

	if (!fFramePending.exchange(true))
		fTarget.SendMessage(kMsgNewFrame);
}


void
Core::_SetupBitmaps(unsigned width, unsigned height)
{
	if (width == 0 || height == 0) {
		width = 640;
		height = 480;
	}
	BRect bounds(0, 0, width - 1, height - 1);
	BBitmap* front = new BBitmap(bounds, B_RGB32);
	BBitmap* back = new BBitmap(bounds, B_RGB32);
	memset(front->Bits(), 0, front->BitsLength());
	memset(back->Bits(), 0, back->BitsLength());

	BAutolock _(fFrameLock);
	delete fFront;
	delete fBack;
	fFront = front;
	fBack = back;
	fFrontSource.Set(0, 0, 0, 0);
}


void
Core::_AudioSample(int16_t left, int16_t right)
{
	if (sCurrent != NULL) {
		int16_t frame[2] = { left, right };
		sCurrent->_PushAudio(frame, 1);
	}
}


size_t
Core::_AudioSampleBatch(const int16_t* data, size_t frames)
{
	if (sCurrent != NULL)
		sCurrent->_PushAudio(data, frames);
	return frames;
}


void
Core::_PushAudio(const int16_t* data, size_t frames)
{
	if (fAudioRing == NULL)
		return;
	uint32 write = fAudioWrite.load(std::memory_order_relaxed);
	uint32 read = fAudioRead.load(std::memory_order_acquire);
	uint32 room = fAudioCapacity - (write - read);
	if (frames > room)
		frames = room;
	for (size_t i = 0; i < frames; i++) {
		uint32 index = (write + i) & (fAudioCapacity - 1);
		fAudioRing[index * 2] = data[i * 2];
		fAudioRing[index * 2 + 1] = data[i * 2 + 1];
	}
	fAudioWrite.store(write + frames, std::memory_order_release);
}


void
Core::_PlayAudio(void* cookie, void* buffer, size_t size,
	const media_raw_audio_format& format)
{
	Core* core = (Core*)cookie;
	int16* out = (int16*)buffer;
	uint32 frames = size / 4;

	uint32 read = core->fAudioRead.load(std::memory_order_relaxed);
	uint32 write = core->fAudioWrite.load(std::memory_order_acquire);
	uint32 available = write - read;
	uint32 count = available < frames ? available : frames;
	for (uint32 i = 0; i < count; i++) {
		uint32 index = (read + i) & (core->fAudioCapacity - 1);
		out[i * 2] = core->fAudioRing[index * 2];
		out[i * 2 + 1] = core->fAudioRing[index * 2 + 1];
	}
	if (count < frames)
		memset(out + count * 2, 0, (frames - count) * 4);
	core->fAudioRead.store(read + count, std::memory_order_release);

	release_sem_etc(core->fAudioSem, 1, B_DO_NOT_RESCHEDULE);
}


void
Core::_StartAudio()
{
	fAudioSem = create_sem(0, "game audio");
	double rate = fAVInfo.timing.sample_rate;
	if (rate <= 0)
		return;

	fAudioCapacity = 16384;
	fAudioRing = (int16*)calloc(fAudioCapacity * 2, sizeof(int16));
	fAudioRead = 0;
	fAudioWrite = 0;
	// keep about three frames queued: enough against hiccups, short enough
	// for the sound to follow the game
	fAudioTarget = (uint32)(rate * 3 / fAVInfo.timing.fps);

	media_raw_audio_format format = media_raw_audio_format::wildcard;
	format.frame_rate = rate;
	format.channel_count = 2;
	format.format = media_raw_audio_format::B_AUDIO_SHORT;
	format.byte_order = B_MEDIA_HOST_ENDIAN;
	format.buffer_size = 1024 * 4;

	fSoundPlayer = new BSoundPlayer(&format, "SalaGiochi", _PlayAudio, NULL,
		this);
	if (fSoundPlayer->InitCheck() != B_OK) {
		delete fSoundPlayer;
		fSoundPlayer = NULL;
		free(fAudioRing);
		fAudioRing = NULL;
		return;
	}
	fSoundPlayer->SetVolume(fVolume);
	fSoundPlayer->Start();
	fSoundPlayer->SetHasData(true);
}


void
Core::_InputPoll()
{
	if (sCurrent == NULL)
		return;
	uint32 buttons = sCurrent->fKeyButtons;
	if (sCurrent->fGamepad != NULL)
		buttons |= sCurrent->fGamepad->Buttons();
	sCurrent->_CoinOnStart(buttons);
	sCurrent->fPolledButtons = buttons;
}


int16_t
Core::_InputState(unsigned port, unsigned device, unsigned index, unsigned id)
{
	if (sCurrent == NULL || port != 0
		|| (device & RETRO_DEVICE_MASK) != RETRO_DEVICE_JOYPAD)
		return 0;
	uint32 buttons = sCurrent->fPolledButtons;
	if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
		return (int16_t)(buttons & 0xffff);
	if (id > 15)
		return 0;
	return (buttons >> id) & 1;
}


/*!	Turns a press of Start into: the coin (Select) for a few frames, a
	pause, then Start, for arcade games.
*/
void
Core::_CoinOnStart(uint32& buttons)
{
	const uint32 start = 1U << RETRO_DEVICE_ID_JOYPAD_START;
	const uint32 select = 1U << RETRO_DEVICE_ID_JOYPAD_SELECT;
	if (!fCoinOnStart)
		return;

	bool startDown = (buttons & start) != 0;
	if (startDown && !fStartWasDown && fCoinFrames == 0)
		fCoinFrames = 24;
	fStartWasDown = startDown;

	if (fCoinFrames > 0) {
		buttons &= ~start;
		if (fCoinFrames > 18)
			buttons |= select;
		else if (fCoinFrames <= 6)
			buttons |= start;
		fCoinFrames--;
	}
}


//	#pragma mark - the game thread


status_t
Core::_ThreadEntry(void* data)
{
	((Core*)data)->_Run();
	return B_OK;
}


void
Core::_Run()
{
	const bigtime_t frameTime = (bigtime_t)(1000000 / fAVInfo.timing.fps);
	bigtime_t next = system_time();

	while (!fQuit) {
		if (fPaused) {
			acquire_sem(fPauseSem);
			next = system_time();
			continue;
		}

		fRun();

		if (fSoundPlayer != NULL) {
			// the sound card sets the pace: wait while enough is queued
			while (!fQuit && !fPaused
				&& fAudioWrite.load() - fAudioRead.load() > fAudioTarget) {
				acquire_sem_etc(fAudioSem, 1, B_RELATIVE_TIMEOUT, 50000);
			}
		} else {
			next += frameTime;
			bigtime_t now = system_time();
			if (next > now)
				snooze_until(next, B_SYSTEM_TIMEBASE);
			else if (now - next > 10 * frameTime)
				next = now;
		}
	}
}


//	#pragma mark - saves


void
Core::_LoadMemory()
{
	if (fGetMemoryData == NULL || fGetMemorySize == NULL)
		return;
	void* memory = fGetMemoryData(RETRO_MEMORY_SAVE_RAM);
	size_t size = fGetMemorySize(RETRO_MEMORY_SAVE_RAM);
	if (memory == NULL || size == 0)
		return;

	BString path = fSaveBase;
	path << ".srm";
	BFile file(path.String(), B_READ_ONLY);
	if (file.InitCheck() == B_OK)
		file.Read(memory, size);
}


void
Core::_SaveMemory()
{
	if (fGetMemoryData == NULL || fGetMemorySize == NULL)
		return;
	void* memory = fGetMemoryData(RETRO_MEMORY_SAVE_RAM);
	size_t size = fGetMemorySize(RETRO_MEMORY_SAVE_RAM);
	if (memory == NULL || size == 0)
		return;

	BString path = fSaveBase;
	path << ".srm";
	write_file(path.String(), memory, size);
}


void
Core::_LoadState()
{
	if (fUnserialize == NULL)
		return;
	BString path = fSaveBase;
	path << ".state";
	void* data = NULL;
	size_t size = 0;
	if (!read_file(path.String(), &data, &size))
		return;
	if (!fUnserialize(data, size))
		fprintf(stderr, "SalaGiochi: could not restore %s\n", path.String());
	free(data);
}


void
Core::_SaveState()
{
	if (fSerializeSize == NULL || fSerialize == NULL)
		return;
	size_t size = fSerializeSize();
	if (size == 0)
		return;
	void* data = malloc(size);
	if (data == NULL)
		return;
	if (fSerialize(data, size)) {
		BString path = fSaveBase;
		path << ".state";
		write_file(path.String(), data, size);
	}
	free(data);
}
