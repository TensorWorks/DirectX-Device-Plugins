BOOL APIENTRY DllMain(HINSTANCE hModule, DWORD dwReason, PVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		// Disable logging by default
		spdlog::set_level(spdlog::level::off);
	}
	
	return TRUE;
}
