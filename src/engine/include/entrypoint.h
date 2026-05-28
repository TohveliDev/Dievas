#pragma once
#include <application.h>

// Factory function implemented by the user/application layer
// Used to create the concrete Application instance
extern Application* createApplication(int argc, char** argv);

// Global flag controlling the main loop lifecycle
bool g_applicationRunning = true;

// Entry point of the program
int main(int argc, char** argv)
{
	// Main loop allows the application to be recreated and rerun
	while (g_applicationRunning)
	{
		// Create a new application instance
		Application* app = createApplication(argc, argv);

		// Run the application (main update/render loop happens here)
		app->run();

		// Clean up after application exits
		delete app;
	}

	return 0;
}