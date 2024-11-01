/*
 * Copyright (c) 2024 Masaaki Hamada
 */

#pragma once

#include "framework.h"

BEGIN_NAMESPACE(imeControl);

void SetIme(HWND hWnd, bool on);
void SetImeDebug(HWND hWnd, bool on);

END_NAMESPACE(imeControl);
