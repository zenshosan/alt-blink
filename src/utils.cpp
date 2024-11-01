/*
 * Copyright (c) 2024 Masaaki Hamada
 */

#include "framework.h"

std::atomic_bool g_debugPrint;

void win32ErrorMessage(LPCTSTR functionName)
{
    DWORD errorCode = ::GetLastError();
    LPTSTR lpMsgBuf = nullptr;

    DWORD dwFlags
        = FORMAT_MESSAGE_ALLOCATE_BUFFER  //  テキストのメモリ割り当てを要求する
        | FORMAT_MESSAGE_FROM_SYSTEM      //  エラーメッセージはWindowsが用意しているものを使用
        | FORMAT_MESSAGE_IGNORE_INSERTS;  //  次の引数を無視してエラーコードに対するエラーメッセージを作成する
    FormatMessage(
        dwFlags,
        /*lpSource*/     nullptr,
        /*dwMessageId*/  0,
        /*dwLanguageId*/ MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),//   言語を指定
        /*lpBuffer*/     reinterpret_cast<LPTSTR>(&lpMsgBuf),
        /*nSize*/        0,
        /*Argument*/     nullptr);

    defer {
        ::LocalFree(lpMsgBuf);
    };
    auto msg = tfmt(_T("A critial error happened in {}.\n"
                       "errorCode = {}\n"
                       "{}\n\n"
                       "press OK button to exit."),
                    functionName,
                    errorCode,
                    lpMsgBuf);
    ::MessageBox(
        /*hWnd*/      nullptr,
        /*lpText*/    msg.c_str(),
        /*lpCaption*/ nullptr,
        /*uType*/     MB_OK | MB_ICONERROR);

    UINT uExitCode = 1;
    ::ExitProcess(uExitCode);
}

void errorMessage(LPCTSTR message)
{
    auto msg = tfmt(_T("{}\n\n"
                       "press OK button to exit."),
                    message);
    ::MessageBox(
        /*hWnd*/      nullptr,
        /*lpText*/    msg.c_str(),
        /*lpCaption*/ nullptr,
        /*uType*/     MB_OK | MB_ICONERROR);

    UINT uExitCode = 1;
    ::ExitProcess(uExitCode);
}
