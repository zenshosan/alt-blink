/*
 * Copyright (c) 2024 Masaaki Hamada
 */

#pragma once

#include "framework.h"

BEGIN_NAMESPACE(blinkDetector);

enum : uint32_t {
    kModifier_Shift_L   = 1,
    kModifier_Shift_R   = 2,
    kModifier_Control_L = 4,
    kModifier_Control_R = 8,
};
using FuncHandleBlinkEvent = void (*)(HWND fgWnd, bool altRight, uint32_t modifiers);

int Start(HINSTANCE hInstance, FuncHandleBlinkEvent handler) noexcept;
int Stop(void);

END_NAMESPACE(blinkDetector);
