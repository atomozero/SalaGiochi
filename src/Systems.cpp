/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "Systems.h"

#include <string.h>
#include <strings.h>

#include <Catalog.h>
#include <SupportDefs.h>


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Systems"


static const SystemInfo kSystems[] = {
	{ "nes", "fceumm", "NES", "Nintendo_-_Nintendo_Entertainment_System" },
	{ "fds", "fceumm", "NES", "Nintendo_-_Family_Computer_Disk_System" },
	{ "gb", "gambatte", "Game Boy", "Nintendo_-_Game_Boy" },
	{ "gbc", "gambatte", "Game Boy Color", "Nintendo_-_Game_Boy_Color" },
	{ "gba", "gpsp", "Game Boy Advance", "Nintendo_-_Game_Boy_Advance" },
	{ "md", "picodrive", "Mega Drive", "Sega_-_Mega_Drive_-_Genesis" },
	{ "gen", "picodrive", "Mega Drive", "Sega_-_Mega_Drive_-_Genesis" },
	{ "smd", "picodrive", "Mega Drive", "Sega_-_Mega_Drive_-_Genesis" },
	{ "sms", "picodrive", "Master System", "Sega_-_Master_System_-_Mark_III" },
	{ "gg", "picodrive", "Game Gear", "Sega_-_Game_Gear" },
	{ "sfc", "snes9x2005", "Super Nintendo",
		"Nintendo_-_Super_Nintendo_Entertainment_System" },
	{ "smc", "snes9x2005", "Super Nintendo",
		"Nintendo_-_Super_Nintendo_Entertainment_System" },
	{ "zip", "mame2003_plus", B_TRANSLATE_MARK("Arcade"), NULL },
	{ "vec", "vecx", "Vectrex", "GCE_-_Vectrex" },
	{ "cue", "pcsx_rearmed", "PlayStation", "Sony_-_PlayStation" },
	{ "pbp", "pcsx_rearmed", "PlayStation", "Sony_-_PlayStation" },
	{ "chd", "pcsx_rearmed", "PlayStation", "Sony_-_PlayStation" },
	{ "tic", "tic80", "TIC-80", NULL },
	// a text file: the program to start, then one argument per line
	{ "gioco", NULL, B_TRANSLATE_MARK("Haiku game"), NULL },
};


const SystemInfo*
FindSystem(const char* fileName)
{
	const char* dot = strrchr(fileName, '.');
	if (dot == NULL || dot == fileName)
		return NULL;
	for (const SystemInfo& system : kSystems) {
		if (strcasecmp(dot + 1, system.extension) == 0)
			return &system;
	}
	return NULL;
}


bool
IsArcade(const SystemInfo* system)
{
	return system != NULL && system->core != NULL
		&& strcmp(system->core, "mame2003_plus") == 0;
}
