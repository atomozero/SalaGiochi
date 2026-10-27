/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "Settings.h"

#include <time.h>

#include <algorithm>

#include <File.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>


static const char* kSettingsName = "SalaGiochi_settings";


static BString
today()
{
	time_t now = time(NULL);
	struct tm local;
	localtime_r(&now, &local);
	char day[16];
	strftime(day, sizeof(day), "%Y-%m-%d", &local);
	return BString(day);
}


static bool
settings_path(BPath& path)
{
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path, true) != B_OK)
		return false;
	return path.Append(kSettingsName) == B_OK;
}


Settings::Settings()
	:
	fDailyMinutes(30),
	fMaxVolume(70),
	fPlayedMinutes(0)
{
}


void
Settings::Load()
{
	BPath path;
	if (!settings_path(path))
		return;
	BFile file(path.Path(), B_READ_ONLY);
	BMessage archive;
	if (file.InitCheck() != B_OK || archive.Unflatten(&file) != B_OK)
		return;

	fDailyMinutes = archive.GetInt32("daily minutes", fDailyMinutes);
	fMaxVolume = archive.GetInt32("max volume", fMaxVolume);
	fDay = archive.GetString("day", "");
	fPlayedMinutes = archive.GetFloat("played minutes", 0);
	const char* game;
	for (int32 i = 0; archive.FindString("hidden", i, &game) == B_OK; i++)
		fHidden.insert(game);
	_CheckDay();
}


void
Settings::Save() const
{
	BMessage archive;
	archive.AddInt32("daily minutes", fDailyMinutes);
	archive.AddInt32("max volume", fMaxVolume);
	archive.AddString("day", fDay);
	archive.AddFloat("played minutes", fPlayedMinutes);
	for (const std::string& game : fHidden)
		archive.AddString("hidden", game.c_str());

	BPath path;
	if (!settings_path(path))
		return;
	BString temp(path.Path());
	temp << ".tmp";
	BFile file(temp.String(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK || archive.Flatten(&file) != B_OK
		|| file.Sync() != B_OK)
		return;
	file.Unset();
	rename(temp.String(), path.Path());
}


void
Settings::SetDailyMinutes(int32 minutes)
{
	fDailyMinutes = std::max<int32>(5, std::min<int32>(240, minutes));
}


void
Settings::SetMaxVolume(int32 percent)
{
	fMaxVolume = std::max<int32>(0, std::min<int32>(100, percent));
}


float
Settings::PlayedMinutes()
{
	_CheckDay();
	return fPlayedMinutes;
}


void
Settings::AddPlayed(float minutes)
{
	_CheckDay();
	fPlayedMinutes += minutes;
}


int32
Settings::MinutesLeft()
{
	float left = fDailyMinutes - PlayedMinutes();
	return left > 0 ? (int32)(left + 0.99f) : 0;
}


bool
Settings::IsHidden(const char* game) const
{
	return fHidden.find(game) != fHidden.end();
}


void
Settings::SetHidden(const char* game, bool hidden)
{
	if (hidden)
		fHidden.insert(game);
	else
		fHidden.erase(game);
}


void
Settings::_CheckDay()
{
	BString day = today();
	if (day != fDay) {
		fDay = day;
		fPlayedMinutes = 0;
	}
}
