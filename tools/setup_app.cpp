#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include "InstallCore.h"
#include <commctrl.h>
#include <compressapi.h>
#include <cstdio>
#include <cstring>

namespace
{
void ReportQuietError(const std::wstring& msg)
{
    HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    if (!hErr || hErr == INVALID_HANDLE_VALUE || GetFileType(hErr) == FILE_TYPE_UNKNOWN)
    {
        if (AttachConsole(ATTACH_PARENT_PROCESS))
        {
            FILE* err = nullptr;
            freopen_s(&err, "CONOUT$", "w", stderr);
        }
        std::fwprintf(stderr, L"%ls\n", msg.c_str());
        return;
    }

    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, msg.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8Len > 1)
    {
        std::string utf8(static_cast<size_t>(utf8Len - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, msg.c_str(), -1, utf8.data(), utf8Len, nullptr, nullptr);
        utf8.push_back('\n');
        DWORD written = 0;
        WriteFile(hErr, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
    }
}
inline constexpr int kBtnPerUser = 101;
inline constexpr int kBtnAllUsers = 102;
inline constexpr int kBtnElevationNeededId = kBtnAllUsers;

uint32_t ReadU32(const BYTE* p)
{
    uint32_t v = 0;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

uint64_t ReadU64(const BYTE* p)
{
    uint64_t v = 0;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

HRESULT CALLBACK ModeDialogCallback(HWND hwnd, UINT msg, WPARAM, LPARAM, LONG_PTR lpRefData)
{
    if (msg == TDN_CREATED && lpRefData == 1)
    {
        SendMessageW(hwnd, TDM_SET_BUTTON_ELEVATION_REQUIRED_STATE, kBtnElevationNeededId, TRUE);
    }
    return S_OK;
}

enum class InstallModeChoice
{
    Cancel,
    PerUser,
    AllUsers
};

InstallModeChoice PromptInstallMode(HINSTANCE hInstance, bool currentlyElevated, const std::wstring& userDir, const std::wstring& adminDir)
{
    const std::wstring title = std::wstring(cr3setup::kAppTitle) + L" v" + CR3_VER_STR_W + L" – Setup";
    const std::wstring perUserText = std::wstring(cr3setup::L(
        L"Install for me only (no administrator rights)\n",
        L"Nur für mich installieren (ohne Administratorrechte)\n")) + userDir;
    const std::wstring allUsersText = std::wstring(cr3setup::L(
        L"Install for all users (administrator)\n",
        L"Für alle Benutzer installieren (Administratorrechte)\n")) + adminDir;

    TASKDIALOG_BUTTON buttons[2]{};
    if (currentlyElevated)
    {
        buttons[0] = {kBtnAllUsers, allUsersText.c_str()};
        buttons[1] = {kBtnPerUser, perUserText.c_str()};
    }
    else
    {
        buttons[0] = {kBtnPerUser, perUserText.c_str()};
        buttons[1] = {kBtnAllUsers, allUsersText.c_str()};
    }

    TASKDIALOGCONFIG cfg{};
    cfg.cbSize = sizeof(cfg);
    cfg.hInstance = hInstance;
    cfg.dwFlags = TDF_USE_COMMAND_LINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT;
    cfg.dwCommonButtons = TDCBF_CANCEL_BUTTON;
    cfg.pszWindowTitle = title.c_str();
    cfg.pszMainIcon = MAKEINTRESOURCEW(1);
    cfg.pszMainInstruction = cr3setup::L(
        L"Install Canon CR3 WIC RAW Codec",
        L"Canon CR3 WIC RAW Codec installieren");
    cfg.pszContent = cr3setup::L(
        L"Renders .CR3 RAW photos in Windows Explorer with true 14-bit colors and EXIF/GPS metadata. "
        L"Existing installations in the chosen folder are updated automatically.",
        L"Stellt .CR3-RAW-Fotos im Windows Explorer in echten 14-Bit-Farben samt EXIF-/GPS-Daten dar. "
        L"Bestehende Installationen im gewählten Ordner werden automatisch aktualisiert.");
    cfg.cButtons = 2;
    cfg.pButtons = buttons;
    cfg.nDefaultButton = currentlyElevated ? kBtnAllUsers : kBtnPerUser;
    cfg.pfCallback = ModeDialogCallback;
    cfg.lpCallbackData = currentlyElevated ? 0 : 1;

    int selectedButton = 0;
    const HRESULT hr = TaskDialogIndirect(&cfg, &selectedButton, nullptr, nullptr);
    if (FAILED(hr) || selectedButton == IDCANCEL)
        return InstallModeChoice::Cancel;
    if (selectedButton == kBtnAllUsers)
        return InstallModeChoice::AllUsers;
    if (selectedButton == kBtnPerUser)
        return InstallModeChoice::PerUser;
    return InstallModeChoice::Cancel;
}

bool ExtractEmbeddedPayload(HINSTANCE hInstance, const std::wstring& targetDir, uint64_t& outWrittenBytes, std::wstring& outErrMsg)
{
    HRSRC resInfo = FindResourceW(hInstance, MAKEINTRESOURCEW(101), RT_RCDATA);
    if (!resInfo)
    {
        outErrMsg = L"Embedded payload resource missing.";
        return false;
    }

    const DWORD resSize = SizeofResource(hInstance, resInfo);
    HGLOBAL resHandle = LoadResource(hInstance, resInfo);
    const BYTE* resBytes = resHandle ? static_cast<const BYTE*>(LockResource(resHandle)) : nullptr;
    if (!resBytes || resSize < 20 || std::memcmp(resBytes, "CR3Z", 4) != 0)
    {
        outErrMsg = L"Embedded payload header is invalid.";
        return false;
    }

    const uint64_t rawSize64 = ReadU64(resBytes + 4);
    const uint64_t compSize64 = ReadU64(resBytes + 12);
    if (rawSize64 == 0 || rawSize64 > 256ULL * 1024ULL * 1024ULL ||
        compSize64 == 0 || 20ULL + compSize64 > resSize)
    {
        outErrMsg = L"Embedded payload size is out of range.";
        return false;
    }

    DECOMPRESSOR_HANDLE decomp = nullptr;
    if (!CreateDecompressor(COMPRESS_ALGORITHM_LZMS, nullptr, &decomp))
    {
        outErrMsg = std::wstring(L"CreateDecompressor failed: ") + cr3setup::FormatWin32Error(GetLastError());
        return false;
    }

    std::vector<BYTE> rawBuf(static_cast<size_t>(rawSize64));
    SIZE_T decompSize = 0;
    const BOOL ok = Decompress(
        decomp,
        resBytes + 20,
        static_cast<SIZE_T>(compSize64),
        rawBuf.data(),
        rawBuf.size(),
        &decompSize);
    const DWORD decompErr = GetLastError();
    CloseDecompressor(decomp);

    if (!ok || decompSize < 8 || std::memcmp(rawBuf.data(), "CR3P", 4) != 0)
    {
        outErrMsg = std::wstring(L"Payload decompression failed (res=") + std::to_wstring(resSize) +
                    L", comp=" + std::to_wstring(compSize64) +
                    L", raw=" + std::to_wstring(rawSize64) + L"): " +
                    cr3setup::FormatWin32Error(decompErr);
        return false;
    }

    size_t pos = 4;
    const uint32_t fileCount = ReadU32(rawBuf.data() + pos);
    pos += 4;
    outWrittenBytes = 0;

    for (uint32_t i = 0; i < fileCount; ++i)
    {
        if (pos + 4 > decompSize)
        {
            outErrMsg = L"Truncated payload entry header.";
            return false;
        }
        const uint32_t nameChars = ReadU32(rawBuf.data() + pos);
        pos += 4;
        const size_t nameBytes = static_cast<size_t>(nameChars) * sizeof(wchar_t);
        if (nameChars == 0 || pos + nameBytes + 4 > decompSize)
        {
            outErrMsg = L"Invalid payload filename record.";
            return false;
        }

        std::wstring relPath(reinterpret_cast<const wchar_t*>(rawBuf.data() + pos), nameChars);
        pos += nameBytes;

        const uint32_t dataSize = ReadU32(rawBuf.data() + pos);
        pos += 4;
        if (pos + dataSize > decompSize)
        {
            outErrMsg = L"Truncated payload file data.";
            return false;
        }

        const std::wstring dstPath = targetDir + L"\\" + relPath;
        const size_t lastSlash = dstPath.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos)
        {
            cr3setup::EnsureDirRecursive(dstPath.substr(0, lastSlash));
        }

        DWORD writeErr = ERROR_SUCCESS;
        if (!cr3setup::WriteBytesLockSafe(rawBuf.data() + pos, dataSize, dstPath, writeErr))
        {
            std::wstring hint;
            if (writeErr == ERROR_SHARING_VIOLATION || writeErr == ERROR_ACCESS_DENIED)
            {
                hint = cr3setup::L(
                    L"\n\nHint: The file may currently be locked by Windows Explorer. "
                    L"Please close Explorer windows (or sign out briefly) and try again.",
                    L"\n\nHinweis: Die Datei ist eventuell noch vom Windows Explorer gesperrt. "
                    L"Bitte Explorer-Fenster schließen (oder kurz abmelden) und erneut versuchen.");
            }
            outErrMsg =
                std::wstring(cr3setup::L(L"Failed to write:\n", L"Fehler beim Schreiben von:\n")) +
                dstPath + L"\n\n" + cr3setup::FormatWin32Error(writeErr) + hint;
            return false;
        }

        outWrittenBytes += dataSize;
        pos += dataSize;
    }

    return true;
}
} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    bool forceAllUsers = false;
    bool forceCurrentUser = false;
    bool quiet = false;

    int argc = 0;
    if (LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc))
    {
        for (int i = 1; i < argc; ++i)
        {
            if (_wcsicmp(argv[i], L"--allusers") == 0 || _wcsicmp(argv[i], L"/ALLUSERS") == 0)
                forceAllUsers = true;
            else if (_wcsicmp(argv[i], L"--currentuser") == 0 || _wcsicmp(argv[i], L"/CURRENTUSER") == 0)
                forceCurrentUser = true;
            else if (_wcsicmp(argv[i], L"/SILENT") == 0 || _wcsicmp(argv[i], L"/VERYSILENT") == 0 ||
                     _wcsicmp(argv[i], L"/quiet") == 0 || _wcsicmp(argv[i], L"--quiet") == 0)
                quiet = true;
        }
        LocalFree(argv);
    }

    if (!cr3setup::CheckHostArchitecture(quiet))
    {
        CoUninitialize();
        return 1;
    }

    const bool currentlyElevated = cr3setup::IsProcessElevated();
    const std::wstring adminDir = cr3setup::GetProgramFilesInstallDir();
    const std::wstring userDir = cr3setup::GetPerUserInstallDir();

    bool installAllUsers = false;
    if (forceAllUsers)
    {
        if (!currentlyElevated)
        {
            wchar_t selfPath[MAX_PATH]{};
            GetModuleFileNameW(nullptr, selfPath, MAX_PATH);
            std::wstring params = L"--allusers";
            if (quiet)
                params += L" /quiet";
            DWORD elevExit = 1;
            const bool ok = cr3setup::RunShellProcessWait(selfPath, params, true, elevExit);
            CoUninitialize();
            return ok ? static_cast<int>(elevExit) : 1;
        }
        installAllUsers = true;
    }
    else if (forceCurrentUser)
    {
        installAllUsers = false;
    }
    else if (quiet)
    {
        installAllUsers = currentlyElevated;
    }
    else
    {
        const InstallModeChoice mode = PromptInstallMode(hInstance, currentlyElevated, userDir, adminDir);
        if (mode == InstallModeChoice::Cancel)
        {
            CoUninitialize();
            return 0;
        }
        if (mode == InstallModeChoice::AllUsers)
        {
            if (!currentlyElevated)
            {
                wchar_t selfPath[MAX_PATH]{};
                GetModuleFileNameW(nullptr, selfPath, MAX_PATH);
                DWORD elevExit = 1;
                const bool ok = cr3setup::RunShellProcessWait(selfPath, L"--allusers", true, elevExit);
                CoUninitialize();
                return ok ? static_cast<int>(elevExit) : 1;
            }
            installAllUsers = true;
        }
    }

    if (!cr3setup::EnsureVCRedistReady(!installAllUsers, quiet))
    {
        CoUninitialize();
        return 1;
    }

    const std::wstring targetDir = installAllUsers ? adminDir : userDir;
    const int dirRes = cr3setup::EnsureDirRecursive(targetDir);
    if (dirRes != ERROR_SUCCESS && dirRes != ERROR_ALREADY_EXISTS && dirRes != ERROR_FILE_EXISTS)
    {
        if (!quiet)
        {
            const std::wstring msg = std::wstring(cr3setup::L(
                L"Could not create installation directory:\n",
                L"Installationsordner konnte nicht erstellt werden:\n")) +
                targetDir + L"\n\n" + cr3setup::FormatWin32Error(static_cast<DWORD>(dirRes));
            MessageBoxW(nullptr, msg.c_str(), cr3setup::kAppTitle, MB_OK | MB_ICONERROR);
        }
        CoUninitialize();
        return 1;
    }

    uint64_t writtenBytes = 0;
    std::wstring extractErr;
    if (!ExtractEmbeddedPayload(hInstance, targetDir, writtenBytes, extractErr))
    {
        if (!quiet)
        {
            MessageBoxW(nullptr, extractErr.c_str(), cr3setup::kAppTitle, MB_OK | MB_ICONERROR);
        }
        else
        {
            ReportQuietError(extractErr);
        }
        CoUninitialize();
        return 1;
    }

    const bool done = cr3setup::FinalizeCodecInstall(targetDir, installAllUsers, quiet, writtenBytes);
    if (!done && quiet)
    {
        ReportQuietError(L"FinalizeCodecInstall returned false.");
    }
    CoUninitialize();
    return done ? 0 : 1;
}
