/*
 * Copyright (c) 2026 Masaaki Hamada
 */

#include "framework.h"

#define MODULE_NAME _T("exclude_list")
#define EXCLUDE_FILENAME _T("exclude.txt")


static std::unordered_set<std::wstring> g_excludedProcesses;
static std::unordered_map<HWND, bool> g_processCache;
static std::list<HWND> g_cacheLruList;
static std::mutex g_cacheMutex;


BEGIN_NAMESPACE(processExcluder);

// --- 除外リストとキャッシュ関連 ---
void LoadExclusionList(void)
{
    std::wifstream file(EXCLUDE_FILENAME);
    if (!file.is_open()) {
        //WriteLog(L"除外リストファイル 'exclude.txt' が見つかりません。");
        return;
    }
    defer { file.close(); };

    std::wstring line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            if (line.length() > 0 && line[0] == 0xFEFF)
            {
                line.erase(0, 1);
            }
            std::transform(line.begin(), line.end(), line.begin(), ::towlower);
            g_excludedProcesses.insert(line);
            //WriteLog(L"除外リストに追加: " + line);
        }
    }
}

void ClearExclusionList(void)
{
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    g_excludedProcesses.clear();
    g_processCache.clear();
}

bool IsExcludedProcess(HWND hwnd)
{
    if (hwnd == NULL) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_cacheMutex);

    auto it = g_processCache.find(hwnd);
    if (it != g_processCache.end()) {
        g_cacheLruList.remove(hwnd);
        g_cacheLruList.push_front(hwnd);
        return it->second;
    }

    DWORD processId;
    ::GetWindowThreadProcessId(hwnd, &processId);
    if (processId == 0)
        return false;

    HANDLE hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (hProcess == NULL) {
        return false;
    }
    defer { CloseHandle(hProcess); }

    WCHAR processName[MAX_PATH];
    bool isExcluded = false;
    BOOL bRet = ::QueryFullProcessImageNameW(hProcess, 0, processName, MAX_PATH);
    if (bRet != 0) {
        std::wstring fullPath(processName);
        size_t lastSlash = fullPath.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            std::wstring exeName = fullPath.substr(lastSlash + 1);
            std::transform(exeName.begin(), exeName.end(), exeName.begin(), ::towlower);
            if (g_excludedProcesses.count(exeName)) {
                isExcluded = true;
            }
        }
    }

    if (g_processCache.size() >= MAX_CACHE_SIZE)
    {
        HWND oldest = g_cacheLruList.back();
        g_cacheLruList.pop_back();
        g_processCache.erase(oldest);
    }
    g_cacheLruList.push_front(hwnd);
    g_processCache[hwnd] = isExcluded;

    if (isExcluded) {
        //WriteLog(L"除外プロセスを検出: " + std::wstring(processName));
    }

    return isExcluded;
}

END_NAMESPACE(processExcluder);
