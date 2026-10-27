/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef SETTINGS_H
#define SETTINGS_H


#include <String.h>

#include <set>
#include <string>


/*!	What the parents set, and the time played today. */
class Settings {
public:
								Settings();

			void				Load();
			void				Save() const;

			int32				DailyMinutes() const { return fDailyMinutes; }
			void				SetDailyMinutes(int32 minutes);
			int32				MaxVolume() const { return fMaxVolume; }
			void				SetMaxVolume(int32 percent);

			// the time played today; it starts over on another day
			float				PlayedMinutes();
			void				AddPlayed(float minutes);
			int32				MinutesLeft();

			bool				IsHidden(const char* game) const;
			void				SetHidden(const char* game, bool hidden);

private:
			void				_CheckDay();

			int32				fDailyMinutes;
			int32				fMaxVolume;
			BString				fDay;
			float				fPlayedMinutes;
			std::set<std::string> fHidden;
};


#endif	// SETTINGS_H
