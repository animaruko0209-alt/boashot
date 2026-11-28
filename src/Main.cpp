#include "Application.h"

#include <Windows.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR commandLine, int) {
    boa::Application app;
    return app.run(commandLine);
}
