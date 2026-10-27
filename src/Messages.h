/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef MESSAGES_H
#define MESSAGES_H


enum {
	kMsgNewFrame		= 'nfrm',	// the core has a frame to show
	kMsgPollPad			= 'ppad',	// read the gamepad for the menus
	kMsgPlayTick		= 'ptik',	// count the time played
	kMsgStartGame		= 'strt',	// "index": the game to start
	kMsgPause			= 'paus',
	kMsgResume			= 'resm',
	kMsgQuitGame		= 'quit',	// back to the list of games
	kMsgOpenParents		= 'prnt',
	kMsgCloseParents	= 'clpr',
	kMsgExitApp			= 'exit',
	kMsgSettingsChanged	= 'sets',
};


#endif	// MESSAGES_H
