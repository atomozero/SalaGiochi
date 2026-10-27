/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef GAME_LIBRARY_H
#define GAME_LIBRARY_H


#include <GraphicsDefs.h>
#include <String.h>

#include <vector>


class BBitmap;


struct Game {
			BString				name;		// shown, from the file name
			BString				path;
			BString				core;		// path of the libretro core
			BString				system;		// "NES", "Game Boy"...
			BString				saveBase;	// saves, without extension
			rgb_color			color;		// tile color without a cover
			BBitmap*			cover;		// may be NULL
};


/*!	The games in ~/Giochi (one file each), with a cover picture of the same
	name (.png or .jpg) when there is one, and the core that runs them.
*/
class GameLibrary {
public:
								GameLibrary();
								~GameLibrary();

			void				Scan();

			int32				CountGames() const { return fGames.size(); }
			const Game&			GameAt(int32 index) const
									{ return fGames[index]; }
			const char*			Directory() const
									{ return fDirectory.String(); }

private:
			void				_Clear();
			BString				_FindCore(const char* name) const;

			BString				fDirectory;
			BString				fSaveDirectory;
			std::vector<BString> fCoreDirectories;
			std::vector<Game>	fGames;
};


#endif	// GAME_LIBRARY_H
