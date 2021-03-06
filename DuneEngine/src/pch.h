#pragma once

#ifdef DUNE_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#pragma comment(lib,"d3d12.lib") 
#pragma comment(lib,"d3dcompiler.lib") 
#pragma comment(lib,"dxgi.lib") 
#pragma comment(lib,"dxguid.lib")

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <dxgidebug.h>
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"
#include "Remotery/Remotery.h"

#define ThrowIfFailed(hr) if(FAILED(hr)) { __debugbreak(); assert(false);}
#ifdef _DEBUG
#define NameDXObject(obj, name) obj->SetName(name); OutputDebugString(name); OutputDebugString(L" has been created \n");

#define NameDXObjectIndexed(obj, n, name)						\
{																\
	wchar_t indexedName[128];									\
	if (swprintf_s(indexedName, L"%s[%u]", name, n) > 0)		\
	{															\
		obj->SetName(indexedName);								\
		OutputDebugString(indexedName);							\
		OutputDebugString(L" has been created \n");				\
	}															\
}
#else
#define NameDXObject(obj, name)
#define NameDXObjectIndexed(obj, n, name)
#endif
#endif // DUNE_PLATFORM_WINDOWS

#define Assert(condition) if (!(condition)) {__debugbreak(); assert(false);} 

#include <iostream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <memory>
#include <assert.h>
#include <type_traits>
#include <thread>
#include <mutex>
#include <atomic>
#include "imgui/imgui.h"
#include "Dune/Common/Types.h"
#include "Dune/Core/ECS/ID.h"

#define RESERVED_ENTITIES 4096