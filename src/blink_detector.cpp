/*
 * Copyright (c) 2024 Masaaki Hamada
 */

#include "blink_detector.h"
#include "keyboard_hook.h"

#define MODULE_NAME _T("blink_detector")

extern std::atomic_bool g_debugPrint;

using sc_type = USHORT;  // Scan code.
using vk_type = UCHAR;   // Virtual key.

enum {
    kStateWatching = 0,
    kStateAltDown,
    kStateAltRepeat,
};

struct Context {
    int    state;
    HANDLE foreWindow;
    DWORD  tick;
    DWORD  vkCode;
    uint32_t modifiers;
} static s_context;

static blinkDetector::FuncHandleBlinkEvent s_handler;

static LRESULT handleKeyEvent(const KBDLLHOOKSTRUCT& event, WPARAM wParam)
{
    auto& ctx = s_context;
    const vk_type vk = static_cast<vk_type>(event.vkCode);   // 8bit
    const sc_type sc = static_cast<sc_type>(event.scanCode); // 16bit

    HWND fgWnd   = ::GetForegroundWindow();
    bool keyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);

    uint32_t modifier = 0;
    switch (vk) {
    case VK_LSHIFT:   modifier = blinkDetector::kModifier_Shift_L; break;
    case VK_RSHIFT:   modifier = blinkDetector::kModifier_Shift_R; break;
    case VK_LCONTROL: modifier = blinkDetector::kModifier_Control_L; break;
    case VK_RCONTROL: modifier = blinkDetector::kModifier_Control_L; break;
    default:
        break;
    }
    if (modifier) {
        auto before = ctx.modifiers;
        if (keyDown) {
            ctx.modifiers |= modifier;
        } else {
            ctx.modifiers &= ~modifier;
        }
        if (before != ctx.modifiers) {
            LOG_TRACE(_T("{} modifier {}. modifiers: {:#x} -> {:#x}"),
                      keyDown ? _T("set") : _T("clear"),
                      modifier,
                      before,
                      ctx.modifiers);
        }
    }

    switch (ctx.state) {
    case kStateWatching: {
        if (keyDown && (vk == VK_LMENU || vk == VK_RMENU)) {
            // altが押された
            LOG_TRACE(_T("->kStateAltDown: consume key event"));
            ctx.state      = kStateAltDown;
            ctx.foreWindow = fgWnd;
            ctx.tick       = event.time;
            ctx.vkCode     = vk;
            // altキーイベントをここで消費してForeground Windowに渡らないようにする
            //
            // If the hook procedure processed the message,
            // it may return a nonzero value to prevent the system
            // from passing the message to the rest of the hook chain
            // or the target window procedure.
            return 1;
        }
        break;
    }
    case kStateAltDown: {
        bool doSend = (vk == ctx.vkCode) && (fgWnd == ctx.foreWindow);
        if (doSend) {
            // 前と同じキー(alt)のイベントがやってきた
            if (keyDown) {
                // またaltが押された＝キーリピート
                // ime切り替え動作は行わない
                // altの送り直しは行わない
                LOG_TRACE(_T("->kStateAltRepeat:"));
                ctx.state = kStateAltRepeat;
            } else {
                // altが離された
                bool on = (vk == VK_RMENU);
                uint32_t modifiers = 0;
                s_handler(fgWnd, /*altRight*/on, ctx.modifiers);

                LOG_TRACE(_T("->kStateWatching: consume key event"));
                ctx.state = kStateWatching;
                // altキーイベントをここで消費してアプリに渡らないようにする
                return -1;
            }
        } else {
            // alt以外が押されたので、さきほどaltを消費した分をここで送り直す
            // SendInputの呼び出し中にさらに LowLevelKeyboardProcが呼び出されるという
            // 危険な動作になるので注意が必要。
            // injectedフラグが立つのでLowLevelKeyboardProcでパススルーされる動作になる
            // ので問題ない
            INPUT input[1]{};
            input[0].type           = INPUT_KEYBOARD;
            input[0].ki.dwFlags     = 0;
            input[0].ki.wVk         = (WORD)ctx.vkCode;
            input[0].ki.dwExtraInfo = keyboardHook::kSelfInjectionId;
            LOG_TRACE(_T("inject to recover. key:{}"), ctx.vkCode);
            UINT ret = SendInput(_countof(input), input, sizeof(input[0]));
            if (ret != _countof(input)) {
                LOG_FUNC_CALL_ERROR(_T("SendInput"), ret);
            }
            LOG_TRACE(_T("->kStateWatching: SendInput done."));
            ctx.state = kStateWatching;
        }
        break;
    }
    case kStateAltRepeat: {
        bool repeat = keyDown && (vk == ctx.vkCode) && (fgWnd == ctx.foreWindow);
        if (! repeat) {
            // キーリピート終了
            LOG_TRACE(_T("->kStateWatching: from alt_repeat"));
            ctx.state = kStateWatching;
        }
        break;
    }
    } /* switch */

    return 0;
}

BEGIN_NAMESPACE(blinkDetector);

int Start(HINSTANCE hInstance, FuncHandleBlinkEvent handler) noexcept
{
    s_context = Context{};
    s_handler = handler;
    return keyboardHook::Start(hInstance, handleKeyEvent);
}

int Stop(void)
{
    s_handler = nullptr;
    return keyboardHook::Stop();
}

END_NAMESPACE(blinkDetector);
