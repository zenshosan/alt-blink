/*
 * Copyright (c) 2024 Masaaki Hamada
 */

#include "ime_control.h"

#pragma comment(lib, "Imm32.lib")

#define MODULE_NAME _T("ime_control")

static void printImeStatus(HWND imeWnd)
{
    static const WPARAM IMC_GETOPENSTATUS = 0x005;
    LRESULT lrRet = SendMessage(imeWnd,
                                WM_IME_CONTROL,
                                /*wParam*/ IMC_GETOPENSTATUS,
                                /*lParam*/ 0);
    if (1 > (unsigned)lrRet) {
        LOG_TRACE(_T("IME Status: {}"), int(lrRet));
    } else {
        LOG_FUNC_CALL_ERROR(_T("SendMessage(GETOPENSTATUS)"), lrRet);
    }
}

static void setIme(HWND hWnd, bool on, bool debug)
{
    DWORD         idThread = 0; // foreground thread
    GUITHREADINFO threadInfo{
        .cbSize = sizeof(threadInfo),
    };
    BOOL bRet = GetGUIThreadInfo(idThread, &threadInfo);
    if (! bRet) {
        auto errCode = GetLastError();
        LOG_FUNC_CALL_ERROR(_T("GetGUIThreadInfo()"), errCode);
    };
    HWND hwndFocus = threadInfo.hwndFocus;
    HWND imeWnd    = ImmGetDefaultIMEWnd(hwndFocus);

    if (debug) {
        printImeStatus(imeWnd);
    }

    static const WPARAM IMC_SETOPENSTATUS = 0x006;
    LOG_TRACE(_T("SendMessage hWnd={}, hwndFocus={}, imeWnd={}"),
              (void*)hWnd,
              (void*)hwndFocus,
              (void*)imeWnd);
    LRESULT lrRet = SendMessage(imeWnd,
                                WM_IME_CONTROL,
                                /*wParam*/ IMC_SETOPENSTATUS,
                                /*lParam*/ on ? 1 : 0);
    if (lrRet != 0) {
        LOG_FUNC_CALL_ERROR(_T("SendMessage(SETOPENSTATUS)"), lrRet);
    }
}

BEGIN_NAMESPACE(imeControl);

void SetIme(HWND hWnd, bool on)
{
    setIme(hWnd, on, /*debug*/false);
}

void SetImeDebug(HWND hWnd, bool on)
{
    setIme(hWnd, on, /*debug*/true);
}

END_NAMESPACE(imeControl);
