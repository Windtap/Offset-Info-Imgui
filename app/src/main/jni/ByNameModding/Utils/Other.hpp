#pragma once

#include <string>
#include <sstream>
#ifdef _WIN32
#include <psapi.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace BNM
{
    namespace Utils
    {
        std::string NumberToHex(uint64_t number)
        {
            std::stringstream stream;
            stream << std::hex << number;

            return stream.str();
        }

        std::string ByteToHex(uint64_t number)
        {
            std::stringstream stream;
            if (number < 16)
                stream << "0";
            stream << std::hex << number;

            return stream.str();
        }

#ifdef _WIN32
        MODULEINFO GetModuleInfo(const char *szModule)
        {
            MODULEINFO modinfo = {0};
            HMODULE hModule = GetModuleHandleA(szModule);
            if (hModule == 0)
                return modinfo;
            GetModuleInformation(GetCurrentProcess(), hModule, &modinfo, sizeof(MODULEINFO));
            return modinfo;
        }
#else
        struct ModuleInfo {
            void* lpBaseOfDll;
            unsigned long SizeOfImage;
            void* EntryPoint;
        };
        
        ModuleInfo GetModuleInfo(const char *szModule)
        {
            ModuleInfo modinfo = {0};
            void* handle = dlopen(szModule, RTLD_LAZY | RTLD_NOLOAD);
            if (handle) {
                modinfo.lpBaseOfDll = handle;
                // Note: Getting actual module size on Android requires parsing /proc/self/maps
                // For now, we'll return basic info
            }
            return modinfo;
        }
#endif
    }
}