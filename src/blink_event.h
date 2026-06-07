/*
 * Copyright (c) 2025 Masaaki Hamada
 */

#pragma once

#include "framework.h"

// フック内での同期SendMessage(IME操作)を避けるため、
// IME操作はこのメッセージでメインウィンドウへ委譲し、メッセージループ側で実行する。
// wParam: 0 = IME off, 1 = IME on
#define WM_ALTBLINK_SETIME (WM_APP + 2)

BEGIN_NAMESPACE(blinkEvent);

int Start(HWND hMainWnd, HINSTANCE hInstance) noexcept;
int Stop(void) noexcept;
void HandleTimer(WPARAM wParam);
void HandleSetIme(WPARAM wParam);

END_NAMESPACE(blinkEvent);
