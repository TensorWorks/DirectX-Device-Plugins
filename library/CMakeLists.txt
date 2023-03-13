cmake_minimum_required(VERSION 3.22)
project(directx-device-discovery)

# Set the C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Locate our dependencies (these will be provided by vcpkg)
find_package(cppwinrt CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(wil CONFIG REQUIRED)

# Build our shared library
add_library(directx-device-discovery SHARED
	src/AdapterEnumeration.cpp
	src/D3DHelpers.cpp
	src/DeviceDiscovery.cpp
	src/DeviceDiscoveryImp.cpp
	src/DllMain.cpp
	src/ErrorHandling.cpp
	src/RegistryQuery.cpp
	src/SafeArray.cpp
	src/WmiQuery.cpp
)
target_link_libraries(directx-device-discovery PRIVATE
	dxcore.lib
	dxguid.lib
	fmt::fmt-header-only
	gdi32.lib
	Microsoft::CppWinRT
	spdlog::spdlog_header_only
	wbemuuid.lib
	WIL::WIL
	WindowsApp.lib
)
set_property(TARGET directx-device-discovery PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
target_include_directories(directx-device-discovery PUBLIC include)
target_precompile_headers(directx-device-discovery PRIVATE src/pch.h)

# Build our test executable
add_executable(test-device-discovery-cpp test/test-device-discovery-cpp.cpp)
set_property(TARGET test-device-discovery-cpp PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
target_link_libraries(test-device-discovery-cpp PRIVATE Microsoft::CppWinRT directx-device-discovery)

# Install the shared library and the test executable to the top-level bin directory
install(
	TARGETS directx-device-discovery test-device-discovery-cpp
	RUNTIME DESTINATION bin
)
