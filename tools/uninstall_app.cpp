#include "SetupCommon.h"
#include <shlobj.h>

namespace
{
std::wstring ReadRegisteredDllPath()
{
    const std::wstring keyPath = std::wstring(L"SOFTWARE\\Classes\\CLSID\\") + kClsidCR3WICDecoderStr + L"\\InprocServer32";
    wchar_t buf[MAX_PATH * 2]{};
    DWORD size = sizeof(buf);
    if (RegGetValueW(
            HKEY_LOCAL_MACHINE,
            keyPath.c_str(),
            nullptr,
            RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ,
            nullptr,
            buf,
            &size) == ERROR_SUCCESS)
    {
        return std::wstring(buf);
    }
    return {};
}

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

    const std::wstring installDir = cr3setup::GetProgramFilesInstallDir();
    std::wstring dllPath = ReadRegisteredDllPath();
    if (dllPath.empty())
    {
        const std::wstring fallback = installDir + L"\\CR3WICCodec.dll";
        if (cr3setup::FileExists(fallback))
            dllPath = fallback;
    }

    if (!dllPath.empty() && cr3setup::FileExists(dllPath))
    {
        wchar_t sysDir[MAX_PATH]{};
        GetSystemDirectoryW(sysDir, MAX_PATH);
        const std::wstring regCmd = L"\"" + std::wstring(sysDir) + L"\\regsvr32.exe\" /u /s \"" + dllPath + L"\"";
        DWORD regExit = 0;
        cr3setup::RunProcessSilent(regCmd, regExit);
    }

    const std::wstring clsidKey = std::wstring(L"SOFTWARE\\Classes\\CLSID\\") + kClsidCR3WICDecoderStr;
    const std::wstring instanceKey = std::wstring(L"SOFTWARE\\Classes\\CLSID\\") + kWicBitmapDecodersCategoryStr + L"\\Instance\\" + kClsidCR3WICDecoderStr;
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, instanceKey.c_str());
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, clsidKey.c_str());

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    RemoveDirectoryContentsOrSchedule(installDir);

    const bool scheduled = cr3setup::DirExists(installDir);
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
