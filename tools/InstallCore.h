#pragma once

#include "SetupCommon.h"
#include "../src/Version.h"
#include <urlmon.h>

namespace cr3setup
{
#if defined(_M_ARM64)
inline constexpr wchar_t kVcRegSubKey[] = L"SOFTWARE\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\ARM64";
inline constexpr wchar_t kVcDownloadUrl[] = L"https://aka.ms/vs/17/release/vc_redist.arm64.exe";
inline constexpr wchar_t kVcTempFileName[] = L"vc_redist.arm64.exe";
inline constexpr wchar_t kArchLabel[] = L"ARM64";
inline constexpr bool kBinaryIsArm64 = true;
#else
inline constexpr wchar_t kVcRegSubKey[] = L"SOFTWARE\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\x64";
inline constexpr wchar_t kVcDownloadUrl[] = L"https://aka.ms/vs/17/release/vc_redist.x64.exe";
inline constexpr wchar_t kVcTempFileName[] = L"vc_redist.x64.exe";
inline constexpr wchar_t kArchLabel[] = L"x64";
inline constexpr bool kBinaryIsArm64 = false;
#endif

inline bool CheckHostArchitecture(bool quiet)
{
    bool hostIsArm64 = false;
    bool known = false;

    using IsWow64Process2Fn = BOOL(WINAPI*)(HANDLE, USHORT*, USHORT*);
    if (HMODULE k32 = GetModuleHandleW(L"kernel32.dll"))
    {
        auto fn = reinterpret_cast<IsWow64Process2Fn>(GetProcAddress(k32, "IsWow64Process2"));
        if (fn)
        {
            USHORT procMachine = 0;
            USHORT nativeMachine = 0;
            if (fn(GetCurrentProcess(), &procMachine, &nativeMachine))
            {
                known = true;
                hostIsArm64 = (nativeMachine == IMAGE_FILE_MACHINE_ARM64);
            }
        }
    }

    if (!known)
    {
        SYSTEM_INFO si{};
        GetNativeSystemInfo(&si);
        hostIsArm64 = (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64);
    }

    if (hostIsArm64 == kBinaryIsArm64)
        return true;

    if (!quiet)
    {
        const wchar_t* msg = kBinaryIsArm64
            ? L(
                  L"This ARM64 installer is built for Snapdragon / ARM64 Windows PCs.\n"
                  L"Your PC uses x64 (Intel/AMD). Please download CR3WICCodec-Setup-x64-*.exe instead.",
                  L"Dieser ARM64-Installer ist für Snapdragon-/ARM64-Windows-PCs gedacht.\n"
                  L"Ihr PC nutzt x64 (Intel/AMD). Bitte laden Sie stattdessen CR3WICCodec-Setup-x64-*.exe herunter.")
            : L(
                  L"This x64 installer is for Intel/AMD PCs, while your PC runs native Windows ARM64.\n"
                  L"Windows ARM64 Explorer requires CR3WICCodec-Setup-arm64-*.exe.",
                  L"Dieser x64-Installer ist für Intel/AMD-PCs gedacht, Ihr PC nutzt jedoch Windows ARM64.\n"
                  L"Der Windows-ARM64-Explorer benötigt CR3WICCodec-Setup-arm64-*.exe.");
        MessageBoxW(nullptr, msg, kAppTitle, MB_OK | MB_ICONERROR);
    }
    return false;
}

inline bool IsVCRedistInstalled()
{
    DWORD installed = 0;
    DWORD size = sizeof(installed);
    if (RegGetValueW(
            HKEY_LOCAL_MACHINE,
            kVcRegSubKey,
            L"Installed",
            RRF_RT_REG_DWORD | RRF_SUBKEY_WOW6464KEY,
            nullptr,
            &installed,
            &size) == ERROR_SUCCESS &&
        installed == 1)
    {
        return true;
    }

    wchar_t sysDir[MAX_PATH]{};
    if (GetSystemDirectoryW(sysDir, MAX_PATH) > 0)
    {
        const std::wstring vcDll = std::wstring(sysDir) + L"\\vcruntime140_1.dll";
        if (FileExists(vcDll))
            return true;
    }
    return false;
}

inline bool DownloadAndInstallVCRedist(bool runAsAdmin)
{
    wchar_t tempDir[MAX_PATH]{};
    const DWORD tempLen = GetTempPathW(MAX_PATH, tempDir);
    if (tempLen == 0 || tempLen >= MAX_PATH)
        return false;

    const std::wstring installerPath = std::wstring(tempDir) + kVcTempFileName;
    DeleteFileW(installerPath.c_str());

    const HRESULT dlHr = URLDownloadToFileW(nullptr, kVcDownloadUrl, installerPath.c_str(), 0, nullptr);
    if (FAILED(dlHr) || !FileExists(installerPath))
        return false;

    DWORD exitCode = 1;
    const bool launched = RunShellProcessWait(installerPath, L"/install /passive /norestart", runAsAdmin, exitCode);
    DeleteFileW(installerPath.c_str());

    return launched && (exitCode == 0 || exitCode == 3010 || exitCode == 1638) && IsVCRedistInstalled();
}

inline bool EnsureVCRedistReady(bool runAsAdmin, bool quiet)
{
    if (IsVCRedistInstalled())
        return true;

    if (!quiet)
    {
        const std::wstring promptMsg =
            std::wstring(L"Microsoft Visual C++ 2015-2022 Redistributable (") +
            kArchLabel +
            L(
                L") is not installed.\nLibRaw requires this runtime library.\n\n"
                L"Download and install it automatically from Microsoft now?",
                L") ist nicht installiert.\nLibRaw benötigt diese Laufzeitbibliothek.\n\n"
                L"Jetzt automatisch von Microsoft herunterladen und installieren?");

        const int choice = MessageBoxW(nullptr, promptMsg.c_str(), kAppTitle, MB_YESNO | MB_ICONWARNING);
        if (choice != IDYES)
            return false;
    }

    if (!DownloadAndInstallVCRedist(runAsAdmin))
    {
        if (!quiet)
        {
            const int openBrowser = MessageBoxW(
                nullptr,
                L(
                    L"Automatic download or installation failed (e.g. offline or cancelled).\n\n"
                    L"Open the official Microsoft download page in your browser instead?",
                    L"Automatischer Download oder Installation ist fehlgeschlagen (z. B. offline oder abgebrochen).\n\n"
                    L"Stattdessen die offizielle Microsoft-Downloadseite im Browser öffnen?"),
                kAppTitle,
                MB_YESNO | MB_ICONERROR);
            if (openBrowser == IDYES)
            {
                ShellExecuteW(nullptr, L"open", kVcDownloadUrl, nullptr, nullptr, SW_SHOWNORMAL);
            }
        }
        return false;
    }
    return true;
}

inline void SetRegStringValue(HKEY key, const wchar_t* name, const std::wstring& value)
{
    const DWORD bytes = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
    RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()), bytes);
}

inline void RegisterUninstallEntry(HKEY regRoot, const std::wstring& targetDir, uint64_t totalBytes)
{
    UnregisterUninstallEntry(regRoot);

    HKEY key = nullptr;
    if (RegCreateKeyExW(
            regRoot,
            kUninstallSubKey,
            0,
            nullptr,
            REG_OPTION_NON_VOLATILE,
            KEY_WRITE | KEY_WOW64_64KEY,
            nullptr,
            &key,
            nullptr) != ERROR_SUCCESS)
    {
        return;
    }

    const std::wstring uninstallExe = targetDir + L"\\Uninstall.exe";
    SetRegStringValue(key, L"DisplayName", kAppTitle);
    SetRegStringValue(key, L"DisplayVersion", CR3_VER_STR_W);
    SetRegStringValue(key, L"Publisher", L"CR3 WIC Codec Project");
    SetRegStringValue(key, L"URLInfoAbout", L"https://github.com/felix-henry/windows-cr3-raw-codec");
    SetRegStringValue(key, L"InstallLocation", targetDir);
    SetRegStringValue(key, L"DisplayIcon", uninstallExe + L",0");
    SetRegStringValue(key, L"UninstallString", L"\"" + uninstallExe + L"\"");
    SetRegStringValue(key, L"QuietUninstallString", L"\"" + uninstallExe + L"\" /quiet");

    const DWORD estKb = static_cast<DWORD>((totalBytes + 1023ULL) / 1024ULL);
    RegSetValueExW(key, L"EstimatedSize", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&estKb), sizeof(estKb));
    const DWORD one = 1;
    RegSetValueExW(key, L"NoModify", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&one), sizeof(one));
    RegSetValueExW(key, L"NoRepair", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&one), sizeof(one));

    RegCloseKey(key);
}

inline bool FinalizeCodecInstall(const std::wstring& targetDir, bool isAdmin, bool quiet, uint64_t totalBytes)
{
    if (isAdmin)
    {
        GrantAppContainerReadAccess(targetDir);
    }

    wchar_t sysDir[MAX_PATH]{};
    GetSystemDirectoryW(sysDir, MAX_PATH);
    const std::wstring targetDll = targetDir + L"\\CR3WICCodec.dll";
    const std::wstring regCmd = isAdmin
        ? (L"\"" + std::wstring(sysDir) + L"\\regsvr32.exe\" /s \"" + targetDll + L"\"")
        : (L"\"" + std::wstring(sysDir) + L"\\regsvr32.exe\" /s /n /i:user \"" + targetDll + L"\"");

    DWORD regExit = 1;
    if (!RunProcessSilent(regCmd, regExit) || regExit != 0)
    {
        if (!quiet)
        {
            const std::wstring msg =
                std::wstring(L(
                    L"COM/WIC registration (regsvr32) failed (code ",
                    L"COM/WIC-Registrierung (regsvr32) ist fehlgeschlagen (Code ")) +
                std::to_wstring(regExit) + L").";
            MessageBoxW(nullptr, msg.c_str(), kAppTitle, MB_OK | MB_ICONERROR);
        }
        return false;
    }

    const HKEY regRoot = isAdmin ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    const std::wstring clsidBase = std::wstring(L"SOFTWARE\\Classes\\CLSID\\") + kClsidCR3WICDecoderStr;
    const std::wstring instanceKey = std::wstring(L"SOFTWARE\\Classes\\CLSID\\") + kWicBitmapDecodersCategoryStr + L"\\Instance\\" + kClsidCR3WICDecoderStr;

    if (!RegKeyExists(regRoot, clsidBase + L"\\Patterns\\0") ||
        !RegKeyExists(regRoot, instanceKey))
    {
        if (!quiet)
        {
            MessageBoxW(
                nullptr,
                L(
                    L"Could not verify the WIC registry entries.",
                    L"Die WIC-Registry-Einträge konnten nicht verifiziert werden."),
                kAppTitle,
                MB_OK | MB_ICONERROR);
        }
        return false;
    }

    RegisterUninstallEntry(regRoot, targetDir, totalBytes);

    if (!quiet)
    {
        const std::wstring successMsg =
            std::wstring(L(
                L"Canon CR3 WIC RAW Codec has been installed successfully!\n\nInstall path:\n",
                L"Canon CR3 WIC RAW Codec wurde erfolgreich installiert!\n\nInstallationspfad:\n")) +
            targetDir +
            L(
                L"\n\n.CR3 files in Windows Explorer will now render via LibRaw (14-bit RAW).\n\n"
                L"Tip: If old cached thumbnails remain visible, clear them via "
                L"Windows Disk Cleanup (cleanmgr.exe -> Thumbnails).",
                L"\n\n.CR3-Dateien werden im Windows Explorer nun über LibRaw (14-Bit RAW) gerendert.\n\n"
                L"Tipp: Falls alte gecachte Thumbnails noch sichtbar sind, können Sie diese über "
                L"die Windows-Datenträgerbereinigung (cleanmgr.exe -> Miniaturansichten) leeren.");

        MessageBoxW(nullptr, successMsg.c_str(), kAppTitle, MB_OK | MB_ICONINFORMATION);
    }

    return true;
}
} // namespace cr3setup
