/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "Paths.h"

#include <string.h>

#include <vector>

#include <Application.h>
#include <Catalog.h>
#include <Directory.h>
#include <Entry.h>
#include <FindDirectory.h>
#include <Language.h>
#include <LocaleRoster.h>
#include <Message.h>
#include <Path.h>
#include <Roster.h>

#include "libretro.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Paths"


static const char* kGames = B_TRANSLATE_MARK("Games");
static const char* kCovers = B_TRANSLATE_MARK("covers");
static const char* kNames = B_TRANSLATE_MARK("names.txt");
static const char* kSaves = B_TRANSLATE_MARK("saves");


/*!	The name of \a english in \a parent: the one already there, in the
	language of the system, in English or in any language the application
	has a catalog for; or, if there is none yet, in the language of the
	system.
*/
static BString
localized_entry(const char* parent, const char* english)
{
	BString translated(B_TRANSLATE_NOCOLLECT(english));
	std::vector<BString> names = { translated, english };

	app_info info;
	if (be_app != NULL && be_app->GetAppInfo(&info) == B_OK) {
		BString leaf(info.signature);
		leaf.Remove(0, leaf.FindFirst('/') + 1);
		BMessage languages;
		BLocaleRoster::Default()->GetAvailableCatalogs(&languages,
			leaf.String());
		const char* language;
		for (int32 i = 0; languages.FindString("language", i, &language)
				== B_OK; i++) {
			BCatalog catalog(info.signature, language);
			if (catalog.InitCheck() == B_OK)
				names.push_back(catalog.GetString(english, "Paths"));
		}
	}

	for (const BString& name : names) {
		BPath path(parent, name.String());
		if (BEntry(path.Path()).Exists())
			return name;
	}
	return translated;
}


static BString
child(const char* parent, const char* english)
{
	BPath path(parent, localized_entry(parent, english).String());
	return path.Path();
}


BString
GamesDirectory()
{
	BPath home;
	if (find_directory(B_USER_DIRECTORY, &home) != B_OK)
		return BString();
	BString games = child(home.Path(), kGames);
	create_directory(games.String(), 0755);
	return games;
}


BString
CoversDirectory(const char* games)
{
	return child(games, kCovers);
}


BString
NamesFile(const char* games)
{
	return child(games, kNames);
}


BString
SavesDirectory()
{
	BPath settings;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &settings) != B_OK)
		return BString();
	settings.Append("SalaGiochi");
	create_directory(settings.Path(), 0755);
	BString saves = child(settings.Path(), kSaves);
	create_directory(saves.String(), 0755);
	return saves;
}


unsigned
CoreLanguage()
{
	struct Language {
		const char*	code;
		unsigned	language;
	};
	// the more precise first
	static const Language kLanguages[] = {
		{ "pt_BR", RETRO_LANGUAGE_PORTUGUESE_BRAZIL },
		{ "zh_Hant", RETRO_LANGUAGE_CHINESE_TRADITIONAL },
		{ "zh_TW", RETRO_LANGUAGE_CHINESE_TRADITIONAL },
		{ "en_GB", RETRO_LANGUAGE_BRITISH_ENGLISH },
		{ "en", RETRO_LANGUAGE_ENGLISH },
		{ "ja", RETRO_LANGUAGE_JAPANESE },
		{ "fr", RETRO_LANGUAGE_FRENCH },
		{ "es", RETRO_LANGUAGE_SPANISH },
		{ "de", RETRO_LANGUAGE_GERMAN },
		{ "it", RETRO_LANGUAGE_ITALIAN },
		{ "nl", RETRO_LANGUAGE_DUTCH },
		{ "pt", RETRO_LANGUAGE_PORTUGUESE_PORTUGAL },
		{ "ru", RETRO_LANGUAGE_RUSSIAN },
		{ "ko", RETRO_LANGUAGE_KOREAN },
		{ "zh", RETRO_LANGUAGE_CHINESE_SIMPLIFIED },
		{ "eo", RETRO_LANGUAGE_ESPERANTO },
		{ "pl", RETRO_LANGUAGE_POLISH },
		{ "vi", RETRO_LANGUAGE_VIETNAMESE },
		{ "ar", RETRO_LANGUAGE_ARABIC },
		{ "el", RETRO_LANGUAGE_GREEK },
		{ "tr", RETRO_LANGUAGE_TURKISH },
		{ "sk", RETRO_LANGUAGE_SLOVAK },
		{ "fa", RETRO_LANGUAGE_PERSIAN },
		{ "he", RETRO_LANGUAGE_HEBREW },
		{ "ast", RETRO_LANGUAGE_ASTURIAN },
		{ "fi", RETRO_LANGUAGE_FINNISH },
		{ "id", RETRO_LANGUAGE_INDONESIAN },
		{ "sv", RETRO_LANGUAGE_SWEDISH },
		{ "uk", RETRO_LANGUAGE_UKRAINIAN },
		{ "cs", RETRO_LANGUAGE_CZECH },
		{ "ca", RETRO_LANGUAGE_CATALAN },
		{ "hu", RETRO_LANGUAGE_HUNGARIAN },
		{ "be", RETRO_LANGUAGE_BELARUSIAN },
	};

	BMessage preferred;
	const char* code;
	if (BLocaleRoster::Default()->GetPreferredLanguages(&preferred) != B_OK
		|| preferred.FindString("language", &code) != B_OK)
		return RETRO_LANGUAGE_ENGLISH;
	BString language(code);
	for (const Language& candidate : kLanguages) {
		size_t length = strlen(candidate.code);
		if (language.Compare(candidate.code, length) == 0
			&& (language.Length() == (int32)length
				|| language.ByteAt(length) == '_'
				|| language.ByteAt(length) == '-'))
			return candidate.language;
	}
	return RETRO_LANGUAGE_ENGLISH;
}
