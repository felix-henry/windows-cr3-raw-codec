#include "SetupCommon.h"
#include <shellapi.h>

namespace
{
bool IsVCRedistX64Installed()
{
    DWORD installed = 0;
    DWORD size = sizeof(installed);
    if (RegGetValueW(
            HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\x64",
            L"Installed",
            RRF_RT_REG_DWORD,
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
        if (cr3setup::FileExists(vcDll))
            return true;
    }
    return false;
}

std::wstring ResolveSourceDir(const std::wstring& exeDir)
{
    if (cr3setup::FileExists(exeDir + L"\\CR3WICCodec.dll"))
        return exeDir;
    if (cr3setup::FileExists(exeDir + L"\\bin\\CR3WICCodec.dll"))
        return exeDir + L"\\bin";
    return {};
}
} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    const std::wstring exeDir = cr3setup::GetExecutableDir();
    const std::wstring srcDir = ResolveSourceDir(exeDir);
    if (srcDir.empty() || !cr3setup::FileExists(srcDir + L"\\libraw.dll"))
    {
        MessageBoxW(
            nullptr,
            cr3setup::L(
                L"CR3WICCodec.dll or libraw.dll was not found in the package folder.\n"
                L"Please extract the complete release ZIP before running Install.exe.",
                L"CR3WICCodec.dll oder libraw.dll wurde im Paketordner nicht gefunden.\n"
                L"Bitte entpacken Sie zuerst das vollstaendige Release-ZIP vor dem Start von Install.exe."),
            cr3setup::kAppTitle,
            MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    if (!IsVCRedistX64Installed())
    {
        const int choice = MessageBoxW(
            nullptr,
            cr3setup::L(
                L"Microsoft Visual C++ 2015-2022 Redistributable (x64) is not installed.\n"
                L"LibRaw requires this runtime library.\n\n"
                L"Open the official Microsoft download page now?",
                L"Microsoft Visual C++ 2015-2022 Redistributable (x64) ist nicht installiert.\n"
                L"LibRaw benoetigt diese Laufzeitbibliothek.\n\n"
                L"Soll die offizielle Microsoft-Downloadseite jetzt geoeffnet werden?"),
            cr3setup::kAppTitle,
            MB_YESNO | MB_ICONWARNING);
        if (choice == IDYES)
        {
            ShellExecuteW(nullptr, L"open", L"https://aka.ms/vs/17/release/vc_redist.x64.exe", nullptr, nullptr, SW_SHOWNORMAL);
        }
        CoUninitialize();
        return 1;
    }

    const std::wstring targetDir = cr3setup::GetProgramFilesInstallDir();
    if (!CreateDirectoryW(targetDir.c_str(), nullptr))
    {
        const DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS)
        {
            const std::wstring msg = std::wstring(cr3setup::L(
                L"Could not create installation directory:\n",
                L"Installationsordner konnte nicht erstellt werden:\n")) +
                targetDir + L"\n\n" + cr3setup::FormatWin32Error(err);
            MessageBoxW(nullptr, msg.c_str(), cr3setup::kAppTitle, MB_OK | MB_ICONERROR);
            CoUninitialize();
            return 1;
        }
    }

    const wchar_t* requiredFiles[] = {L"CR3WICCodec.dll", L"libraw.dll"};
    for (const wchar_t* name : requiredFiles)
    {
        const std::wstring src = srcDir + L"\\" + name;
        const std::wstring dst = targetDir + L"\\" + name;
        if (!CopyFileW(src.c_str(), dst.c_str(), FALSE))
        {
            const DWORD err = GetLastError();
            std::wstring hint;
            if (err == ERROR_SHARING_VIOLATION || err == ERROR_ACCESS_DENIED)
            {
                hint = cr3setup::L(
                    L"\n\nHint: The file may currently be locked by Windows Explorer. "
                    L"Please close Explorer windows (or sign out briefly) and try again.",
                    L"\n\nHinweis: Die Datei ist eventuell noch vom Windows Explorer gesperrt. "
                    L"Bitte Explorer-Fenster schliessen (oder kurz abmelden) und erneut versuchen.");
            }
            const std::wstring msg =
                std::wstring(cr3setup::L(L"Failed to copy ", L"Fehler beim Kopieren von ")) +
                name +
                cr3setup::L(L" to:\n", L" nach:\n") +
                dst + L"\n\n" + cr3setup::FormatWin32Error(err) + hint;
            MessageBoxW(nullptr, msg.c_str(), cr3setup::kAppTitle, MB_OK | MB_ICONERROR);
            CoUninitialize();
            return 1;
        }
    }

    const wchar_t* optionalFiles[] = {
        L"vcomp140.dll",
        L"test_decoder.exe",
        L"Uninstall.exe",
        L"README.md",
        L"LICENSE",
        L"THIRD_PARTY_NOTICES.md",
        L"LICENSE.LGPL.LibRaw.txt",
        L"LICENSE.CDDL.LibRaw.txt",
        L"COPYRIGHT.LibRaw.txt"};
    for (const wchar_t* name : optionalFiles)
    {
        const std::wstring src = (cr3setup::FileExists(srcDir + L"\\" + name)) ? (srcDir + L"\\" + name) : (exeDir + L"\\" + name);
        if (cr3setup::FileExists(src))
        {
            CopyFileW(src.c_str(), (targetDir + L"\\" + name).c_str(), FALSE);
        }
    }

    cr3setup::GrantAppContainerReadAccess(targetDir);

    wchar_t sysDir[MAX_PATH]{};
    GetSystemDirectoryW(sysDir, MAX_PATH);
    const std::wstring targetDll = targetDir + L"\\CR3WICCodec.dll";
    const std::wstring regCmd = L"\"" + std::wstring(sysDir) + L"\\regsvr32.exe\" /s \"" + targetDll + L"\"";

    DWORD regExit = 1;
    if (!cr3setup::RunProcessSilent(regCmd, regExit) || regExit != 0)
    {
        const std::wstring msg =
            std::wstring(cr3setup::L(
                L"COM/WIC registration (regsvr32) failed (code ",
                L"COM/WIC-Registrierung (regsvr32) ist fehlgeschlagen (Code ")) +
            std::to_wstring(regExit) + L").";
        MessageBoxW(nullptr, msg.c_str(), cr3setup::kAppTitle, MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    const std::wstring clsidBase = std::wstring(L"SOFTWARE\\Classes\\CLSID\\") + kClsidCR3WICDecoderStr;
    const std::wstring instanceKey = std::wstring(L"SOFTWARE\\Classes\\CLSID\\") + kWicBitmapDecodersCategoryStr + L"\\Instance\\" + kClsidCR3WICDecoderStr;

    if (!cr3setup::RegKeyExists(HKEY_LOCAL_MACHINE, clsidBase + L"\\Patterns\\0") ||
        !cr3setup::RegKeyExists(HKEY_LOCAL_MACHINE, instanceKey))
    {
        MessageBoxW(
            nullptr,
            cr3setup::L(
                L"Could not verify the WIC registry entries.",
                L"Die WIC-Registry-Eintraege konnten nicht verifiziert werden."),
            cr3setup::kAppTitle,
            MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    const std::wstring successMsg =
        std::wstring(cr3setup::L(
            L"Canon CR3 WIC RAW Codec has been installed successfully!\n\nInstall path:\n",
            L"Canon CR3 WIC RAW Codec wurde erfolgreich installiert!\n\nInstallationspfad:\n")) +
        targetDir +
        cr3setup::L(
            L"\n\n.CR3 files in Windows Explorer will now render via LibRaw (14-bit RAW).\n\n"
            L"Tip: If previously cached thumbnails still appear, clear them via "
            L"Windows Disk Cleanup (cleanmgr.exe -> Thumbnails).",
            L"\n\n.CR3-Dateien werden im Windows Explorer nun ueber LibRaw (14-Bit RAW) gerendert.\n\n"
            L"Tipp: Falls alte gecachte Thumbnails noch sichtbar sind, koennen Sie diese ueber "
            L"die Windows-Datentraegerbereinigung (cleanmgr.exe -> Miniaturansichten) leeren.");

    MessageBoxW(nullptr, successMsg.c_str(), cr3setup::kAppTitle, MB_OK | MB_ICONINFORMATION);

    CoUninitialize();
    return 0;
}
