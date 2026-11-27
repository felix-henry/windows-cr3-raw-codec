#include "InstallCore.h"

namespace
{
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

    if (!cr3setup::CheckHostArchitecture(false))
    {
        CoUninitialize();
        return 1;
    }

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
                L"Bitte entpacken Sie zuerst das vollständige Release-ZIP vor dem Start von Install.exe."),
            cr3setup::kAppTitle,
            MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    const bool isAdmin = cr3setup::IsProcessElevated();
    if (!cr3setup::EnsureVCRedistReady(!isAdmin, false))
    {
        CoUninitialize();
        return 1;
    }

    const std::wstring targetDir = isAdmin ? cr3setup::GetProgramFilesInstallDir() : cr3setup::GetPerUserInstallDir();
    const int dirRes = cr3setup::EnsureDirRecursive(targetDir);
    if (dirRes != ERROR_SUCCESS && dirRes != ERROR_ALREADY_EXISTS && dirRes != ERROR_FILE_EXISTS)
    {
        const std::wstring msg = std::wstring(cr3setup::L(
            L"Could not create installation directory:\n",
            L"Installationsordner konnte nicht erstellt werden:\n")) +
            targetDir + L"\n\n" + cr3setup::FormatWin32Error(static_cast<DWORD>(dirRes));
        MessageBoxW(nullptr, msg.c_str(), cr3setup::kAppTitle, MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    uint64_t totalBytes = 0;
    const wchar_t* requiredFiles[] = {L"CR3WICCodec.dll", L"libraw.dll"};
    for (const wchar_t* name : requiredFiles)
    {
        const std::wstring src = srcDir + L"\\" + name;
        const std::wstring dst = targetDir + L"\\" + name;
        DWORD copyErr = ERROR_SUCCESS;
        if (!cr3setup::CopyFileLockSafe(src, dst, copyErr))
        {
            std::wstring hint;
            if (copyErr == ERROR_SHARING_VIOLATION || copyErr == ERROR_ACCESS_DENIED)
            {
                hint = cr3setup::L(
                    L"\n\nHint: The file may currently be locked by Windows Explorer. "
                    L"Please close Explorer windows (or sign out briefly) and try again.",
                    L"\n\nHinweis: Die Datei ist eventuell noch vom Windows Explorer gesperrt. "
                    L"Bitte Explorer-Fenster schließen (oder kurz abmelden) und erneut versuchen.");
            }
            const std::wstring msg =
                std::wstring(cr3setup::L(L"Failed to copy ", L"Fehler beim Kopieren von ")) +
                name +
                cr3setup::L(L" to:\n", L" nach:\n") +
                dst + L"\n\n" + cr3setup::FormatWin32Error(copyErr) + hint;
            MessageBoxW(nullptr, msg.c_str(), cr3setup::kAppTitle, MB_OK | MB_ICONERROR);
            CoUninitialize();
            return 1;
        }
        WIN32_FILE_ATTRIBUTE_DATA fad{};
        if (GetFileAttributesExW(dst.c_str(), GetFileExInfoStandard, &fad))
            totalBytes += (static_cast<uint64_t>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
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
        const std::wstring src = cr3setup::FileExists(srcDir + L"\\" + name) ? (srcDir + L"\\" + name) : (exeDir + L"\\" + name);
        if (cr3setup::FileExists(src))
        {
            const std::wstring dst = targetDir + L"\\" + name;
            DWORD ignoredErr = 0;
            if (cr3setup::CopyFileLockSafe(src, dst, ignoredErr))
            {
                WIN32_FILE_ATTRIBUTE_DATA fad{};
                if (GetFileAttributesExW(dst.c_str(), GetFileExInfoStandard, &fad))
                    totalBytes += (static_cast<uint64_t>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
            }
        }
    }

    const std::wstring targetScriptsDir = targetDir + L"\\scripts";
    CreateDirectoryW(targetScriptsDir.c_str(), nullptr);
    const wchar_t* scriptFiles[] = {L"install.ps1", L"uninstall.ps1", L"update.ps1"};
    for (const wchar_t* scriptName : scriptFiles)
    {
        const std::wstring rel = std::wstring(L"\\scripts\\") + scriptName;
        const std::wstring src = cr3setup::FileExists(srcDir + rel) ? (srcDir + rel) : (exeDir + rel);
        if (cr3setup::FileExists(src))
        {
            CopyFileW(src.c_str(), (targetScriptsDir + L"\\" + scriptName).c_str(), FALSE);
        }
    }

    const bool done = cr3setup::FinalizeCodecInstall(targetDir, isAdmin, false, totalBytes);
    CoUninitialize();
    return done ? 0 : 1;
}
