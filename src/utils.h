/*
 * Copyright (c) 2026 Masaaki Hamada
 */

#pragma once

#include <utility>
#include <string_view>
#include <format>
#include <iostream>
#include <mutex>

#include <tchar.h>

#define BEGIN_NAMESPACE(nm) namespace nm {
#define END_NAMESPACE(nm)   }

#define CASE_TO_STRING(p, x)    case x:  p = _T(#x); break;
#define DEFAULT_TO_STRING(p, x) default: p = x;      break;

extern std::atomic_bool g_debugPrint;
extern std::mutex g_pauseLock;
extern bool g_pause;

#define isDebugPrintEnabled() g_debugPrint.load(std::memory_order_relaxed)

/*
 * go like defer for c++
 *
 * usege:
 * auto a = openSomeghing();
 * defer { closeSomething(a); };  // semicolon needed
 */
template <class Functor>
class Defer {
private:
    Functor m_func;
public:
    explicit Defer(Functor&& func) noexcept
        : m_func{std::forward<Functor>(func)} { }
    ~Defer(void) noexcept { m_func(); }

    // delete since this is useless as var name is unknwon for user
    //Defer& operator=(Defer&& rhs) noexcept {
    //    m_func = std::move(rhs.m_task);
    //    return *this;
    //}
    Defer& operator=(Defer&& rhs) = delete;

    // no copy
    Defer(const Defer&) = delete;
    Defer& operator=(const Defer&) = delete;
};

class DeferMakeHelper final {
public:
    template <typename Functor>
    Defer<Functor> operator+(Functor&& func) {
        return Defer{std::forward<Functor>(func)};
    }
};

#define DEFER_CONCAT_NAME(prefix, line) prefix ## line
#define DEFER_MAKE_VARNAME(prefix, line)  DEFER_CONCAT_NAME(prefix, line)
#define defer auto DEFER_MAKE_VARNAME(defer_, __LINE__) = DeferMakeHelper{} + [&](void) noexcept -> void

using tstring      = std::basic_string<TCHAR>;
using tstring_view = std::basic_string_view<TCHAR>;

template <class... Args>
using tformat_string = std::basic_format_string<TCHAR, std::type_identity_t<Args>...>;

template <class... Args>
[[nodiscard]]
tstring tfmt(tformat_string<Args...> _Fmt, Args&&... args)
{
    return std::format(_Fmt, std::forward<Args>(args)...);
}

template <class... Args>
void tprint(tformat_string<Args...> fmt, Args&&... args)
{
    if (!g_debugPrint) {
        return;
    }
    auto s = std::format(fmt, std::forward<Args>(args)...);
    _putts(s.c_str());
}

void win32ErrorMessage(LPCTSTR functionName);
void errorMessage(LPCTSTR message);

#define SHOW_ERROR_AND_QUIT(msg) errorMessage(msg)
#define SHOW_WIN32_ERROR_AND_QUIT(func) win32ErrorMessage(func)

#define LOG_FUNC_CALL_ERROR(func, errorCode)                    \
    tprint(_T("[{:16}] * {} failed with errorCode {} at {}"),   \
           MODULE_NAME,                                         \
           func,                                                \
           errorCode,                                           \
           __LINE__)

#define LOG_TRACE(fmt, ...)                                 \
    tprint(_T("[{:16}]   ") fmt, MODULE_NAME, __VA_ARGS__);
