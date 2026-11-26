#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

HRESULT RegisterCR3Codec(HMODULE moduleHandle);
HRESULT UnregisterCR3Codec();
