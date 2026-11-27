#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <cstdint>
#include <string>
#include <vector>
#include "../src/Guid.h"

namespace cr3setup
{
inline constexpr wchar_t kAppTitle[] = L"Canon CR3 WIC RAW Codec";

inline bool IsGermanUI()
{
    return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_GERMAN;
}

inline const wchar_t* L(const wchar_t* en, const wchar_t* de)
{
    return IsGermanUI() ? de : en;
}

inline std::wstring GetExecutableDir()
{
    std::wstring path(MAX_PATH, L'\0');
    DWORD len = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    while (len >= path.size() - 1)
    {
        path.resize(path.size() * 2);
        len = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    }
    if (len == 0)
        return {};
    path.resize(len);
    const size_t slash = path.find_last_of(L"\\/");
    return (slash == std::wstring::npos) ? std::wstring{} : path.substr(0, slash);
}

inline bool IsProcessElevated()
{
    return IsUserAnAdmin() != FALSE;
}

inline std::wstring GetProgramFilesInstallDir()
{
    PWSTR rawPath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramFiles, 0, nullptr, &rawPath)) && rawPath)
    {
        std::wstring result = std::wstring(rawPath) + L"\\CR3WICCodec";
        CoTaskMemFree(rawPath);
        return result;
    }
    return L"C:\\Program Files\\CR3WICCodec";
}

inline std::wstring GetPerUserInstallDir()
{
    PWSTR rawPath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &rawPath)) && rawPath)
    {
        std::wstring result = std::wstring(rawPath) + L"\\Programs\\CR3WICCodec";
        CoTaskMemFree(rawPath);
        return result;
    }
    return L"CR3WICCodec";
}

inline int EnsureDirRecursive(const std::wstring& path)
{
    return SHCreateDirectoryExW(nullptr, path.c_str(), nullptr);
}

inline std::wstring ReadRegisteredDllPath(HKEY root)
{
    const std::wstring keyPath = std::wstring(L"SOFTWARE\\Classes\\CLSID\\") + kClsidCR3WICDecoderStr + L"\\InprocServer32";
    wchar_t buf[MAX_PATH * 2]{};
    DWORD size = sizeof(buf);
    if (RegGetValueW(
            root,
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

inline bool RunShellProcessWait(const std::wstring& exePath, const std::wstring& params, bool runAsAdmin, DWORD& exitCode)
{
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = runAsAdmin ? L"runas" : nullptr;
    sei.lpFile = exePath.c_str();
    sei.lpParameters = params.empty() ? nullptr : params.c_str();
    sei.nShow = SW_SHOW;

    if (!ShellExecuteExW(&sei))
    {
        exitCode = GetLastError();
        return false;
    }

    if (sei.hProcess)
    {
        WaitForSingleObject(sei.hProcess, INFINITE);
        GetExitCodeProcess(sei.hProcess, &exitCode);
        CloseHandle(sei.hProcess);
    }
    else
    {
        exitCode = 0;
    }
    return true;
}

inline bool FileExists(const std::wstring& path)
{
    const DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

inline bool DirExists(const std::wstring& path)
{
    const DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

inline std::wstring FormatWin32Error(DWORD err)
{
    wchar_t* msg = nullptr;
    const DWORD len = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&msg),
        0,
        nullptr);
    std::wstring out = (len > 0 && msg) ? std::wstring(msg, len) : std::wstring(L"Fehlercode ") + std::to_wstring(err);
    if (msg)
        LocalFree(msg);
    return out;
}

inline bool RunProcessSilent(const std::wstring& cmdLine, DWORD& exitCode)
{
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> mutableCmd(cmdLine.begin(), cmdLine.end());
    mutableCmd.push_back(L'\0');

    if (!CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        exitCode = GetLastError();
        return false;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

inline bool GrantAppContainerReadAccess(const std::wstring& folderPath)
{
    PSID appPkgSid = nullptr;
    if (!ConvertStringSidToSidW(L"S-1-15-2-1", &appPkgSid))
        return false;

    PACL oldDacl = nullptr;
    PSECURITY_DESCRIPTOR sd = nullptr;
    DWORD res = GetNamedSecurityInfoW(
        folderPath.c_str(),
        SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION,
        nullptr,
        nullptr,
        &oldDacl,
        nullptr,
        &sd);

    if (res == ERROR_SUCCESS)
    {
        EXPLICIT_ACCESSW ea{};
        ea.grfAccessPermissions = GENERIC_READ | GENERIC_EXECUTE;
        ea.grfAccessMode = GRANT_ACCESS;
        ea.grfInheritance = OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE;
        ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        ea.Trustee.ptstrName = static_cast<LPWSTR>(appPkgSid);

        PACL newDacl = nullptr;
        res = SetEntriesInAclW(1, &ea, oldDacl, &newDacl);
        if (res == ERROR_SUCCESS && newDacl)
        {
            res = SetNamedSecurityInfoW(
                const_cast<LPWSTR>(folderPath.c_str()),
                SE_FILE_OBJECT,
                DACL_SECURITY_INFORMATION,
                nullptr,
                nullptr,
                newDacl,
                nullptr);
            LocalFree(newDacl);
        }
        if (sd)
            LocalFree(sd);
    }

    LocalFree(appPkgSid);
    return res == ERROR_SUCCESS;
}

inline bool RegKeyExists(HKEY root, const std::wstring& subKey)
{
    HKEY key = nullptr;
    const LONG res = RegOpenKeyExW(root, subKey.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &key);
    if (res == ERROR_SUCCESS)
    {
        RegCloseKey(key);
        return true;
    }
    return false;
}

inline constexpr wchar_t kUninstallSubKey[] = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\CR3WICCodec";
inline constexpr wchar_t kLegacyInnoUninstallSubKey[] = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{D7E4A3B1-5C8F-4A9E-9B2D-6F1A8E3C4D50}_is1";

inline void UnregisterUninstallEntry(HKEY regRoot)
{
    RegDeleteTreeW(regRoot, kUninstallSubKey);
    RegDeleteTreeW(regRoot, kLegacyInnoUninstallSubKey);
}

inline bool ReplaceLockedFileStep(const std::wstring& dst, DWORD err, DWORD& outErr)
{
    if (err != ERROR_SHARING_VIOLATION && err != ERROR_ACCESS_DENIED)
    {
        outErr = err;
        return false;
    }
    const std::wstring oldDst = dst + L".old." + std::to_wstring(GetTickCount64());
    if (MoveFileExW(dst.c_str(), oldDst.c_str(), MOVEFILE_REPLACE_EXISTING))
    {
        MoveFileExW(oldDst.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
        return true;
    }
    outErr = GetLastError();
    return false;
}

inline bool CopyFileLockSafe(const std::wstring& src, const std::wstring& dst, DWORD& outErr)
{
    outErr = ERROR_SUCCESS;
    if (CopyFileW(src.c_str(), dst.c_str(), FALSE))
        return true;

    const DWORD err = GetLastError();
    if (ReplaceLockedFileStep(dst, err, outErr))
    {
        if (CopyFileW(src.c_str(), dst.c_str(), FALSE))
            return true;
        outErr = GetLastError();
    }
    return false;
}

inline bool WriteBytesLockSafe(const void* data, DWORD size, const std::wstring& dst, DWORD& outErr)
{
    auto writeOnce = [&]() -> bool {
        HANDLE h = CreateFileW(dst.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            return false;
        DWORD total = 0;
        const BYTE* p = static_cast<const BYTE*>(data);
        while (total < size)
        {
            DWORD written = 0;
            if (!WriteFile(h, p + total, size - total, &written, nullptr))
            {
                const DWORD err = GetLastError();
                CloseHandle(h);
                SetLastError(err);
                return false;
            }
            total += written;
        }
        CloseHandle(h);
        return true;
    };

    outErr = ERROR_SUCCESS;
    if (writeOnce())
        return true;

    const DWORD err = GetLastError();
    if (ReplaceLockedFileStep(dst, err, outErr))
    {
        if (writeOnce())
            return true;
        outErr = GetLastError();
    }
    return false;
}
} // namespace cr3setup
