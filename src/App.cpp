/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include <Application.h>

#include "MainWindow.h"
#include "Messages.h"


static const char* kSignature = "application/x-vnd.atomozero-SalaGiochi";


class App : public BApplication {
public:
	App()
		:
		BApplication(kSignature)
	{
	}

	virtual void ReadyToRun()
	{
		fWindow = new MainWindow;
		fWindow->Show();
	}

	virtual void MessageReceived(BMessage* message)
	{
		if (message->what == kMsgRescan && fWindow != NULL)
			fWindow->PostMessage(kMsgRescan);
		else
			BApplication::MessageReceived(message);
	}

private:
	MainWindow*	fWindow = NULL;
};


int
main()
{
	App app;
	app.Run();
	return 0;
}
