/*
 * Copyright (c) 2024 Masaaki Hamada
 */

#include "blink_detector.h"
#include "process_excluder.h"
#include "keyboard_hook.h"
#include <queue>
#include <deque>
#include <unordered_map>
#include <filesystem>
#include <psapi.h>
#include <winuser.h>
//#include <imm.h> //IMC_SETOPENSTATUS

#define MODULE_NAME _T("blink_event")

// ★修正点: Windows 11 SDKで未定義のため、手動で定義
#ifndef IMC_SETOPENSTATUS
#define IMC_SETOPENSTATUS 0x0006
#endif

// --- グローバル変数 ---
static HWND s_hMainWnd = NULL;
static HHOOK g_hKeyboardHook = NULL;
static HWINEVENTHOOK g_hWinEventHook = NULL;
//static HINSTANCE g_hInstance = NULL;
//static HWND g_hMainWnd = NULL;

// --- キー状態管理用 ---
std::mutex g_altStateMutex; // Altキー状態を保護するためのミューテックス
bool g_isLAltDown = false;
bool g_isRAltDown = false;
bool g_isCombinationPress = false;
bool g_lAltLongPress = false;
bool g_rAltLongPress = false;
// 実Altのdownを注入済みか（押下しっぱなし防止のため、解放時に必ずupを送る必要がある）
bool g_lAltInjected = false;
bool g_rAltInjected = false;
const UINT LONG_PRESS_THRESHOLD_MS = 200;
const UINT_PTR IDT_LALT_TIMER = 1;
const UINT_PTR IDT_RALT_TIMER = 2;
#define ALT_BLINK_MAGIC_NUMBER 0xABCD

namespace pd = processExcluder;

static void SendKey(WORD vKey, bool press);
static void SetImeStatus(bool enable);


// --- キーボードフックプロシージャ ---
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    HWND fgWnd = GetForegroundWindow();
    if (pd::IsExcludedProcess(fgWnd)) {
        return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
    }

    if (nCode == HC_ACTION)
    {
        KBDLLHOOKSTRUCT* pkbhs = (KBDLLHOOKSTRUCT*)lParam;

        if (pkbhs->dwExtraInfo == ALT_BLINK_MAGIC_NUMBER)
        {
            return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
        }

        std::lock_guard<std::mutex> lock(g_altStateMutex);

        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
        {
            if (pkbhs->vkCode == VK_LMENU)
            {
                if (g_isLAltDown) return 1;
                //WriteLog(L"左Altキー押下を検出。");
                g_isLAltDown = true;
                g_isCombinationPress = false;
                g_lAltLongPress = false;
                g_lAltInjected = false;
                SetTimer(s_hMainWnd, IDT_LALT_TIMER, LONG_PRESS_THRESHOLD_MS, NULL);
                return 1;
            }
            else if (pkbhs->vkCode == VK_RMENU)
            {
                if (g_isRAltDown) return 1;
                //WriteLog(L"右Altキー押下を検出。");
                g_isRAltDown = true;
                g_isCombinationPress = false;
                g_rAltLongPress = false;
                g_rAltInjected = false;
                SetTimer(s_hMainWnd, IDT_RALT_TIMER, LONG_PRESS_THRESHOLD_MS, NULL);
                return 1;
            }
            else if (g_isLAltDown || g_isRAltDown)
            {
                //WriteLog(L"組み合わせ押しを検出。 (他キー VK_CODE: " + std::to_wstring(pkbhs->vkCode) + L")");
                g_isCombinationPress = true;
                if (g_isLAltDown) {
                    KillTimer(s_hMainWnd, IDT_LALT_TIMER);
                    if (!g_lAltInjected) {
                        SendKey(VK_LMENU, true);
                        g_lAltInjected = true;
                    }
                }
                if (g_isRAltDown) {
                    KillTimer(s_hMainWnd, IDT_RALT_TIMER);
                    if (!g_rAltInjected) {
                        SendKey(VK_RMENU, true);
                        g_rAltInjected = true;
                    }
                }
            }
        }
        else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
        {
            if (pkbhs->vkCode == VK_LMENU)
            {
                if (!g_isLAltDown) return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
                //WriteLog(L"左Altキー解放を検出。");
                g_isLAltDown = false;
                KillTimer(s_hMainWnd, IDT_LALT_TIMER);

                if (g_lAltInjected) {
                    //WriteLog(L"実Altを注入済みのため、キー解放イベントを送信。");
                    SendKey(VK_LMENU, false);
                    g_lAltInjected = false;
                } else if (!g_isCombinationPress) {
                    //WriteLog(L"単独タップと判断 -> IMEをオフにします。");
                    SetImeStatus(false);
                }
                return 1;
            }
            else if (pkbhs->vkCode == VK_RMENU)
            {
                if (!g_isRAltDown) return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
                //WriteLog(L"右Altキー解放を検出。");
                g_isRAltDown = false;
                KillTimer(s_hMainWnd, IDT_RALT_TIMER);

                if (g_rAltInjected) {
                    //WriteLog(L"実Altを注入済みのため、キー解放イベントを送信。");
                    SendKey(VK_RMENU, false);
                    g_rAltInjected = false;
                } else if (!g_isCombinationPress) {
                    //WriteLog(L"単独タップと判断 -> IMEをオンにします。");
                    SetImeStatus(true);
                }
                return 1;
            }
        }
    }
    return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
}

// --- タイマープロシージャ (長押し判定用) ---
void HandleTimer_(UINT_PTR idEvent)
{
    std::lock_guard<std::mutex> lock(g_altStateMutex);

    if (idEvent == IDT_LALT_TIMER) {
        KillTimer(s_hMainWnd, idEvent);
        if (g_isLAltDown && !g_isCombinationPress) {
            //WriteLog(L"左Alt長押しを検出。キー押下イベントを送信します。");
            g_lAltLongPress = true;
            g_lAltInjected = true;
            SendKey(VK_LMENU, true);
        }
    } else if (idEvent == IDT_RALT_TIMER) {
        KillTimer(s_hMainWnd, idEvent);
        if (g_isRAltDown && !g_isCombinationPress) {
            //WriteLog(L"右Alt長押しを検出。キー押下イベントを送信します。");
            g_rAltLongPress = true;
            g_rAltInjected = true;
            SendKey(VK_RMENU, true);
        }
    }
}

// --- IMEの状態を設定する関数 (GetGUIThreadInfoを使用) ---
void SetImeStatus(bool enable)
{
    GUITHREADINFO guiThreadInfo = {};
    guiThreadInfo.cbSize = sizeof(GUITHREADINFO);

    if (!GetGUIThreadInfo(0, &guiThreadInfo) || guiThreadInfo.hwndFocus == NULL)
    {
        //WriteLog(L"IME操作エラー: フォーカスを持つウィンドウが取得できません。");
        return;
    }

    HWND hImeWnd = ImmGetDefaultIMEWnd(guiThreadInfo.hwndFocus);
    if (hImeWnd == NULL) {
        //WriteLog(L"IME操作エラー: IMEウィンドウハンドルが取得できません。");
        return;
    }
    SendMessage(hImeWnd, WM_IME_CONTROL, IMC_SETOPENSTATUS, enable ? 1 : 0);
    //WriteLog(std::wstring(L"IME状態を ") + (enable ? L"オン" : L"オフ") + L" に設定しました。");
}

// --- キーイベントを送信する関数 ---
void SendKey(WORD vKey, bool press)
{
    INPUT input = { 0 };
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vKey;
    input.ki.dwExtraInfo = ALT_BLINK_MAGIC_NUMBER;
    if (!press) {
        input.ki.dwFlags = KEYEVENTF_KEYUP;
    }
    SendInput(1, &input, sizeof(INPUT));
}

// --- Altキーの状態をリセットする関数 ---
void ResetAltState()
{
    std::lock_guard<std::mutex> lock(g_altStateMutex);
    if (g_isLAltDown || g_isRAltDown) {
        KillTimer(s_hMainWnd, IDT_LALT_TIMER);
        KillTimer(s_hMainWnd, IDT_RALT_TIMER);
        g_isLAltDown = false;
        g_isRAltDown = false;
        g_isCombinationPress = false;
        g_lAltLongPress = false;
        g_rAltLongPress = false;
        g_lAltInjected = false;
        g_rAltInjected = false;
    }
}

static void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook,
                                  DWORD event,
                                  HWND hwnd,
                                  LONG idObject,
                                  LONG idChild,
                                  DWORD dwEventThread,
                                  DWORD dwmsEventTime)
{
    if (event == EVENT_SYSTEM_FOREGROUND) {
        //WriteLog(L"フォアグラウンドウィンドウが変更されました。Altキーの状態をリセットします。");
        ResetAltState();
    }
}


BEGIN_NAMESPACE(blinkEvent);

int Start(HWND hMainWnd, HINSTANCE hInstance) noexcept
{
    s_hMainWnd = hMainWnd;
    g_hWinEventHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND,
                                      EVENT_SYSTEM_FOREGROUND,
                                      NULL,
                                      WinEventProc,
                                      0,
                                      0,
                                      WINEVENT_OUTOFCONTEXT);

    g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);
    if (g_hKeyboardHook == NULL) {
        //WriteLog(L"初期キーボードフックの設定に失敗しました。");
    }

    return 0;
}

int Stop(void) noexcept
{
    if (g_hKeyboardHook != NULL) {
        UnhookWindowsHookEx(g_hKeyboardHook);
        g_hKeyboardHook = NULL;
    }
    if (g_hWinEventHook != NULL) {
        UnhookWinEvent(g_hWinEventHook);
        g_hWinEventHook = NULL;
    }
    return 0;
}

void HandleTimer(WPARAM wParam)
{
    HandleTimer_(wParam);
}

END_NAMESPACE(blinkEvent);
