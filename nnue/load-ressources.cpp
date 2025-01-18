
#include <windows.h>
#include <stdexcept>
#include "load-ressources.h"

std::istringstream load_embedded_resource(const std::string& resourceName) {
    HMODULE hModule = GetModuleHandle(nullptr); // Handle zur aktuellen .exe
    HRSRC hRes = FindResource(hModule, resourceName.c_str(), RT_RCDATA); // Ressource finden
    if (!hRes) {
        throw std::runtime_error("Resource not found.");
    }

    HGLOBAL hData = LoadResource(hModule, hRes); // Ressource laden
    DWORD size = SizeofResource(hModule, hRes); // Größe ermitteln
    const void* data = LockResource(hData);     // Zugriff auf die Daten

    return std::istringstream(std::string(static_cast<const char*>(data), size)); // In Stream packen
}

