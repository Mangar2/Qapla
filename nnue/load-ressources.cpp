/*
 * Reads a net that is embedded in the program as a Windows resource.
 *
 * Only Windows has such a thing. Everywhere else the net comes out of a file, which
 * Network::load does anyway, so the function below is never called there - it exists so
 * that network.cpp links, and says what happened if it ever is called.
 */

#include <stdexcept>
#include "load-ressources.h"

#ifdef _WIN32
#include <windows.h>

std::istringstream load_embedded_resource(const std::string& resourceName) {
    HMODULE hModule = GetModuleHandle(nullptr);
    HRSRC hRes = FindResource(hModule, resourceName.c_str(), RT_RCDATA);
    if (!hRes) {
        throw std::runtime_error("Resource not found.");
    }

    HGLOBAL hData = LoadResource(hModule, hRes);
    DWORD size = SizeofResource(hModule, hRes);
    const void* data = LockResource(hData);

    return std::istringstream(std::string(static_cast<const char*>(data), size));
}

#else

std::istringstream load_embedded_resource(const std::string& resourceName) {
    throw std::runtime_error("no embedded resources on this platform, asked for "
        + resourceName + " - give the net as a file instead");
}

#endif
