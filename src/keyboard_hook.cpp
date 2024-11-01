/*
 * Copyright (c) 2024 Masaaki Hamada
 */

#include "framework.h"

#include "keyboard_hook.h"
#include "blink_detector.h"

#define MODULE_NAME _T("keyboard_hook")

static HINSTANCE s_hInstance;
static keyboardHook::FuncHandleKeyEvent s_keyEventHandler;
static HANDLE s_threadHandle;
static DWORD s_mainThreadID;
static DWORD s_hookThreadID;
static HHOOK s_keyboardHook;
static bool s_stopByError;
static tstring s_errorString;

constexpr SIZE_T s_dwThreadStackSize = 8 * 1024;

static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

static void postQuit(void) noexcept
{
    int nExitCode = 0;
    LOG_TRACE(_T("post WM_QUIT to abort"));
    ::PostThreadMessage(s_hookThreadID, WM_QUIT, 0, 0 );
}

static DWORD WINAPI HookThreadProc(LPVOID aUnused)
{
    (void)aUnused;

    DWORD dwThreadId = 0; // system hook
    HHOOK keybdHook = ::SetWindowsHookEx(WH_KEYBOARD_LL,
                                         LowLevelKeyboardProc,
                                         s_hInstance,
                                         dwThreadId);
    if (keybdHook == nullptr) {
        SHOW_WIN32_ERROR_AND_QUIT(_T("SetWindowsHookEx"));
    }
    s_keyboardHook = keybdHook;
    defer {
         BOOL bRet = ::UnhookWindowsHookEx(keybdHook);
         if (! bRet) {
             LOG_FUNC_CALL_ERROR(_T("UnhookWindowsHookEx"), ::GetLastError());
         }
         s_keyboardHook = nullptr;
    };

    for (;;) {
        MSG msg;
        UINT wMsgFilterMin = 0;
        UINT wMsgFilterMax = 0; // get all available messages
        auto bRet = ::GetMessage(&msg, /*hWnd*/nullptr, wMsgFilterMin, wMsgFilterMax);
        if (bRet == -1) {
            // -1 is an error
            SHOW_WIN32_ERROR_AND_QUIT(_T("GetMessage"));
            break;
        }

        if (msg.message == WM_QUIT) {
            LOG_TRACE(_T("received WM_QUIT"));
            break;
        }
    }

    if (s_stopByError) {
        auto msg = tfmt(_T("a critical error happened during KeyboardHook execution.\n {}"), s_errorString);
        SHOW_ERROR_AND_QUIT(msg.c_str());
    }

    LOG_TRACE(_T("HookThreadProc done"));
    return 0;
}

static void printKeyEvent(int nCode, WPARAM wParam, const KBDLLHOOKSTRUCT& event)
{
    if (! isDebugPrintEnabled()) {
        return;
    }

    using sc_type = USHORT;  // Scan code.
    using vk_type = UCHAR;   // Virtual key.

    vk_type vk = static_cast<vk_type>(event.vkCode);   // 8bit
    sc_type sc = static_cast<sc_type>(event.scanCode); // 16bit

    LPCTSTR pWParam;
    switch (wParam) {
        CASE_TO_STRING(pWParam, WM_KEYDOWN);
        CASE_TO_STRING(pWParam, WM_KEYUP);
        CASE_TO_STRING(pWParam, WM_SYSKEYDOWN);
        CASE_TO_STRING(pWParam, WM_SYSKEYUP);
        DEFAULT_TO_STRING(pWParam, _T("unknown"));
    }

    LPCTSTR pVk = nullptr;
    constexpr vk_type vk_0 = 0x30;
    constexpr vk_type vk_9 = 0x39;
    constexpr vk_type vk_a = 0x41;
    constexpr vk_type vk_z = 0x5A;
    switch (vk) {
        CASE_TO_STRING(pVk, VK_SHIFT);
        CASE_TO_STRING(pVk, VK_CONTROL);
        CASE_TO_STRING(pVk, VK_MENU);
        CASE_TO_STRING(pVk, VK_SPACE);
        CASE_TO_STRING(pVk, VK_LWIN);
        CASE_TO_STRING(pVk, VK_RWIN);
        CASE_TO_STRING(pVk, VK_LSHIFT);
        CASE_TO_STRING(pVk, VK_RSHIFT);
        CASE_TO_STRING(pVk, VK_LCONTROL);
        CASE_TO_STRING(pVk, VK_RCONTROL);
        CASE_TO_STRING(pVk, VK_LMENU);
        CASE_TO_STRING(pVk, VK_RMENU);
    default:
        if (vk_0 <= vk && vk <= vk_9) {
            constexpr const TCHAR* tb[] = {
                _T("0"), _T("1"), _T("2"), _T("3"), _T("4"),
                _T("5"), _T("6"), _T("7"), _T("8"), _T("9")
            };
            pVk = tb[vk - vk_0];
        } else if (vk_a <= vk && vk <= vk_z) {
            constexpr const TCHAR* tb[] = {
                _T("a"), _T("b"), _T("c"), _T("d"), _T("e"),
                _T("f"), _T("g"), _T("h"), _T("i"), _T("j"),
                _T("k"), _T("l"), _T("m"), _T("n"), _T("o"),
                _T("p"), _T("q"), _T("r"), _T("s"), _T("t"),
                _T("u"), _T("v"), _T("w"), _T("x"), _T("y"),
                _T("z")
            };
            pVk = tb[vk - vk_a];
        } else {
            pVk = _T("");
        }
    }

    DWORD flags = event.flags;
    auto check_and_clear = +[](DWORD& flags, DWORD f) {
        bool s = (flags & f);
        flags &= ~f;
        return int(s);
    };
    auto extended          = check_and_clear(flags, LLKHF_EXTENDED);
    auto injected          = check_and_clear(flags, LLKHF_INJECTED);
    auto altdown           = check_and_clear(flags, LLKHF_ALTDOWN);
    auto up                = check_and_clear(flags, LLKHF_UP);
    auto lower_il_injected = check_and_clear(flags, LLKHF_LOWER_IL_INJECTED);

    LOG_TRACE(_T("({:08}) ex:{} inj:{} altd:{} up:{} ll_inj:{}  extra={} nCode={} sc={:#02x} vk={:#02x}({})"),
           event.time,
           extended, injected, altdown, up, lower_il_injected,
           event.dwExtraInfo,
           nCode,
           sc, vk, pVk);
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    /*
     * typedef struct tagKBDLLHOOKSTRUCT {
     *     DWORD     vkCode;
     *     DWORD     scanCode;
     *     DWORD     flags;
     *     DWORD     time;
     *     ULONG_PTR dwExtraInfo;
     * } KBDLLHOOKSTRUCT, *LPKBDLLHOOKSTRUCT, *PKBDLLHOOKSTRUCT;
     */
    KBDLLHOOKSTRUCT &event = *reinterpret_cast<PKBDLLHOOKSTRUCT>(lParam);
    printKeyEvent(nCode, wParam, event);

    static_assert(0 == HC_ACTION);
    if (HC_ACTION > nCode) {
        LOG_TRACE(_T("0 > nCode"));
        return CallNextHookEx(s_keyboardHook, nCode, wParam, lParam);
    }

    bool injected = static_cast<bool>(event.flags & LLKHF_INJECTED);
    if (injected && event.dwExtraInfo == keyboardHook::kSelfInjectionId) {
        LOG_TRACE(_T("injected!! flags:{:#x}"), event.flags);
        return CallNextHookEx(s_keyboardHook, nCode, wParam, lParam);
    }

    try {
        LRESULT processed = s_keyEventHandler(event, wParam);
        if (processed) {
            LOG_TRACE(_T("key event was processed"));
            return processed;
        }
    } catch (keyboardHook::Exception& e) {
        s_stopByError = true;
        s_errorString = e.msg();
        postQuit();
    } catch (std::runtime_error& e) {
        (void)e;
        s_stopByError = true;
        s_errorString = _T("");
        //s_errorString = e.what();
        postQuit();
    } catch (std::logic_error& e) {
        (void)e;
        s_stopByError = true;
        s_errorString = _T("");
        //s_errorString = e.what();
        postQuit();
    } catch (...) {
        s_stopByError = true;
        s_errorString = _T("unknown exception");
        postQuit();
    }

    return CallNextHookEx(s_keyboardHook, nCode, wParam, lParam);
}

static int startImpl(HINSTANCE hInstance, keyboardHook::FuncHandleKeyEvent handler) noexcept
{
    s_stopByError = false;
    s_hInstance = hInstance;
    s_keyEventHandler = handler;
    s_mainThreadID = ::GetCurrentThreadId();

    DWORD  dwCreationFlags = CREATE_SUSPENDED;
    HANDLE hRet = CreateThread(/*lpThreadAttributes*/ nullptr,
                               s_dwThreadStackSize,
                               HookThreadProc,
                               /*lpParameter*/ nullptr,
                               dwCreationFlags,
                               &s_hookThreadID);
    if (hRet == NULL) {
        SHOW_WIN32_ERROR_AND_QUIT(_T("CreateThread"));
        return -1;
    }
    s_threadHandle = hRet;

    BOOL bRet = SetThreadPriority(s_threadHandle, THREAD_PRIORITY_TIME_CRITICAL);
    if (! bRet) {
        SHOW_WIN32_ERROR_AND_QUIT(_T("SetThreadPriority"));
        ::CloseHandle(s_threadHandle);
        return -1;
    }

    DWORD dwRet = ResumeThread(s_threadHandle);
    if (dwRet == -1) {
        SHOW_WIN32_ERROR_AND_QUIT(_T("ResumeThread"));
        ::CloseHandle(s_threadHandle);
        return -1;
    }

    return 0;
}

static int stopImpl(void) noexcept
{
    int nExitCode = 0;
    LOG_TRACE(_T("post WM_QUIT"));
    ::PostThreadMessage(s_hookThreadID, WM_QUIT, 0, 0 );

    LOG_TRACE(_T("WaitForSingleObject"));
    ::WaitForSingleObject(s_threadHandle, INFINITE);

    LOG_TRACE(_T("CloseHandle"));
    ::CloseHandle(s_threadHandle);
    s_threadHandle = nullptr;

    return 0;
}

BEGIN_NAMESPACE(keyboardHook);

int Start(HINSTANCE hInstance, FuncHandleKeyEvent handler) noexcept
{
    return startImpl(hInstance, handler);
}

int Stop(void) noexcept
{
    return stopImpl();
}

END_NAMESPACE(keyboardHook);
