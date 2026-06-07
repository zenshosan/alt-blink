/*
 * Copyright (c) 2025 Masaaki Hamada
 */

#pragma once

#include "framework.h"
#include <string>
#include <atomic>
#include <format>
#include <TCHAR.h>

extern std::atomic<bool> g_stopLogger;

BEGIN_NAMESPACE(asyncLog);

void Start(void);
void Stop(void);
void WriteLog(const std::wstring& message);

END_NAMESPACE(asyncLog);

using tstring2      = std::basic_string<TCHAR>;
using tstring_view2 = std::basic_string_view<TCHAR>;

template <class... Args>
using tformat_string2 = std::basic_format_string<TCHAR, std::type_identity_t<Args>...>;

//template <class... Args>
//[[nodiscard]]
//tstring2 tfmt2(tformat_string2<Args...> _Fmt, Args&&... args)
//{
//    return std::format(_Fmt, std::forward<Args>(args)...);
//}
template <class... Args>
void WriteLog_(tformat_string2<Args...> fmt, Args&&... args)
{
    if (!g_stopLogger.load(std::memory_order_relaxed)) {
        return;
    }
    auto s = std::format(fmt, std::forward<Args>(args)...);
    WriteLog(s.c_str());
}

#define LOG_FUNC_CALL_ERROR2(func, errorCode)                    \
    WriteLog_(_T("[{:16}] * {} failed with errorCode {} at {}"),   \
              MODULE_NAME,                                         \
              func,                                                \
              errorCode,                                           \
              __LINE__)

#define LOG_TRACE2(fmt, ...)                                 \
    WriteLog_(_T("[{:16}]   ") fmt, MODULE_NAME, __VA_ARGS__)
