/*
 * Copyright (c) 2024 Masaaki Hamada
 */

#pragma once

#include "framework.h"
#include <stdexcept>
#include <string>

BEGIN_NAMESPACE(keyboardHook);

class Exception : public std::exception {
    const tstring m_msg;
public:
    Exception(const tstring& msg) : m_msg(msg) {}
    const tstring& msg() const noexcept { return m_msg; }
};

static const uint32_t kSelfInjectionId = 29; // meat

using FuncHandleKeyEvent = LRESULT (*)(const KBDLLHOOKSTRUCT& event, WPARAM wParam);

int Start(HINSTANCE hInstance, FuncHandleKeyEvent handler) noexcept;

int Stop(void) noexcept;

END_NAMESPACE(keyboardHook);
