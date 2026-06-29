#pragma once

// Target Windows 10
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WINVER
#define WINVER 0x0A00
#endif

// Windows / MFC
#include <afxwin.h>
#include <afxext.h>
#include <afxcmn.h>
#include <commctrl.h>

// STL
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>

// Resource IDs
#include "../resources/resource.h"
