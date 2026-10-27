/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include <Application.h>

#include "MainWindow.h"


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
		(new MainWindow)->Show();
	}
};


int
main()
{
	App app;
	app.Run();
	return 0;
}
