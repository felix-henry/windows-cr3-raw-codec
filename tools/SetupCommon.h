#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include "../src/Guid.h"

namespace cr3setup
{
inline constexpr wchar_t kAppTitle[] = L"Canon CR3 WIC RAW Codec";

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
} // namespace cr3setup
