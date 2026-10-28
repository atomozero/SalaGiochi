/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef PATHS_H
#define PATHS_H


#include <String.h>


/*!	Where the games, their covers and names, and the saves are. The names of
	these folders and files are in the language of the system, but one that
	is already there in another language is kept, so that nothing gets lost
	when the language changes. Shared with the installer.
*/
BString		GamesDirectory();					// ~/Games, created
BString		CoversDirectory(const char* games);	// in the games, not created
BString		NamesFile(const char* games);		// in the games
BString		SavesDirectory();					// in the settings, created

// the language of the system, for the libretro cores
unsigned	CoreLanguage();


#endif	// PATHS_H
