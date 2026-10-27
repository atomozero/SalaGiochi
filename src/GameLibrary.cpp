/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "GameLibrary.h"

#include <algorithm>
#include <ctype.h>
#include <stdio.h>
#include <map>
#include <string.h>

#include <Application.h>
#include <Bitmap.h>
#include <Catalog.h>
#include <Directory.h>
#include <Entry.h>
#include <FindDirectory.h>
#include <Path.h>
#include <Roster.h>
#include <TranslationUtils.h>


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Systems"


struct SystemInfo {
	const char*	extension;
	const char*	core;
	const char*	system;
};


static const SystemInfo kSystems[] = {
	{ "nes", "fceumm", "NES" },
	{ "fds", "fceumm", "NES" },
	{ "gb", "gambatte", "Game Boy" },
	{ "gbc", "gambatte", "Game Boy Color" },
	{ "gba", "gpsp", "Game Boy Advance" },
	{ "md", "picodrive", "Mega Drive" },
	{ "gen", "picodrive", "Mega Drive" },
	{ "smd", "picodrive", "Mega Drive" },
	{ "sms", "picodrive", "Master System" },
	{ "gg", "picodrive", "Game Gear" },
	{ "sfc", "snes9x2005", "Super Nintendo" },
	{ "smc", "snes9x2005", "Super Nintendo" },
	{ "zip", "mame2003_plus", B_TRANSLATE_MARK("Arcade") },
	{ "vec", "vecx", "Vectrex" },
	{ "cue", "pcsx_rearmed", "PlayStation" },
	{ "pbp", "pcsx_rearmed", "PlayStation" },
	{ "chd", "pcsx_rearmed", "PlayStation" },
	{ "tic", "tic80", "TIC-80" },
};


static const rgb_color kTileColors[] = {
	{ 232, 96, 76, 255 },
	{ 123, 198, 126, 255 },
	{ 91, 164, 230, 255 },
	{ 180, 139, 224, 255 },
	{ 242, 179, 61, 255 },
	{ 64, 190, 190, 255 },
};


static BString
display_name(const char* fileName)
{
	BString name(fileName);
	int32 dot = name.FindLast('.');
	if (dot > 0)
		name.Truncate(dot);
	// "Some_Game (Europe) [!]" -> "Some Game"
	int32 bracket = name.FindFirst(" (");
	if (bracket > 0)
		name.Truncate(bracket);
	bracket = name.FindFirst(" [");
	if (bracket > 0)
		name.Truncate(bracket);
	name.ReplaceAll('_', ' ');
	if (name.Length() > 0)
		name.SetByteAt(0, toupper(name.ByteAt(0)));
	return name;
}


/*!	Names for files whose name can not be changed (the arcade sets), one per
	line in "nomi.txt": "supertnk.zip = Super Tank".
*/
static std::map<BString, BString>
read_names(const char* directory)
{
	std::map<BString, BString> names;
	BString path(directory);
	path << "/nomi.txt";
	FILE* file = fopen(path.String(), "r");
	if (file == NULL)
		return names;
	char line[512];
	while (fgets(line, sizeof(line), file) != NULL) {
		BString text(line);
		text.RemoveAll("\n").RemoveAll("\r");
		int32 equals = text.FindFirst('=');
		if (equals <= 0 || text.ByteAt(0) == '#')
			continue;
		BString file;
		BString name;
		text.CopyInto(file, 0, equals);
		text.CopyInto(name, equals + 1, text.Length() - equals - 1);
		file.Trim();
		name.Trim();
		if (!file.IsEmpty() && !name.IsEmpty())
			names[file.ToLower()] = name;
	}
	fclose(file);
	return names;
}


static BBitmap*
load_cover(const BPath& directory, const BString& base)
{
	static const char* kPlaces[] = { "", "copertine/" };
	static const char* kExtensions[] = { ".png", ".jpg", ".jpeg" };
	for (const char* place : kPlaces) {
		for (const char* extension : kExtensions) {
			BPath path(directory);
			BString leaf;
			leaf << place << base << extension;
			path.Append(leaf.String());
			if (BEntry(path.Path()).Exists())
				return BTranslationUtils::GetBitmap(path.Path());
		}
	}
	return NULL;
}


GameLibrary::GameLibrary()
{
	BPath path;
	if (find_directory(B_USER_DIRECTORY, &path) == B_OK) {
		path.Append("Giochi");
		create_directory(path.Path(), 0755);
		fDirectory = path.Path();
	}
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK) {
		path.Append("SalaGiochi/salvataggi");
		create_directory(path.Path(), 0755);
		fSaveDirectory = path.Path();
	}

	// the cores: next to the application first, then the add-on folders
	app_info info;
	if (be_app != NULL && be_app->GetAppInfo(&info) == B_OK) {
		BPath appPath(&info.ref);
		BPath parent;
		if (appPath.GetParent(&parent) == B_OK) {
			parent.Append("cores");
			fCoreDirectories.push_back(parent.Path());
		}
	}
	static const directory_which kAddOns[] = {
		B_USER_NONPACKAGED_ADDONS_DIRECTORY,
		B_USER_ADDONS_DIRECTORY,
		B_SYSTEM_NONPACKAGED_ADDONS_DIRECTORY,
		B_SYSTEM_ADDONS_DIRECTORY,
	};
	for (directory_which which : kAddOns) {
		if (find_directory(which, &path) == B_OK) {
			path.Append("libretro");
			fCoreDirectories.push_back(path.Path());
		}
	}
}


GameLibrary::~GameLibrary()
{
	_Clear();
}


void
GameLibrary::Scan()
{
	_Clear();

	std::map<BString, BString> names = read_names(fDirectory.String());
	BDirectory directory(fDirectory.String());
	BEntry entry;
	while (directory.GetNextEntry(&entry, true) == B_OK) {
		if (!entry.IsFile())
			continue;
		char leaf[B_FILE_NAME_LENGTH];
		entry.GetName(leaf);
		const char* dot = strrchr(leaf, '.');
		if (dot == NULL || leaf[0] == '.')
			continue;

		const SystemInfo* system = NULL;
		for (const SystemInfo& candidate : kSystems) {
			if (strcasecmp(dot + 1, candidate.extension) == 0) {
				system = &candidate;
				break;
			}
		}
		if (system == NULL)
			continue;

		BString core = _FindCore(system->core);
		if (core.IsEmpty())
			continue;

		BPath path;
		entry.GetPath(&path);
		BString base(leaf, dot - leaf);

		Game game;
		auto named = names.find(BString(leaf).ToLower());
		game.name = named != names.end() ? named->second : display_name(leaf);
		game.path = path.Path();
		game.core = core;
		game.system = B_TRANSLATE_NOCOLLECT(system->system);
		game.arcade = strcmp(system->core, "mame2003_plus") == 0;
		game.saveBase = fSaveDirectory;
		game.saveBase << "/" << base;
		game.cover = load_cover(BPath(fDirectory.String()), base);
		fGames.push_back(game);
	}

	std::sort(fGames.begin(), fGames.end(),
		[](const Game& a, const Game& b) {
			return a.name.ICompare(b.name) < 0;
		});
	for (size_t i = 0; i < fGames.size(); i++)
		fGames[i].color = kTileColors[i % B_COUNT_OF(kTileColors)];
}


void
GameLibrary::_Clear()
{
	for (Game& game : fGames)
		delete game.cover;
	fGames.clear();
}


BString
GameLibrary::_FindCore(const char* name) const
{
	BString leaf(name);
	leaf << "_libretro.so";
	for (const BString& directory : fCoreDirectories) {
		BString path(directory);
		path << "/" << leaf;
		if (BEntry(path.String()).Exists())
			return path;
	}
	return BString();
}
