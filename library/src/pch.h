#include <wil/cppwinrt.h>
#include <wil/resource.h>
#include <wil/result.h>
#include <wil/win32_helpers.h>
#include <winrt/base.h>
#include <d3dkmthk.h>
#include <d3dukmdt.h>
#include <dxcore.h>
#include <OleAuto.h>

// Prevent the GetObject => GetObjectW macro definition from the Windows headers from interfering with the IDL for IWbemServices
#ifdef GetObject
#undef GetObject
#endif
#include <WbemIdl.h>

// Enable wchar_t support for filenames in spdlog
#define SPDLOG_WCHAR_FILENAMES
#include <spdlog/spdlog.h>
#define LOG(...) SPDLOG_INFO(__VA_ARGS__)

// Include the range formatting support from fmt to facilitate logging container types
#include <fmt/ranges.h>
#define FMT(x) fmt::format(L"{}", x)

#include <filesystem>
#include <map>
#include <memory>
#include <initializer_list>
#include <iomanip>
#include <set>
#include <stdint.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
