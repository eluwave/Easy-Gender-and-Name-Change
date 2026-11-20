#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <cstring>

#include <Mod/CppUserModBase.hpp>

namespace
{
    constexpr DWORD GameTimestamp = 0x5e0517a6;
    constexpr DWORD GameImageSize = 0x0fa65000;

    // The game's two appearance-completion callbacks. On confirmation each calls
    // the name-entry state for FirstTime=1 and skips it for FirstTime=0.
    // The other callback outcomes, including Cancel, branch before these jumps.
    constexpr unsigned char AppearanceComplete[] = {
        0x85, 0xd2, 0x75, 0x1d, 0x48, 0x8b, 0x49, 0x08, 0x38, 0x91, 0x21, 0x05, 0x00, 0x00,
        0x74, 0x0c, 0x48, 0x8b, 0x89, 0x00, 0x05, 0x00, 0x00, 0xe9, 0xc4, 0xde, 0x36, 0xfa,
    };
    constexpr unsigned char AppearanceCompleteHandled[] = {
        0x48, 0x83, 0xec, 0x28, 0x85, 0xd2, 0x75, 0x2b, 0x48, 0x8b, 0x49, 0x08,
        0x38, 0x91, 0x21, 0x05, 0x00, 0x00, 0x74, 0x13, 0x48, 0x8b, 0x89, 0x00, 0x05, 0x00,
        0x00, 0xe8, 0xb0, 0x91, 0x36, 0xfa,
    };

    struct Branch
    {
        DWORD functionRva;
        const unsigned char* signature;
        size_t signatureSize;
        DWORD jumpOffset;
        SHORT original;
    };

    constexpr std::array<Branch, 2> Branches = {{
        {0x0608aa00, AppearanceComplete, sizeof(AppearanceComplete), 14, 0x0c74},
        {0x0608f710, AppearanceCompleteHandled, sizeof(AppearanceCompleteHandled), 18, 0x1374},
    }};
    static_assert((Branches[0].functionRva + Branches[0].jumpOffset) % alignof(SHORT) == 0);
    static_assert((Branches[1].functionRva + Branches[1].jumpOffset) % alignof(SHORT) == 0);
    constexpr SHORT NopPair = static_cast<SHORT>(0x9090);

    void log(const char* message)
    {
        OutputDebugStringA(message);
        HMODULE module{};
        wchar_t path[MAX_PATH];
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(&Branches), &module))
        {
            return;
        }
        const DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
        if (!length || length >= MAX_PATH)
        {
            return;
        }
        auto* filename = wcsrchr(path, L'\\');
        if (!filename || wcscpy_s(filename + 1, MAX_PATH - (filename + 1 - path), L"CustomizeNames.log"))
        {
            return;
        }
        const HANDLE file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
                                       FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE)
        {
            DWORD written{};
            WriteFile(file, message, static_cast<DWORD>(std::strlen(message)), &written, nullptr);
            CloseHandle(file);
        }
    }

    bool replaceJump(unsigned char* address, SHORT from, SHORT to)
    {
        DWORD previousProtection{};
        if (!VirtualProtect(address, sizeof(SHORT), PAGE_EXECUTE_READWRITE, &previousProtection))
        {
            return false;
        }
        // Both jumps are naturally aligned. An atomic write avoids exposing a
        // partially replaced instruction if the game thread is already running.
        const bool replaced = InterlockedCompareExchange16(reinterpret_cast<volatile SHORT*>(address), to, from) == from;
        FlushInstructionCache(GetCurrentProcess(), address, sizeof(SHORT));
        DWORD unused{};
        if (!VirtualProtect(address, sizeof(SHORT), previousProtection, &unused))
        {
            log("[CustomizeNames] Could not restore page protection.\r\n");
        }
        return replaced;
    }

    class CustomizeNames final : public RC::CppUserModBase
    {
    public:
        CustomizeNames()
        {
            ModName = STR("CustomizeNames");
            ModVersion = STR("1.0.0");
            ModDescription = STR("Enables the native name-entry step in ordinary Fatal Bullet customization.");
        }

        void on_unreal_init() override
        {
            if (m_applied)
            {
                return;
            }
            m_image = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
            const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(m_image);
            const auto* pe = reinterpret_cast<const IMAGE_NT_HEADERS64*>(m_image + dos->e_lfanew);
            if (pe->FileHeader.TimeDateStamp != GameTimestamp || pe->OptionalHeader.SizeOfImage != GameImageSize)
            {
                log("[CustomizeNames] Unsupported game build.\r\n");
                return;
            }
            for (const auto& branch : Branches)
            {
                if (std::memcmp(m_image + branch.functionRva, branch.signature, branch.signatureSize) != 0)
                {
                    log("[CustomizeNames] Appearance callback differs from the supported game build.\r\n");
                    return;
                }
            }
            for (const auto& branch : Branches)
            {
                if (!replaceJump(m_image + branch.functionRva + branch.jumpOffset, branch.original, NopPair))
                {
                    log("[CustomizeNames] Could not enable name entry.\r\n");
                    restore();
                    return;
                }
                ++m_applied;
            }
            log("[CustomizeNames] Native name entry enabled for regular customization.\r\n");
        }

        ~CustomizeNames() override
        {
            restore();
        }

    private:
        void restore()
        {
            while (m_applied > 0)
            {
                const auto& branch = Branches[--m_applied];
                if (!replaceJump(m_image + branch.functionRva + branch.jumpOffset, NopPair, branch.original))
                {
                    log("[CustomizeNames] Could not restore an appearance callback. Restart the game.\r\n");
                }
            }
        }

        unsigned char* m_image{};
        size_t m_applied{};
    };
}

extern "C" __declspec(dllexport) RC::CppUserModBase* start_mod()
{
    return new CustomizeNames();
}

extern "C" __declspec(dllexport) void uninstall_mod(RC::CppUserModBase* mod)
{
    delete mod;
}
