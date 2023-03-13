#pragma once

using std::wstring;
using std::wstring_view;


// Define Unicode versions of __FILE__ and __FUNCTION__
#define __WIDE2(x) L##x
#define __WIDE1(x) __WIDE2(x)
#define __WFILE__ __WIDE1(__FILE__)
#define __WFUNCTION__ __WIDE1(__FUNCTION__)


// The exception type used to represent all errors inside the device discovery library
class DeviceDiscoveryError
{
	public:
		inline DeviceDiscoveryError() : message(L""), file(L""), function(L""), line(0) {}
		
		inline DeviceDiscoveryError(wstring_view message, wstring_view file, wstring_view function, size_t line) :
			message(message), file(file), function(function), line(line)
		{}
		
		inline DeviceDiscoveryError(wstring_view message, const DeviceDiscoveryError& inner) :
			file(inner.file), function(inner.function), line(inner.line)
		{
			this->message = wstring(message) + L": " + inner.message;
		}
		
		DeviceDiscoveryError(const DeviceDiscoveryError& other) = default;
		DeviceDiscoveryError(DeviceDiscoveryError&& other) = default;
		DeviceDiscoveryError& operator=(const DeviceDiscoveryError& other) = default;
		DeviceDiscoveryError& operator=(DeviceDiscoveryError&& other) = default;
		
		inline operator bool() const {
			return (!this->message.empty());
		}
		
		// Wraps this error in a surrounding error message
		inline DeviceDiscoveryError Wrap(wstring_view message) const {
			return DeviceDiscoveryError(message, *this);
		}
		
		// Formats the error details as a pretty string
		inline wstring Pretty() const
		{
			// Extract the filename from the file path
			wstring filename = std::filesystem::path(this->file).filename().wstring();
			
			// Append the filename, line number and function name to the error message
			wstring fileAndLine = filename + L":" + std::to_wstring(this->line);
			return this->message + L" [" + fileAndLine + L" " + this->function + L"]";
		}
		
		wstring message;
		wstring file;
		wstring function;
		size_t line;
};


// Provides functionality related to managing errors
namespace ErrorHandling
{
	// Returns an error object representing the supplied NTSTATUS code
	DeviceDiscoveryError ErrorForNtStatus(NTSTATUS status, wstring_view file, wstring_view function, size_t line);
	
	// Returns an error object representing the supplied HRESULT code
	inline DeviceDiscoveryError ErrorForHresult(const winrt::hresult& result, wstring_view file, wstring_view function, size_t line)
	{
		try
		{
			winrt::check_hresult(result);
			return DeviceDiscoveryError(L"", file, function, line);
		}
		catch (const winrt::hresult_error& err) {
			return DeviceDiscoveryError(err.message(), file, function, line);
		}
	}
	
	// Returns an error object representing the supplied Win32 error code
	template <typename T>
	inline DeviceDiscoveryError ErrorForWin32(T error, wstring_view file, wstring_view function, size_t line)
	{
		try
		{
			winrt::check_win32(error);
			return DeviceDiscoveryError(L"", file, function, line);
		}
		catch (const winrt::hresult_error& err) {
			return DeviceDiscoveryError(err.message(), file, function, line);
		}
	}
	
	// Convenience macros for automatically filling out error file, function and line details
	#define CreateError(message) DeviceDiscoveryError(message, __WFILE__, __WFUNCTION__, __LINE__)
	#define CheckNtStatus(status) ErrorHandling::ErrorForNtStatus(status, __WFILE__, __WFUNCTION__, __LINE__)
	#define CheckHresult(status) ErrorHandling::ErrorForHresult(status, __WFILE__, __WFUNCTION__, __LINE__)
	#define CheckWin32(status) ErrorHandling::ErrorForWin32(status, __WFILE__, __WFUNCTION__, __LINE__)
	
	// Catches a winrt::hresult_error object and converts it to a DeviceDiscoveryError object
	#define CatchHresult(error, operation) try { operation; error = DeviceDiscoveryError(); } catch (const winrt::hresult_error & err) { error = CreateError(err.message()); }
}
