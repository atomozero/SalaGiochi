/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef SYSTEMS_H
#define SYSTEMS_H


/*!	The kinds of game files SalaGiochi knows, by extension. Shared with the
	installer, that copies only these files.
*/
struct SystemInfo {
	const char*	extension;
	const char*	core;		// the libretro core, NULL for Haiku games
	const char*	system;		// to show, marked for translation
	const char*	thumbnails;	// the libretro-thumbnails repository, or NULL
};


const SystemInfo*	FindSystem(const char* fileName);
bool				IsArcade(const SystemInfo* system);


#endif	// SYSTEMS_H
