/*
 * Copyright (c) 2026 Masaaki Hamada
 */

#include <queue>
#include <deque>
#include <unordered_map>
//#include <filesystem>
//#include <psapi.h>

#define MODULE_NAME _T("async_log")

#include "async_log.h"

// --- 非同期ロギング用 ---
std::atomic<bool> g_stopLogger(false);

static std::queue<std::wstring> g_logQueue;
static std::mutex g_logMutex;
static std::condition_variable g_logCv;
static HANDLE g_hLoggerThread = NULL;

static DWORD WINAPI LoggerThreadProc(LPVOID lpParam)
{
    //std::wofstream logFile("alt-blink-log.txt", std::ios_base::app);
    //if (!logFile.is_open()) return 1;

    while (!g_stopLogger.load(std::memory_order_relaxed)) {
        std::unique_lock<std::mutex> lock(g_logMutex);
        g_logCv.wait(lock,
                     [] {
                         return
                             !g_logQueue.empty() ||
                             g_stopLogger.load(std::memory_order_relaxed);
                     });

        while (!g_logQueue.empty()) {
            std::wstring message = g_logQueue.front();
            g_logQueue.pop();
            lock.unlock();
            //auto now = std::chrono::system_clock::now();
            //auto in_time_t = std::chrono::system_clock::to_time_t(now);
            //auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
            //std::wstringstream ss;
            //ss << std::put_time(std::localtime(&in_time_t), L"%Y-%m-%d %H:%M:%S");
            //ss << L"." << std::setfill(L'0') << std::setw(3) << ms.count();
            //logFile << L"[" << ss.str() << L"] " << message << std::endl;
            _putts(message.c_str());
            lock.lock();
        }
    }
    //logFile.close();
    return 0;
}


BEGIN_NAMESPACE(asyncLog);

void Start(void)
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_hLoggerThread) {
        return;
    }

    g_hLoggerThread = CreateThread(NULL, 0, LoggerThreadProc, NULL, 0, NULL);
    if (g_hLoggerThread == NULL) {
        //MessageBox(NULL, L"ロギングスレッドの作成に失敗しました。", L"エラー", MB_ICONERROR);
        //return 1;
        return;
    }
    //WriteLog(L"--- アプリケーション開始 ---");
}

void Stop(void)
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    g_stopLogger = true;
    g_logCv.notify_one();
    WaitForSingleObject(g_hLoggerThread, INFINITE);
    CloseHandle(g_hLoggerThread);
}

void WriteLog(const std::wstring& message)
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    g_logQueue.push(message);
    g_logCv.notify_one();
}

END_NAMESPACE(asyncLog);
