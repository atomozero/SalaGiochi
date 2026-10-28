/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


/*!	The parents drop game files here (on the window, or on its icon or a link
	to it): they are copied to ~/Games (translated), taken out of zip and 7z
	archives, given a cover when one can be found, and SalaGiochi reads them
	again.
*/


#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <unistd.h>

#include <vector>

#include <Application.h>
#include <Button.h>
#include <Catalog.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <LayoutBuilder.h>
#include <Path.h>
#include <ScrollView.h>
#include <String.h>
#include <StringFormat.h>
#include <StringList.h>
#include <TextView.h>
#include <Window.h>

#include "../src/Messages.h"
#include "../src/Paths.h"
#include "../src/Systems.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Installer"


static const char* kSignature
	= "application/x-vnd.atomozero-SalaGiochi-Installer";
static const char* kSalaGiochi = "application/x-vnd.atomozero-SalaGiochi";
static const char* kThumbnails
	= "https://raw.githubusercontent.com/libretro-thumbnails/";

static const uint32 kMsgLog = 'log ';
static const uint32 kMsgDone = 'done';


//	#pragma mark - helpers


/*!	Runs \a arguments (no shell, so names need no quoting) and returns its
	exit status; its output goes to \a output when given.
*/
static int
run(const std::vector<BString>& arguments, BString* output = NULL)
{
	std::vector<const char*> argv;
	for (const BString& argument : arguments)
		argv.push_back(argument.String());
	argv.push_back(NULL);

	int pipes[2];
	if (pipe(pipes) != 0)
		return -1;
	pid_t child = fork();
	if (child < 0) {
		close(pipes[0]);
		close(pipes[1]);
		return -1;
	}
	if (child == 0) {
		dup2(pipes[1], STDOUT_FILENO);
		int null = open("/dev/null", O_WRONLY);
		if (null >= 0)
			dup2(null, STDERR_FILENO);
		close(pipes[0]);
		close(pipes[1]);
		execvp(argv[0], (char* const*)argv.data());
		_exit(127);
	}
	close(pipes[1]);
	char buffer[4096];
	ssize_t bytes;
	while ((bytes = read(pipes[0], buffer, sizeof(buffer))) > 0) {
		if (output != NULL)
			output->Append(buffer, bytes);
	}
	close(pipes[0]);
	int status;
	if (waitpid(child, &status, 0) != child || !WIFEXITED(status))
		return -1;
	return WEXITSTATUS(status);
}


static BString
leaf_of(const char* path)
{
	const char* slash = strrchr(path, '/');
	return BString(slash != NULL ? slash + 1 : path);
}


static BString
base_of(const char* name)
{
	BString base(name);
	int32 dot = base.FindLast('.');
	if (dot > 0)
		base.Truncate(dot);
	return base;
}


static BString
extension_of(const char* name)
{
	const char* dot = strrchr(name, '.');
	BString extension(dot != NULL ? dot + 1 : "");
	return extension.ToLower();
}


static bool
is_png(const char* path)
{
	static const uint8 kMagic[] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a,
		'\n' };
	uint8 header[sizeof(kMagic)];
	BFile file(path, B_READ_ONLY);
	return file.Read(header, sizeof(header)) == (ssize_t)sizeof(header)
		&& memcmp(header, kMagic, sizeof(kMagic)) == 0;
}


/*!	Copies through a temporary file, so that SalaGiochi never sees half a
	game.
*/
static status_t
copy_file(const char* source, const char* target)
{
	BFile in(source, B_READ_ONLY);
	status_t status = in.InitCheck();
	if (status != B_OK)
		return status;
	BPath targetPath(target);
	BPath directory;
	targetPath.GetParent(&directory);
	BString temporary(directory.Path());
	temporary << "/." << targetPath.Leaf() << ".part";
	BFile out(temporary.String(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	status = out.InitCheck();
	if (status != B_OK)
		return status;

	char buffer[64 * 1024];
	ssize_t bytes;
	while ((bytes = in.Read(buffer, sizeof(buffer))) > 0) {
		if (out.Write(buffer, bytes) != bytes) {
			status = errno != 0 ? errno : B_IO_ERROR;
			break;
		}
	}
	if (bytes < 0)
		status = bytes;
	if (status == B_OK)
		status = out.Sync();
	out.Unset();
	BEntry entry(temporary.String());
	if (status == B_OK)
		status = entry.Rename(target, false);
	if (status != B_OK)
		entry.Remove();
	return status;
}


//	#pragma mark - Installer


/*!	Does the work in its own thread, and tells the window what happened. */
class Installer {
public:
								Installer(BMessenger window,
									const std::vector<entry_ref>& refs);

	static	status_t			Run(void* data);

private:
			void				_Log(const char* text);
			void				_Install(const entry_ref& ref, int32 depth);
			void				_InstallFile(const char* path,
									const char* name);
			void				_InstallArchive(const char* path,
									const char* name);
			bool				_CopyGame(const char* source, const char* name);
			void				_CopyCueTracks(const char* cue);
			void				_GetCover(const char* name);
			bool				_Download(const char* repository,
									const char* file, const char* target,
									int32 links);

			BMessenger			fWindow;
			std::vector<entry_ref> fRefs;
			BString				fGames;
			int32				fInstalled;
};


Installer::Installer(BMessenger window, const std::vector<entry_ref>& refs)
	:
	fWindow(window),
	fRefs(refs),
	fInstalled(0)
{
	fGames = GamesDirectory();
}


status_t
Installer::Run(void* data)
{
	Installer* installer = (Installer*)data;
	for (const entry_ref& ref : installer->fRefs)
		installer->_Install(ref, 0);

	if (installer->fInstalled > 0) {
		// SalaGiochi, if it is open, shows them right away
		BMessenger(kSalaGiochi).SendMessage(kMsgRescan);
	}
	BMessage done(kMsgDone);
	done.AddInt32("installed", installer->fInstalled);
	installer->fWindow.SendMessage(&done);
	delete installer;
	return B_OK;
}


void
Installer::_Log(const char* text)
{
	BMessage message(kMsgLog);
	message.AddString("text", text);
	fWindow.SendMessage(&message);
}


void
Installer::_Install(const entry_ref& ref, int32 depth)
{
	BEntry entry(&ref, true);
	BPath path;
	if (entry.GetPath(&path) != B_OK)
		return;

	if (entry.IsDirectory()) {
		// a folder of games, but not a whole disk
		if (depth > 2)
			return;
		BDirectory directory(&entry);
		entry_ref child;
		while (directory.GetNextRef(&child) == B_OK)
			_Install(child, depth + 1);
		return;
	}

	BString extension = extension_of(path.Leaf());
	if (extension == "zip" || extension == "7z")
		_InstallArchive(path.Path(), path.Leaf());
	else
		_InstallFile(path.Path(), path.Leaf());
}


void
Installer::_InstallFile(const char* path, const char* name)
{
	const SystemInfo* system = FindSystem(name);
	if (system == NULL) {
		// the tracks of a CD come with their .cue, no need to say it
		BString extension = extension_of(name);
		if (extension != "bin" && extension != "img" && extension != "iso") {
			BString text(B_TRANSLATE("%name%: not a game SalaGiochi knows"));
			text.ReplaceFirst("%name%", name);
			_Log(text.String());
		}
		return;
	}
	if (!_CopyGame(path, name))
		return;
	if (extension_of(name) == "cue")
		_CopyCueTracks(path);
	_GetCover(name);
}


/*!	Takes the games out of an archive. A zip without any is an arcade game:
	those stay zipped, as MAME wants them.
*/
void
Installer::_InstallArchive(const char* path, const char* name)
{
	bool zip = extension_of(name) == "zip";
	BString listing;
	int status = zip
		? run({ "unzip", "-Z1", path }, &listing)
		: run({ "7z", "l", "-slt", "-ba", path }, &listing);
	if (status != 0) {
		BString text(B_TRANSLATE("%name%: the archive can't be read"));
		text.ReplaceFirst("%name%", name);
		_Log(text.String());
		return;
	}

	BStringList lines;
	listing.Split("\n", true, lines);
	std::vector<BString> games;
	for (int32 i = 0; i < lines.CountStrings(); i++) {
		BString line = lines.StringAt(i);
		line.RemoveAll("\r");
		if (!zip) {
			if (!line.StartsWith("Path = "))
				continue;
			line.Remove(0, 7);
		}
		if (line.IsEmpty() || line.EndsWith("/"))
			continue;
		const SystemInfo* system = FindSystem(leaf_of(line).String());
		if (system == NULL || IsArcade(system))
			continue;
		games.push_back(line);
	}

	if (games.empty()) {
		if (zip) {
			// an arcade set
			if (_CopyGame(path, name))
				_GetCover(name);
		} else {
			BString text(B_TRANSLATE("%name%: there is no game inside"));
			text.ReplaceFirst("%name%", name);
			_Log(text.String());
		}
		return;
	}

	// out to a folder of our own first, then copied like any other game
	BString temporary(fGames);
	temporary << "/.installing";
	run({ "rm", "-rf", temporary });
	create_directory(temporary.String(), 0755);
	// all of it (the tracks of a CD too): unzip would read the brackets of
	// "Game (USA) [!].sfc" as a pattern
	std::vector<BString> extract;
	if (zip)
		extract = { "unzip", "-o", "-j", "-qq", path, "-d", temporary };
	else
		extract = { "7z", "e", "-y", BString("-o") << temporary, path };
	if (run(extract) != 0) {
		BString text(B_TRANSLATE("%name%: the archive can't be read"));
		text.ReplaceFirst("%name%", name);
		_Log(text.String());
		run({ "rm", "-rf", temporary });
		return;
	}

	for (const BString& game : games) {
		BString leaf = leaf_of(game);
		BString source(temporary);
		source << "/" << leaf;
		if (_CopyGame(source.String(), leaf.String())) {
			if (extension_of(leaf) == "cue")
				_CopyCueTracks(source.String());
			_GetCover(leaf.String());
		}
	}
	run({ "rm", "-rf", temporary });
}


bool
Installer::_CopyGame(const char* source, const char* name)
{
	BString target(fGames);
	target << "/" << name;
	BString text;
	if (BEntry(target.String()).Exists()) {
		text = B_TRANSLATE("%name%: it's already there");
		text.ReplaceFirst("%name%", name);
		_Log(text.String());
		return false;
	}
	status_t status = copy_file(source, target.String());
	if (status != B_OK) {
		text = B_TRANSLATE("%name%: it can't be copied (%error%)");
		text.ReplaceFirst("%name%", name);
		text.ReplaceFirst("%error%", strerror(status));
		_Log(text.String());
		return false;
	}

	const SystemInfo* system = FindSystem(name);
	text = B_TRANSLATE("%name%: added (%system%)");
	text.ReplaceFirst("%name%", name);
	text.ReplaceFirst("%system%",
		B_TRANSLATE_NOCOLLECT_ALL(system->system, "Systems", NULL));
	_Log(text.String());
	fInstalled++;
	return true;
}


/*!	A .cue names the tracks of the CD in its FILE lines: they go next to it. */
void
Installer::_CopyCueTracks(const char* cue)
{
	FILE* file = fopen(cue, "r");
	if (file == NULL)
		return;
	BPath directory;
	BPath(cue).GetParent(&directory);
	char line[1024];
	while (fgets(line, sizeof(line), file) != NULL) {
		BString text(line);
		text.Trim();
		if (!text.IStartsWith("FILE"))
			continue;
		int32 first = text.FindFirst('"');
		int32 last = text.FindLast('"');
		if (first < 0 || last <= first)
			continue;
		BString track;
		text.CopyInto(track, first + 1, last - first - 1);
		track = leaf_of(track);
		BString source(directory.Path());
		source << "/" << track;
		BString target(fGames);
		target << "/" << track;
		if (!BEntry(target.String()).Exists())
			copy_file(source.String(), target.String());
	}
	fclose(file);
}


/*!	The picture of the box from libretro-thumbnails, when the file has the
	usual name of the game ("Super Mario World (USA).sfc").
*/
void
Installer::_GetCover(const char* name)
{
	const SystemInfo* system = FindSystem(name);
	if (system == NULL || system->thumbnails == NULL)
		return;

	BString base = base_of(name);
	BString covers = CoversDirectory(fGames.String());
	create_directory(covers.String(), 0755);
	static const char* kExtensions[] = { ".png", ".jpg", ".jpeg" };
	for (const char* extension : kExtensions) {
		BString existing(covers);
		existing << "/" << base << extension;
		if (BEntry(existing.String()).Exists())
			return;
	}

	// the names there have these characters replaced
	BString file(base);
	for (const char* c = "&*/:`<>?\\|\""; *c != '\0'; c++)
		file.ReplaceAll(*c, '_');
	file << ".png";
	BString target(covers);
	target << "/" << base << ".png";

	BString text;
	if (_Download(system->thumbnails, file.String(), target.String(), 1)) {
		text = B_TRANSLATE("%name%: cover found");
		_Log(text.ReplaceFirst("%name%", base).String());
	} else {
		text = B_TRANSLATE("%name%: no cover found");
		_Log(text.ReplaceFirst("%name%", base).String());
	}
}


bool
Installer::_Download(const char* repository, const char* file,
	const char* target, int32 links)
{
	BString url(kThumbnails);
	BString encoded(file);
	// curl wants the URL ready: encode what can be in a game name
	encoded.ReplaceAll("%", "%25");
	encoded.ReplaceAll(" ", "%20");
	encoded.ReplaceAll("#", "%23");
	encoded.ReplaceAll("+", "%2B");
	encoded.ReplaceAll("'", "%27");
	encoded.ReplaceAll("(", "%28");
	encoded.ReplaceAll(")", "%29");
	encoded.ReplaceAll("[", "%5B");
	encoded.ReplaceAll("]", "%5D");
	encoded.ReplaceAll(",", "%2C");
	encoded.ReplaceAll("!", "%21");
	url << repository << "/master/Named_Boxarts/" << encoded;

	BString temporary(target);
	temporary << ".part";
	if (run({ "curl", "-sfL", "--max-time", "30", "-o", temporary, url })
			!= 0) {
		BEntry(temporary.String()).Remove();
		return false;
	}

	if (!is_png(temporary.String())) {
		// a link in the repository: its text is the name of the picture
		BString linked;
		BFile link(temporary.String(), B_READ_ONLY);
		char buffer[256];
		ssize_t bytes = link.Read(buffer, sizeof(buffer) - 1);
		link.Unset();
		BEntry(temporary.String()).Remove();
		if (bytes <= 0 || links <= 0)
			return false;
		buffer[bytes] = '\0';
		linked = leaf_of(BString(buffer).Trim().String());
		if (!linked.EndsWith(".png"))
			return false;
		return _Download(repository, linked.String(), target, links - 1);
	}
	return BEntry(temporary.String()).Rename(target, true) == B_OK;
}


//	#pragma mark - InstallerWindow


/*!	Where the files are dropped, with what happened to them. */
class InstallerWindow : public BWindow {
public:
								InstallerWindow();

	virtual	void				MessageReceived(BMessage* message);
			void				Install(BMessage* refs);

private:
			BTextView*			fLog;
			BButton*			fClose;
			int32				fRunning;
};


InstallerWindow::InstallerWindow()
	:
	BWindow(BRect(100, 100, 620, 460), B_TRANSLATE_SYSTEM_NAME("Add games"),
		B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS | B_QUIT_ON_WINDOW_CLOSE),
	fRunning(0)
{
	BTextView* help = new BTextView("help");
	help->MakeEditable(false);
	help->MakeSelectable(false);
	help->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	BString text(B_TRANSLATE("Drop the games here: files, zip or 7z archives "
		"and folders. They are copied to the %folder% folder in your home, "
		"with the picture of the box when one can be found, and SalaGiochi "
		"shows them right away."));
	text.ReplaceFirst("%folder%", BPath(GamesDirectory().String()).Leaf());
	help->SetText(text.String());

	fLog = new BTextView("log");
	fLog->MakeEditable(false);
	fLog->SetWordWrap(true);
	BScrollView* scroll = new BScrollView("scroll", fLog, 0, false, true);
	scroll->SetExplicitMinSize(BSize(B_SIZE_UNSET, 200));

	fClose = new BButton("close", B_TRANSLATE("Close"),
		new BMessage(B_QUIT_REQUESTED));

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_SPACING)
		.Add(help)
		.Add(scroll)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(fClose)
		.End();
	CenterOnScreen();
}


void
InstallerWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case B_SIMPLE_DATA:
		case B_REFS_RECEIVED:
			Install(message);
			break;
		case kMsgLog:
		{
			BString text(message->GetString("text", ""));
			text << "\n";
			fLog->Insert(fLog->TextLength(), text.String(), text.Length());
			fLog->ScrollToOffset(fLog->TextLength());
			break;
		}
		case kMsgDone:
		{
			fRunning--;
			int32 installed = message->GetInt32("installed", 0);
			BString text;
			if (installed == 0)
				text = B_TRANSLATE("No new games.");
			else {
				static BStringFormat format(B_TRANSLATE("{0, plural, "
					"one{One new game.} other{# new games.}}"));
				format.Format(text, installed);
			}
			text.Prepend("— ").Append("\n\n");
			fLog->Insert(fLog->TextLength(), text.String(), text.Length());
			fLog->ScrollToOffset(fLog->TextLength());
			break;
		}
		default:
			BWindow::MessageReceived(message);
	}
}


void
InstallerWindow::Install(BMessage* message)
{
	std::vector<entry_ref> refs;
	entry_ref ref;
	for (int32 i = 0; message->FindRef("refs", i, &ref) == B_OK; i++)
		refs.push_back(ref);
	if (refs.empty())
		return;

	Installer* installer = new Installer(BMessenger(this), refs);
	thread_id thread = spawn_thread(&Installer::Run, "installer",
		B_NORMAL_PRIORITY, installer);
	if (thread < 0) {
		delete installer;
		return;
	}
	fRunning++;
	resume_thread(thread);
}


//	#pragma mark - App


class App : public BApplication {
public:
	App()
		:
		BApplication(kSignature),
		fWindow(new InstallerWindow)
	{
	}

	virtual void ReadyToRun()
	{
		fWindow->Show();
	}

	virtual void RefsReceived(BMessage* message)
	{
		// dropped on the icon, before or after starting
		fWindow->PostMessage(message);
	}

private:
	InstallerWindow*	fWindow;
};


int
main()
{
	App app;
	app.Run();
	return 0;
}
