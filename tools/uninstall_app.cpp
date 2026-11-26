#include "SetupCommon.h"
#include <shlobj.h>

namespace
{
void RemoveDirectoryContentsOrSchedule(const std::wstring& dir)
{
    if (!cr3setup::DirExists(dir))
        return;

    WIN32_FIND_DATAW ffd{};
    HANDLE hFind = FindFirstFileW((dir + L"\\*").c_str(), &ffd);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0)
                continue;
            const std::wstring full = dir + L"\\" + ffd.cFileName;
            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                RemoveDirectoryContentsOrSchedule(full);
            }
            else
            {
                if (!DeleteFileW(full.c_str()))
                {
                    MoveFileExW(full.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
                }
            }
        } while (FindNextFileW(hFind, &ffd));
        FindClose(hFind);
    }

    if (!RemoveDirectoryW(dir.c_str()))
    {
        MoveFileExW(dir.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
    }
}
} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    const int answer = MessageBoxW(
        nullptr,
        cr3setup::L(
            L"Do you really want to uninstall the Canon CR3 WIC RAW Codec?",
            L"Moechten Sie den Canon CR3 WIC RAW Codec wirklich vollstaendig deinstallieren?"),
        cr3setup::kAppTitle,
        MB_YESNO | MB_ICONQUESTION);
    if (answer != IDYES)
    {
        CoUninitialize();
        return 0;
    }

    const std::wstring adminDir = cr3setup::GetProgramFilesInstallDir();
    const std::wstring userDir = cr3setup::GetPerUserInstallDir();
    const std::wstring hklmDll = cr3setup::ReadRegisteredDllPath(HKEY_LOCAL_MACHINE);
    const std::wstring hkcuDll = cr3setup::ReadRegisteredDllPath(HKEY_CURRENT_USER);
    const bool hasSystemInstall = !hklmDll.empty() || cr3setup::DirExists(adminDir);

    if (hasSystemInstall && !cr3setup::IsProcessElevated())
    {
        wchar_t selfPath[MAX_PATH]{};
        GetModuleFileNameW(nullptr, selfPath, MAX_PATH);
        DWORD elevExit = 0;
        if (!cr3setup::RunShellProcessWait(selfPath, L"", true, elevExit))
        {
            MessageBoxW(
                nullptr,
                cr3setup::L(
                    L"Administrator privileges are required to uninstall the system-wide installation.",
                    L"Fuer die systemweite Deinstallation sind Administratorrechte erforderlich."),
                cr3setup::kAppTitle,
                MB_OK | MB_ICONWARNING);
            CoUninitialize();
            return 1;
        }
        CoUninitialize();
        return static_cast<int>(elevExit);
    }

    const std::wstring clsidKey = std::wstring(L"SOFTWARE\\Classes\\CLSID\\") + kClsidCR3WICDecoderStr;
    const std::wstring instanceKey = std::wstring(L"SOFTWARE\\Classes\\CLSID\\") + kWicBitmapDecodersCategoryStr + L"\\Instance\\" + kClsidCR3WICDecoderStr;
    wchar_t sysDir[MAX_PATH]{};
    GetSystemDirectoryW(sysDir, MAX_PATH);

    auto unregisterScope = [&](HKEY root, const std::wstring& registeredDll, const std::wstring& folder, bool perUser) {
        std::wstring dllPath = registeredDll;
        if (dllPath.empty())
        {
            const std::wstring fallback = folder + L"\\CR3WICCodec.dll";
            if (cr3setup::FileExists(fallback))
                dllPath = fallback;
        }
        if (!dllPath.empty() && cr3setup::FileExists(dllPath))
        {
            const std::wstring regCmd = std::wstring(L"\"") + sysDir + L"\\regsvr32.exe\" /u /s" +
                (perUser ? L" /n /i:user" : L"") + L" \"" + dllPath + L"\"";
            DWORD regExit = 0;
            cr3setup::RunProcessSilent(regCmd, regExit);
        }
        RegDeleteTreeW(root, instanceKey.c_str());
        RegDeleteTreeW(root, clsidKey.c_str());
        RemoveDirectoryContentsOrSchedule(folder);
    };

    unregisterScope(HKEY_CURRENT_USER, hkcuDll, userDir, true);
    if (cr3setup::IsProcessElevated())
    {
        unregisterScope(HKEY_LOCAL_MACHINE, hklmDll, adminDir, false);
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    const bool scheduled = cr3setup::DirExists(userDir) || (cr3setup::IsProcessElevated() && cr3setup::DirExists(adminDir));
    MessageBoxW(
        nullptr,
        scheduled
            ? cr3setup::L(
                  L"Canon CR3 WIC RAW Codec has been unregistered.\n"
                  L"Some in-use DLL files are scheduled for automatic removal on the next Windows restart.",
                  L"Canon CR3 WIC RAW Codec wurde deregistriert.\n"
                  L"Einige geoeffnete DLL-Dateien werden beim naechsten Windows-Neustart automatisch entfernt.")
            : cr3setup::L(
                  L"Canon CR3 WIC RAW Codec has been completely removed.",
                  L"Canon CR3 WIC RAW Codec wurde vollstaendig entfernt."),
        cr3setup::kAppTitle,
        MB_OK | MB_ICONINFORMATION);

    CoUninitialize();
    return 0;
}
