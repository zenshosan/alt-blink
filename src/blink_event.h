/*
 * Copyright (c) 2025 Masaaki Hamada
 */

#pragma once

#include "framework.h"

BEGIN_NAMESPACE(blinkEvent);

int Start(HWND hMainWnd, HINSTANCE hInstance) noexcept;
int Stop(void) noexcept;
void HandleTimer(WPARAM wParam);

END_NAMESPACE(blinkEvent);
