/*
 * Copyright (c) 2026 Masaaki Hamada
 */

// 除外リスト機能:
// 実行ファイルと同じディレクトリに "exclude.txt" を作成し、1行に1つパスを記述すると、
// そのパスがフルパスの末尾と一致するプロセス上ではこのツールの機能が無効になります。
// (例: "explorer.exe", "system32\notepad.exe", "c:\program files\app\app.exe")
// ※ exclude.txt は UTF-8 で保存してください。行頭の'#'はコメントとして扱われます。

#include "process_excluder.h"

#include <unordered_set>
#include <unordered_map>
#include <deque>
#include <mutex>

#include <fstream>
#include <string>
#include <algorithm> // For std::transform

#include <TCHAR.h>

#define MODULE_NAME _T("process_excluder")
#define EXCLUDE_FILENAME "alt-blink_excludes.txt"
static const size_t MAX_CACHE_SIZE = 100;
static std::unordered_set<std::wstring> g_excludedProcesses;
static std::unordered_map<HWND, bool> g_processCache;
static std::deque<HWND> g_cacheLruList;
static std::mutex g_cacheMutex;


BEGIN_NAMESPACE(processExcluder);

// --- 除外リストとキャッシュ関連 ---
void LoadExclusionList(void)
{
    std::ifstream file(EXCLUDE_FILENAME, std::ios::binary);
    if (!file.is_open()) {
        //WriteLog(L"除外リストファイル 'exclude.txt' が見つかりません。");
        return;
    }
    defer { file.close(); };

    char bom[3] = {0};
    file.read(bom, 3);
    if (static_cast<unsigned char>(bom[0]) != 0xEF ||
        static_cast<unsigned char>(bom[1]) != 0xBB ||
        static_cast<unsigned char>(bom[2]) != 0xBF) {
        file.seekg(0, std::ios::beg);
    }

    std::string line_utf8;
    while (std::getline(file, line_utf8))
    {
        // 先頭の空白をトリム
        size_t first = line_utf8.find_first_not_of(" \t\r\n");
        if (std::string::npos == first) {
            continue;
        }
        line_utf8 = line_utf8.substr(first);

        // コメント行をスキップ
        if (line_utf8.empty() || line_utf8[0] == '#') {
            continue;
        }

        // UTF8 to WCHAR
        int wide_char_size = MultiByteToWideChar(CP_UTF8, 0, line_utf8.c_str(), -1, NULL, 0);
        if (wide_char_size == 0) {
            continue;
        }
        std::wstring line_wide(wide_char_size, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, line_utf8.c_str(), -1, &line_wide[0], wide_char_size);

        line_wide.erase(line_wide.find_last_not_of(L"\r\n\0") + 1);

        if (!line_wide.empty()) {
            std::replace(line_wide.begin(), line_wide.end(), L'/', L'\\');
            std::transform(line_wide.begin(), line_wide.end(), line_wide.begin(), ::towlower);
            g_excludedProcesses.insert(line_wide);
            //WriteLog(L"除外リストに追加: " + line_wide);
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
        //g_cacheLruList.erase(std::remove(g_cacheLruList.begin(), g_cacheLruList.end(), hwnd), g_cacheLruList.end());
        //g_cacheLruList.push_front(hwnd);
        return it->second;
    }

    DWORD processId;
    DWORD threadId = GetWindowThreadProcessId(hwnd, &processId);
    if (threadId == 0) {
        // error
        return false;
    }
    //if (processId == 0) return false;

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (hProcess == NULL) {
        return false;
    }
    defer { ::CloseHandle(hProcess); };

    WCHAR processName[MAX_PATH];
    DWORD size = MAX_PATH;
    bool isExcluded = false;
    BOOL bRet = QueryFullProcessImageNameW(hProcess, 0, processName, &size);
    if (bRet == 0) {
        // error
        return false;
    }

    std::wstring fullPath(processName);
    std::replace(fullPath.begin(), fullPath.end(), L'/', L'\\');
    std::transform(fullPath.begin(), fullPath.end(), fullPath.begin(), ::towlower);

    for (const auto& excludedPath : g_excludedProcesses) {
        auto flen = fullPath.length();
        auto elen = excludedPath.length();
        // フルパスの末尾に一致していたら該当すると判断
        // だたし一致した一つ前がセパレータでなければならない
        if (flen >= elen &&
            fullPath.compare(flen - elen, elen, excludedPath) == 0 &&
            (flen == elen || fullPath[flen - elen - 1] == L'\\')) {
            isExcluded = true;
            break;
        }
    }


    if (g_processCache.size() >= MAX_CACHE_SIZE) {
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
