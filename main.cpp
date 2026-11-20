#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include <Mod/CppUserModBase.hpp>

namespace
{
    constexpr DWORD GameTimestamp = 0x5e0517a6;
    constexpr DWORD GameImageSize = 0x0fa65000;

    // Regular customization has already loaded the saved avatar and figure by
    // this point. Route it to the native gender picker while retaining FirstTime=0.
    constexpr unsigned char StartCustomization[] = {
        0x80, 0xbf, 0x21, 0x05, 0x00, 0x00, 0x00, 0x48, 0x8b, 0x8f, 0x00, 0x05, 0x00, 0x00,
        0x74, 0x09, 0x33, 0xd2, 0xe8, 0xd0, 0x8a, 0x03, 0x00, 0xeb, 0x07,
        0xb2, 0x01, 0xe8, 0xa7, 0x8a, 0x03, 0x00,
    };

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

    struct CodePatch
    {
        DWORD functionRva;
        const unsigned char* signature;
        size_t signatureSize;
        DWORD offset;
        SHORT original;
        SHORT replacement;
    };

    constexpr SHORT NopPair = static_cast<SHORT>(0x9090);
    constexpr std::array<CodePatch, 3> Patches = {{
        {0x0608aa00, AppearanceComplete, sizeof(AppearanceComplete), 14, 0x0c74, NopPair},
        {0x0608f710, AppearanceCompleteHandled, sizeof(AppearanceCompleteHandled), 18, 0x1374, NopPair},
        // Set the short jump's displacement to zero. Keep the next opcode (0x33)
        // in the same aligned word so the write remains atomic.
        {0x003bfdd9, StartCustomization, sizeof(StartCustomization), 15, 0x3309, 0x3300},
    }};
    static_assert(std::all_of(Patches.begin(), Patches.end(), [](const CodePatch& patch) {
        return (patch.functionRva + patch.offset) % alignof(SHORT) == 0;
    }));

    constexpr DWORD StartAppearance = 0x003f88a0;
    constexpr DWORD StartGender = 0x003f88c0;
    constexpr size_t DelegateGameMode = 0x08;
    constexpr size_t GameModeAgent = 0x500;
    constexpr size_t GameModeFirstTime = 0x521;
    constexpr int Confirmed = 0;
    constexpr int Back = 2;

    unsigned char* gameImage{};

    bool openRegularStep(const unsigned char* delegate, DWORD step, bool allowBack)
    {
        // These delegates belong to VGCustomizeGameMode and hold its instance
        // directly. The current preview avatar already contains the editable look.
        const auto* gameMode = *reinterpret_cast<unsigned char* const*>(delegate + DelegateGameMode);
        if (!gameMode || gameMode[GameModeFirstTime])
        {
            return false;
        }
        auto* agent = *reinterpret_cast<void* const*>(gameMode + GameModeAgent);
        if (!agent)
        {
            return false;
        }
        reinterpret_cast<void (*)(void*, bool)>(gameImage + step)(agent, allowBack);
        return true;
    }

    template <DWORD Original, int RoutedResult, DWORD NextStep, bool AllowBack, typename Result>
    Result routeCustomization(unsigned char* delegate, int result)
    {
        if (result == RoutedResult && openRegularStep(delegate, NextStep, AllowBack))
        {
            if constexpr (std::is_same_v<Result, bool>)
            {
                return true;
            }
            else
            {
                return;
            }
        }
        return reinterpret_cast<Result (*)(unsigned char*, int)>(gameImage + Original)(delegate, result);
    }

    struct DelegatePatch
    {
        DWORD slotRva;
        DWORD originalRva;
        void* replacement;
    };

    // Both the original and cloned delegate have an Execute slot. The clone
    // also has ExecuteIfBound. Keep both native first-time paths available.
    const std::array<DelegatePatch, 6> DelegatePatches = {{
        {0x0394dd10, 0x003aac40, reinterpret_cast<void*>(&routeCustomization<0x003aac40, Confirmed, StartAppearance, true, void>)},
        {0x0394dd80, 0x003aac40, reinterpret_cast<void*>(&routeCustomization<0x003aac40, Confirmed, StartAppearance, true, void>)},
        {0x0394dd88, 0x003ab130, reinterpret_cast<void*>(&routeCustomization<0x003ab130, Confirmed, StartAppearance, true, bool>)},
        {0x0394ded0, 0x003aabd0, reinterpret_cast<void*>(&routeCustomization<0x003aabd0, Back, StartGender, false, void>)},
        {0x0394df40, 0x003aabd0, reinterpret_cast<void*>(&routeCustomization<0x003aabd0, Back, StartGender, false, void>)},
        {0x0394df48, 0x003ab090, reinterpret_cast<void*>(&routeCustomization<0x003ab090, Back, StartGender, false, bool>)},
    }};

    void log(const char* message)
    {
        OutputDebugStringA(message);
        HMODULE module{};
        wchar_t path[MAX_PATH];
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(&Patches), &module))
        {
            return;
        }
        const DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
        if (!length || length >= MAX_PATH)
        {
            return;
        }
        auto* filename = wcsrchr(path, L'\\');
        if (!filename || wcscpy_s(filename + 1, MAX_PATH - (filename + 1 - path), L"EasyGenderAndNameChange.log"))
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

    bool replaceWord(unsigned char* address, SHORT from, SHORT to)
    {
        DWORD previousProtection{};
        if (!VirtualProtect(address, sizeof(SHORT), PAGE_EXECUTE_READWRITE, &previousProtection))
        {
            return false;
        }
        // The patched words are naturally aligned. An atomic write avoids exposing a
        // partially replaced instruction if the game thread is already running.
        const bool replaced = InterlockedCompareExchange16(reinterpret_cast<volatile SHORT*>(address), to, from) == from;
        FlushInstructionCache(GetCurrentProcess(), address, sizeof(SHORT));
        DWORD unused{};
        if (!VirtualProtect(address, sizeof(SHORT), previousProtection, &unused))
        {
            log("[EasyGenderAndNameChange] Could not restore page protection.\r\n");
        }
        return replaced;
    }

    bool replaceDelegate(DWORD slotRva, void* from, void* to)
    {
        auto* slot = reinterpret_cast<void* volatile*>(gameImage + slotRva);
        DWORD previousProtection{};
        if (!VirtualProtect(gameImage + slotRva, sizeof(void*), PAGE_READWRITE, &previousProtection))
        {
            return false;
        }
        const bool replaced = InterlockedCompareExchangePointer(slot, to, from) == from;
        DWORD unused{};
        if (!VirtualProtect(gameImage + slotRva, sizeof(void*), previousProtection, &unused))
        {
            log("[EasyGenderAndNameChange] Could not restore delegate page protection.\r\n");
        }
        return replaced;
    }

    class EasyGenderAndNameChange final : public RC::CppUserModBase
    {
    public:
        EasyGenderAndNameChange()
        {
            ModName = STR("Easy Gender and Name Change");
            ModVersion = STR("1.1.1");
            ModDescription = STR("Enables gender selection and name entry in regular Fatal Bullet customization.");
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
                log("[EasyGenderAndNameChange] Unsupported game build.\r\n");
                return;
            }
            for (const auto& patch : Patches)
            {
                if (std::memcmp(m_image + patch.functionRva, patch.signature, patch.signatureSize) != 0)
                {
                    log("[EasyGenderAndNameChange] Customization code differs from the supported game build.\r\n");
                    return;
                }
            }
            for (const auto& patch : DelegatePatches)
            {
                if (*reinterpret_cast<void* const*>(m_image + patch.slotRva) != m_image + patch.originalRva)
                {
                    log("[EasyGenderAndNameChange] Customization delegate differs from the supported game build.\r\n");
                    return;
                }
            }
            gameImage = m_image;
            for (const auto& patch : DelegatePatches)
            {
                if (!replaceDelegate(patch.slotRva, gameImage + patch.originalRva, patch.replacement))
                {
                    log("[EasyGenderAndNameChange] Could not enable direct transitions between gender and appearance.\r\n");
                    restore();
                    return;
                }
                ++m_delegatesApplied;
            }
            for (const auto& patch : Patches)
            {
                if (!replaceWord(m_image + patch.functionRva + patch.offset, patch.original, patch.replacement))
                {
                    log("[EasyGenderAndNameChange] Could not enable gender selection and name entry.\r\n");
                    restore();
                    return;
                }
                ++m_applied;
            }
            log("[EasyGenderAndNameChange] Native gender selection and name entry enabled for regular customization.\r\n");
        }

        ~EasyGenderAndNameChange() override
        {
            restore();
        }

    private:
        void restore()
        {
            while (m_applied > 0)
            {
                const auto& patch = Patches[--m_applied];
                if (!replaceWord(m_image + patch.functionRva + patch.offset, patch.replacement, patch.original))
                {
                    log("[EasyGenderAndNameChange] Could not restore customization code. Restart the game.\r\n");
                }
            }
            while (m_delegatesApplied > 0)
            {
                const auto& patch = DelegatePatches[--m_delegatesApplied];
                if (!replaceDelegate(patch.slotRva, patch.replacement, gameImage + patch.originalRva))
                {
                    log("[EasyGenderAndNameChange] Could not restore a customization delegate. Restart the game.\r\n");
                }
            }
        }

        unsigned char* m_image{};
        size_t m_applied{};
        size_t m_delegatesApplied{};
    };
}

extern "C" __declspec(dllexport) RC::CppUserModBase* start_mod()
{
    return new EasyGenderAndNameChange();
}

extern "C" __declspec(dllexport) void uninstall_mod(RC::CppUserModBase* mod)
{
    delete mod;
}
