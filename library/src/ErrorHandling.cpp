#include "ErrorHandling.h"

DeviceDiscoveryError ErrorHandling::ErrorForNtStatus(NTSTATUS status, wstring_view file, wstring_view function, size_t line)
{
	if (status < 0)
	{
		// Allocate a buffer to hold the error message
		size_t bufSize = 1024;
		auto buffer = std::make_unique<wchar_t[]>(bufSize);
		
		// Attempt to retrieve the error message for the status code
		DWORD length = FormatMessageW(
			FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS,
			GetModuleHandleW(L"ntdll.dll"),
			status,
			0,
			buffer.get(),
			bufSize,
			nullptr
		);
		
		if (length > 0)
		{
			// If the message has a trailing newline then remove it
			wstring message(buffer.get(), length);
			size_t newline = message.find_last_of(L"\r\n");
			if (newline != wstring::npos) {
				message = message.substr(0, newline - 1);
			}
			
			// Return an error with the retrieved message
			return DeviceDiscoveryError(message, file, function, line);
		}
		else
		{
			// Return an error with the hexadecimal representation of the NTSTATUS code
			return DeviceDiscoveryError(
				fmt::format(
					L"Unable to retrieve error message for NTSTATUS code 0x{:0>8X}",
					static_cast<uint32_t>(status)
				),
				file,
				function,
				line
			);
		}
	}
	
	return DeviceDiscoveryError(L"", file, function, line);
}
