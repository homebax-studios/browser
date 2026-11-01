#pragma once

#include <windows.h>

namespace HB::Browser {

	bool Initialize(HINSTANCE hInstance, int nCmdShow);
	int Run();
	void Shutdown();

}