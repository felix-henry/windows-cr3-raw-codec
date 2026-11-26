#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

HRESULT RegisterCR3Codec(HMODULE moduleHandle, HKEY regRoot = HKEY_LOCAL_MACHINE);
HRESULT UnregisterCR3Codec(HKEY regRoot = HKEY_LOCAL_MACHINE);
