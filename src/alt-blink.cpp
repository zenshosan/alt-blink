/*
 * Copyright (c) 2026 Masaaki Hamada
 */

#include "framework.h"
#include "alt-blink.h"
//#include "blink_handler.h"
#include "blink_event.h"
#include "async_log.h"

#include <shellapi.h>

#define MODULE_NAME _T("main")

#define MAX_LOADSTRING 100

#define TRAYICON_ID     (0)                 // 複数のアイコンを識別するためのID定数
#define WM_TASKTRAY     (WM_APP + 1)

// グローバル変数:
HINSTANCE hInst;                                // 現在のインターフェイス
WCHAR szTitle[MAX_LOADSTRING];                  // タイトル バーのテキスト
WCHAR szWindowClass[MAX_LOADSTRING];            // メイン ウィンドウ クラス名
HWND s_hWnd;


// このコード モジュールに含まれる関数の宣言を転送します:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

static void showLogWindow(void);
static void hideLogWindow(void);
static void toggleLogWindow(void);
static void reset(void);
static void togglePause(void);
static void pause(void);
static void resume(void);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

#ifdef _DEBUG
    g_debugPrint = true;
#else
    g_debugPrint = false;
#endif

    //2重起動防止
    HANDLE hMutex = ::CreateMutex(
        /*lpMutexAttributes*/NULL,
        /*bInitialOwner*/    FALSE,
        /*lpName*/           _T("alt-blink"));
    if (! hMutex) {
        // failed to create
        LOG_FUNC_CALL_ERROR(_T("CreateMutex"), hMutex);
        return 1;
    }
    defer {::CloseHandle(hMutex);};

    if (::GetLastError() == ERROR_ALREADY_EXISTS) {
        SHOW_ERROR_AND_QUIT(_T("alt-blink is already running."));
        return 1;
    }

    //サーバー起動
    if (g_debugPrint) {
        showLogWindow();
    }

    // グローバル文字列を初期化する
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_ALTBLINK, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // アプリケーション初期化の実行:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_ALTBLINK));

    MSG msg;

    // メイン メッセージ ループ:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}


//
//  関数: MyRegisterClass()
//
//  目的: ウィンドウ クラスを登録します。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ALTBLINK));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_ALTBLINK);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = NULL;

    return RegisterClassExW(&wcex);
}

//
//   関数: InitInstance(HINSTANCE, int)
//
//   目的: インスタンス ハンドルを保存して、メイン ウィンドウを作成します
//
//   コメント:
//
//        この関数で、グローバル変数でインスタンス ハンドルを保存し、
//        メイン プログラム ウィンドウを作成および表示します。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // グローバル変数にインスタンス ハンドルを格納する

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW  & ~WS_VISIBLE,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }
   s_hWnd = hWnd;

   ShowWindow(hWnd, /*nCmdShow*/SW_HIDE);
   UpdateWindow(hWnd);

    NOTIFYICONDATA notifyicondata {};
    notifyicondata.cbSize = sizeof(notifyicondata);
    notifyicondata.hWnd = hWnd;
    notifyicondata.uID = TRAYICON_ID;
    notifyicondata.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    notifyicondata.uCallbackMessage = WM_TASKTRAY;
    notifyicondata.hIcon = ::LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ALTBLINK));
    _tcscpy_s(notifyicondata.szTip, _T("alt-blink"));
    //notifyicondata.DUMMYUNIONNAME.uVersion = NOTIFYICON_VERSION_4;

    Shell_NotifyIcon(NIM_ADD, &notifyicondata);

    int ret = blinkEvent::Start(s_hWnd, hInstance);
    if (0 > ret) {
        return FALSE;
    }

   return TRUE;
}

static BOOL WINAPI HandlerRoutine(DWORD dwCtrlType)
{
    if (dwCtrlType == CTRL_CLOSE_EVENT) {
        hideLogWindow();
        return TRUE;
    }
    return FALSE;
}

static FILE* s_fpStdin;
static FILE* s_fpStdout;
static FILE* s_fpStderr;

static void showLogWindow(void)
{
    ::AllocConsole();

#if 0
    // 最大行数を変更
    static const WORD MAX_CONSOLE_LINES = 32767;
    CONSOLE_SCREEN_BUFFER_INFO coninfo;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
    coninfo.dwSize.Y = MAX_CONSOLE_LINES;
    SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);
#endif
    if (1) {
    freopen_s(&s_fpStdin,  "CONIN$", "r", stdin);
    freopen_s(&s_fpStdout, "CONOUT$", "w", stdout);
    freopen_s(&s_fpStderr, "CONOUT$", "w", stderr);
    } else {
    freopen_s(&s_fpStdin,  "CON", "r", stdin);
    freopen_s(&s_fpStdout, "CON", "w", stdout);
    freopen_s(&s_fpStderr, "CON", "w", stderr);
    }

    // LogWindowと言いつつ実際はConsoleなので閉じると
    // アプリケーションが終了してしまう。
    // その安直な回避策としてcloseボタンを非表示にしておく。
    HWND hwnd = GetConsoleWindow();
    if (hwnd == NULL)  {
        LOG_FUNC_CALL_ERROR(_T("GetConsoleWindow"), (void*)hwnd);
    } else {
        HMENU hMenu = GetSystemMenu(hwnd, /*revert*/FALSE);
        if (hMenu == NULL)  {
            LOG_FUNC_CALL_ERROR(_T("GetSystemMenu"), (void*)hMenu);
        } else {
            UINT  uIDEnableItem = SC_CLOSE;
            UINT  uEnable       = MF_BYCOMMAND | MF_DISABLED | MF_GRAYED;
            ::EnableMenuItem(hMenu, uIDEnableItem, uEnable);
        }
    }

    ::SetConsoleCtrlHandler(HandlerRoutine, /*Add*/TRUE);

    g_debugPrint = true;
    asyncLog::Start();
}

static void hideLogWindow(void)
{
    asyncLog::Stop();
    g_debugPrint = false;

    ::SetConsoleCtrlHandler(HandlerRoutine, /*Add*/FALSE);
    fclose(s_fpStdin);
    fclose(s_fpStdout);
    fclose(s_fpStderr);
    auto bRet = ::FreeConsole();
    if (bRet == 0) {
        //LOG_FUNC_CALL_ERROR(L"FreeConsole", ::GetLastError());
    }
}

static void toggleLogWindow()
{
    if (g_debugPrint) {
        hideLogWindow();
    } else {
        showLogWindow();
    }
}

static void pause(void)
{
    int ret;
    ret = blinkEvent::Stop();
    if (0 > ret) {
        //return FALSE;
    }
}

static void resume(void)
{
    int ret;
    ret = blinkEvent::Start(s_hWnd, hInst);
    if (0 > ret) {
        //return FALSE;
    }
}

static void togglePause(void)
{
    std::lock_guard lock(g_pauseLock);
    if (g_pause) {
        resume();
    } else {
        pause();
    }
    g_pause = !g_pause;
}

static void reset(void)
{
    std::lock_guard lock(g_pauseLock);
    if (!g_pause) {
        pause();
        resume();
    }
}

static void renderTasktrayMenu(WPARAM wParam, LPARAM lParam)
{
    if (wParam != TRAYICON_ID) {
        return;
    }
    auto a = WM_LBUTTONDOWN;
    auto a0 = WM_LBUTTONUP;
    auto a1 = WM_MBUTTONDOWN;
    auto a2 = WM_MBUTTONUP;
    auto a3 = WM_RBUTTONDOWN;
    auto a4 = WM_RBUTTONUP;

    if (lParam == WM_RBUTTONUP) {
        //SetForegroundWindow(g_hWnd);
        // マウスカーソル座標取得
        POINT pos{};
        //pos.x = LOWORD(lParam);
        //pos.y = HIWORD(lParam);
        BOOL bRet = GetCursorPos(&pos);
        //printf("(x,y)=(%d,%d):%d\n", pos.x, pos.y, bRet);
        // スクリーン座標に変換
        //ClientToScreen(g_hWnd, &po);
        HMENU hMenu = ::CreatePopupMenu();
        defer { ::DestroyMenu(hMenu); };

        {
            static const TCHAR text[] = _T("About");
            MENUITEMINFO menuiteminfo{
                .cbSize     = sizeof(menuiteminfo),
                .fMask      = MIIM_STRING | MIIM_ID,
                .wID        = IDM_ABOUT,
                .dwTypeData = const_cast<LPTSTR>(text),
                .cch        = _countof(text),
            };
            ::InsertMenuItem(hMenu, 0, TRUE, &menuiteminfo);
        }
        {
            MENUITEMINFO menuiteminfo{
                .cbSize = sizeof(menuiteminfo),
                .fMask  = MIIM_STRING | MIIM_ID,
                .wID    = IDR_TRAY_LOG,
            };
            if (g_debugPrint) {
                static const TCHAR menuText[] = _T("Hide LogWindow");
                menuiteminfo.dwTypeData = const_cast<LPTSTR>(menuText);
                menuiteminfo.cch        = _countof(menuText);
            } else {
                static const TCHAR menuText[] = _T("Show LogWindow");
                menuiteminfo.dwTypeData = const_cast<LPTSTR>(menuText);
                menuiteminfo.cch        = _countof(menuText);
            }
            ::InsertMenuItem(hMenu, 1, TRUE, &menuiteminfo);
        }
        {
            MENUITEMINFO menuiteminfo{
                .cbSize = sizeof(menuiteminfo),
                .fMask = MIIM_STRING | MIIM_ID,
                .wID = IDR_TRAY_PAUSE,
            };
            if (g_pause) {
                static const TCHAR menuText[] = _T("Resume");
                menuiteminfo.dwTypeData = const_cast<LPTSTR>(menuText);
                menuiteminfo.cch = _countof(menuText);
            }
            else {
                static const TCHAR menuText[] = _T("Pause");
                menuiteminfo.dwTypeData = const_cast<LPTSTR>(menuText);
                menuiteminfo.cch = _countof(menuText);
            }
            ::InsertMenuItem(hMenu, 2, TRUE, &menuiteminfo);
        }
        {
            static const TCHAR terminateText[] = _T("Reset");
            MENUITEMINFO menuiteminfo{
                .cbSize     = sizeof(menuiteminfo),
                .fMask      = MIIM_STRING | MIIM_ID,
                .wID        = IDR_TRAY_RESET,
                .dwTypeData = const_cast<LPTSTR>(terminateText),
                .cch        = _countof(terminateText),
            };
            ::InsertMenuItem(hMenu, 3, TRUE, &menuiteminfo);
        }
        {
            MENUITEMINFO menuiteminfo{
                .cbSize = sizeof(menuiteminfo),
                .fMask  = MIIM_FTYPE,
                .fType  = MFT_SEPARATOR,
            };
            ::InsertMenuItem(hMenu, 4, TRUE, &menuiteminfo);
        }
        {
            static const TCHAR terminateText[] = _T("Exit");
            MENUITEMINFO menuiteminfo{
                .cbSize     = sizeof(menuiteminfo),
                .fMask      = MIIM_STRING | MIIM_ID,
                .wID        = IDR_TRAY_EXIT,
                .dwTypeData = const_cast<LPTSTR>(terminateText),
                .cch        = _countof(terminateText),
            };
            ::InsertMenuItem(hMenu, 5, TRUE, &menuiteminfo);
        }
        // menuの外をクリックしたらmenuを消すようにするためにはFGにする必要があるらしい
        ::SetForegroundWindow(s_hWnd);
        ::TrackPopupMenuEx(hMenu, TPM_LEFTALIGN, pos.x, pos.y, s_hWnd, nullptr);
    }
}

//
//  関数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目的: メイン ウィンドウのメッセージを処理します。
//
//  WM_COMMAND  - アプリケーション メニューの処理
//  WM_PAINT    - メイン ウィンドウを描画する
//  WM_DESTROY  - 中止メッセージを表示して戻る
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // 選択されたメニューの解析:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            case IDR_TRAY_PAUSE:
                togglePause();
                break;
            case IDR_TRAY_RESET:
                reset();
                break;
            case IDR_TRAY_LOG:
                toggleLogWindow();
                break;
            case IDR_TRAY_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: HDC を使用する描画コードをここに追加してください...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        {
            NOTIFYICONDATA notifyicondata {};
            notifyicondata.cbSize = sizeof(notifyicondata);
            notifyicondata.hWnd = s_hWnd;
            notifyicondata.uID = TRAYICON_ID;
            Shell_NotifyIcon(NIM_DELETE, &notifyicondata);
        }
        PostQuitMessage(0);
        break;
    case WM_TASKTRAY:
        renderTasktrayMenu(wParam, lParam);
        break;
    case WM_TIMER:
        blinkEvent::HandleTimer(wParam);
        break;
    case WM_ALTBLINK_SETIME:
        blinkEvent::HandleSetIme(wParam);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// バージョン情報ボックスのメッセージ ハンドラーです。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
