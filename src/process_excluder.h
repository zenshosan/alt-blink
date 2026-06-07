/*
 * Copyright (c) 2026 Masaaki Hamada
 */

#pragma once

#include "framework.h"

BEGIN_NAMESPACE(processExcluder);

int LoadExclusionList(HINSTANCE hInstance);
void ClearExclusionList(void);
bool IsExcludedProcess(HWND hwnd);

END_NAMESPACE(processExcluder);
