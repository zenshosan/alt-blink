/*
 * Copyright (c) 2024 Masaaki Hamada
 */

#include "framework.h"
#include "blink_detector.h"
#include "ime_control.h"
#include "keyboard_hook.h"

#define MODULE_NAME _T("blink_handler")

static void handleBlinkEvent(HWND fgWnd, bool altRight, uint32_t modifiers)
{
    // IME切り替えを行う
    if (altRight) {
        LOG_TRACE(_T("SetIme(on)"));
    } else {
        LOG_TRACE(_T("SetIme(off)"));
    }
    bool imeOn = altRight;
    if (modifiers & blinkDetector::kModifier_Control_L) {
        // 隠しコマンド的にctrl押しながらalt空打ちするとIMEの状態表示を行う
        // デバッグ用
        imeControl::SetImeDebug(fgWnd, imeOn);
    } else {
        imeControl::SetIme(fgWnd, imeOn);
    }

#if 0
    if (false) {
        auto msg = tfmt(_T("exception"));
        throw keyboardHook::Exception(msg);
    }
#endif
}

BEGIN_NAMESPACE(blinkHandler);

int Start(HINSTANCE hInstance)
{
    return blinkDetector::Start(hInstance, handleBlinkEvent);
}

int Stop(void)
{
    return blinkDetector::Stop();
}

END_NAMESPACE(blinkHandler);
