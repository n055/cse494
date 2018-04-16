/*************************************************************************************************
*
* File: MagnifierSample.cpp
*
* Description: Implements a simple control that magnifies the screen, using the 
* Magnification API.
*
* Focus the window and press ESC to toggle between passing through clicks or repositioning window.
*
* Multiple monitors are not supported. // TODO test this?
* 
* Requirements: To compile, link to Magnification.lib. The sample must be run with 
* elevated privileges.
*
* The sample is not designed for multimonitor setups.
*************************************************************************************************/

#include "stdafx.h"

// Ensure that the following definition is in effect before winuser.h is included.
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501    
#endif

// For simplicity, the sample uses a constant magnification factor.
#define MAGFACTOR  2.0f
#define RESTOREDWINDOWSTYLES WS_SIZEBOX | WS_SYSMENU | WS_CLIPCHILDREN | WS_CAPTION // | WS_MAXIMIZEBOX

// Global variables and strings.
HINSTANCE           hInst;
const TCHAR         WindowClassName[]= TEXT("MagnifierWindow");
const TCHAR         WindowTitle[]= TEXT("LOCKED. To unlock magnifier: ALT-TAB to window, press ESC 1 or 2 times");
const TCHAR			WindowTitleMoveable[] = TEXT("UNLOCKED. To lock magnifier: click window, press ESC");
const UINT          timerInterval = 16; // close to the refresh rate @60hz
HWND                hwndMag;
HWND                hwndHost;
HINSTANCE			filterInst;
HWND				hwndFilter;
RECT                magWindowRect;
RECT                hostWindowRect;

// Toolbar GUI controls
#define ID_RED_TEXT		101
#define ID_RED_MULT		102
#define ID_RED_OFFSET	103

#define ID_GREEN_TEXT	111
#define ID_GREEN_MULT	112
#define ID_GREEN_OFFSET	113

#define ID_BLUE_TEXT	121
#define ID_BLUE_MULT	122
#define ID_BLUE_OFFSET	123

#define ID_ZOOM_SLIDER	131

#define INPUT_Y 23
#define INPUT_X 50

// Forward declarations.
ATOM                RegisterHostWindowClass(HINSTANCE hInstance);
BOOL                SetupMagnifier(HINSTANCE hinst);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
void CALLBACK       UpdateMagWindow(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
void SetupToolbarWindow(HINSTANCE hInstance);
BOOL                isMouseTransparent = FALSE;
bool updateMagColors(float rf, float gf, float bf, float ro, float bo, float go);

//
// FUNCTION: WinMain()
//
// PURPOSE: Entry point for the application.
//
int APIENTRY WinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE /*hPrevInstance*/,
                     _In_ LPSTR     /*lpCmdLine*/,
                     _In_ int       nCmdShow)
{
    if (FALSE == MagInitialize())
    {
        return 0;
    }
    if (FALSE == SetupMagnifier(hInstance))
    {
        return 0;
    }

    ShowWindow(hwndHost, nCmdShow);
    UpdateWindow(hwndHost);


	// Toolbar
	SetupToolbarWindow(filterInst);
	
	ShowWindow(hwndFilter, SW_SHOWDEFAULT);
	UpdateWindow(hwndFilter);
	// SetLayeredWindowAttributes(hwndFilter, 0, 255, LWA_ALPHA);

    // Create a timer to update the control.
    UINT_PTR timerId = SetTimer(hwndHost, 0, timerInterval, UpdateMagWindow);

    // Main message loop.
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Shut down.
    KillTimer(NULL, timerId);
    MagUninitialize();
    return (int) msg.wParam;
}


//
// FUNCTION: HostWndProc()
//
// PURPOSE: Window procedure for the window that hosts the magnifier control.
//
LRESULT CALLBACK HostWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) 
    {
	case WM_LBUTTONDOWN:
		SetForegroundWindow(hWnd);
		break;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            if (isMouseTransparent) 
            {
				SetWindowLongPtr(hWnd, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_LAYERED);
				SetWindowText(hWnd, WindowTitleMoveable);
			}
			else {
				SetWindowLongPtr(hWnd, GWL_EXSTYLE, WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_LAYERED);
				SetWindowText(hWnd, WindowTitle);
			}
			isMouseTransparent = !isMouseTransparent;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_SIZE:
        if ( hwndMag != NULL )
        {
            GetClientRect(hWnd, &magWindowRect);
            // Resize the control to fill the window.
            SetWindowPos(hwndMag, NULL, 
                magWindowRect.left, magWindowRect.top, magWindowRect.right, magWindowRect.bottom, 0);
        }
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;  
}

//
// FUNCTION: ToolbarWndProc()
//
// PURPOSE: Window procedure for the window that hosts the toolbar control.
//
LRESULT CALLBACK ToolbarWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int len, idx;
	char *buf;
	float colors[6];
	int ids[] = { ID_RED_MULT, ID_GREEN_MULT, ID_BLUE_MULT, ID_RED_OFFSET, ID_GREEN_OFFSET, ID_BLUE_OFFSET };

	switch (message)
	{

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_COMMAND:
		if (HIWORD(wParam) != EN_CHANGE) {
			break;
		}
		for (idx = 0; idx < 6; idx++) {
			len = GetWindowTextLength(GetDlgItem(hWnd, ids[idx])) + 1;
			buf = new char[len];
			GetDlgItemText(hWnd, ids[idx], buf, len);
			colors[idx] = (float) atof(buf);
			delete buf;
		}

		updateMagColors(
			colors[0], colors[1], colors[2], colors[3], colors[4], colors[5]
		);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

//
//  FUNCTION: RegisterHostWindowClass()
//
//  PURPOSE: Registers the window class for the window that contains the magnification control.
//
ATOM RegisterHostWindowClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcex = {};

    wcex.cbSize = sizeof(WNDCLASSEX); 
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = HostWndProc;
    wcex.hInstance      = hInstance;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(1 + COLOR_BTNFACE);
    wcex.lpszClassName  = WindowClassName;

    return RegisterClassEx(&wcex);
}

//
//  FUNCTION: SetupToolbarWindow()
//
//  PURPOSE: Registers the window class for the window that contains the toolbar.
//
void SetupToolbarWindow(HINSTANCE hInstance)
{
	WNDCLASSEX wcex = {};
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = ToolbarWndProc;
	wcex.hInstance = hInstance;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(1 + COLOR_BTNFACE);
	wcex.lpszClassName = TEXT("toolbar");
	RegisterClassEx(&wcex);

	hwndFilter = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		"toolbar",
		"Toolbar",
		RESTOREDWINDOWSTYLES,
		0, 0, 120, GetSystemMetrics(SM_CYSCREEN),
		NULL, NULL, hInstance, NULL
	);

	// Create Toolbar GUI controls

	CreateWindowEx(0, "STATIC", "R Factor/Offset", WS_CHILD | WS_VISIBLE | SS_LEFT,
		5, 5, INPUT_X * 3, 15, hwndFilter, (HMENU)ID_RED_TEXT, GetModuleHandle(NULL), NULL);
	CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "-1", WS_CHILD | WS_VISIBLE,
		5, 25, INPUT_X, INPUT_Y, hwndFilter, (HMENU)ID_RED_MULT, GetModuleHandle(NULL), NULL);
	CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "1", WS_CHILD | WS_VISIBLE,
		60, 25, INPUT_X, INPUT_Y, hwndFilter, (HMENU)ID_RED_OFFSET, GetModuleHandle(NULL), NULL);

	CreateWindowEx(0, "STATIC", "G Factor/Offset", WS_CHILD | WS_VISIBLE | SS_LEFT,
		5, 65, INPUT_X * 3, 15, hwndFilter, (HMENU)ID_GREEN_TEXT, GetModuleHandle(NULL), NULL);
	CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "-1", WS_CHILD | WS_VISIBLE,
		5, 85, INPUT_X, INPUT_Y, hwndFilter, (HMENU)ID_GREEN_MULT, GetModuleHandle(NULL), NULL);
	CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "1", WS_CHILD | WS_VISIBLE,
		60, 85, INPUT_X, INPUT_Y, hwndFilter, (HMENU)ID_GREEN_OFFSET, GetModuleHandle(NULL), NULL);

	CreateWindowEx(0, "STATIC", "B Factor/Offset", WS_CHILD | WS_VISIBLE | SS_LEFT,
		5, 125, INPUT_X * 3, 15, hwndFilter, (HMENU)ID_BLUE_TEXT, GetModuleHandle(NULL), NULL);
	CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "-1", WS_CHILD | WS_VISIBLE,
		5, 145, INPUT_X, INPUT_Y, hwndFilter, (HMENU)ID_BLUE_MULT, GetModuleHandle(NULL), NULL);
	CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "1", WS_CHILD | WS_VISIBLE,
		60, 145, INPUT_X, INPUT_Y, hwndFilter, (HMENU)ID_BLUE_OFFSET, GetModuleHandle(NULL), NULL);

	CreateWindowEx(0, TRACKBAR_CLASS, "Zoom", WS_CHILD | WS_VISIBLE,	
		5, 185, 100, 30, hwndFilter, (HMENU)ID_ZOOM_SLIDER, GetModuleHandle(NULL), NULL);
	
}

//
// FUNCTION: updateMagColors
//
// PURPOSE: Changes the colors of the magnifier
//
bool updateMagColors(float rf, float gf, float bf, float ro, float bo, float go) {
	MAGCOLOREFFECT magEffectInvert =
	{ {
		{ rf, 0.0f, 0.0f, 0.0f, 0.0f },
		{ 0.0f, gf,  0.0f,  0.0f,  0.0f },
		{ 0.0f,  0.0f, bf,  0.0f,  0.0f },
		{ 0.0f,  0.0f,  0.0f,  1.0f,  0.0f },
		{ ro,  bo,  go,  0.0f,  1.0f }
	} };
	return MagSetColorEffect(hwndMag, &magEffectInvert);
}

//
// FUNCTION: SetupMagnifier
//
// PURPOSE: Creates the windows and initializes magnification.
//
BOOL SetupMagnifier(HINSTANCE hinst)
{
    // Set bounds of host window according to screen size.
    hostWindowRect.top = GetSystemMetrics(SM_CYSCREEN) / 3;
    hostWindowRect.bottom = GetSystemMetrics(SM_CYSCREEN) / 2;  // quarter of screen
    hostWindowRect.left = 100;
    hostWindowRect.right = GetSystemMetrics(SM_CXSCREEN) / 2;

    // Create the host window.
    RegisterHostWindowClass(hinst);
    hwndHost = CreateWindowEx(WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_LAYERED,
        WindowClassName, WindowTitle, 
        RESTOREDWINDOWSTYLES,
        hostWindowRect.left, hostWindowRect.top, hostWindowRect.right, hostWindowRect.bottom, NULL, NULL, hInst, NULL);
    if (!hwndHost)
    {
        return FALSE;
    }

    // Make the window opaque.
    SetLayeredWindowAttributes(hwndHost, 0, 255, LWA_ALPHA);

    // Create a magnifier control that fills the client area.
    GetClientRect(hwndHost, &magWindowRect);
    hwndMag = CreateWindow(WC_MAGNIFIER, TEXT("MagnifierWindow"), 
        WS_CHILD | MS_SHOWMAGNIFIEDCURSOR | WS_VISIBLE,
        magWindowRect.left, magWindowRect.top, magWindowRect.right, magWindowRect.bottom, hwndHost, NULL, hInst, NULL );
    if (!hwndMag)
    {
        return FALSE;
    }

    // Set the magnification factor.
    MAGTRANSFORM matrix;
    memset(&matrix, 0, sizeof(matrix));
    matrix.v[0][0] = MAGFACTOR;
    matrix.v[1][1] = MAGFACTOR;
    matrix.v[2][2] = 1.0f;

    BOOL ret = MagSetWindowTransform(hwndMag, &matrix);

    if (ret)
    {
        MAGCOLOREFFECT magEffectInvert = 
    	{{ // MagEffectInvert
			{ -1.0f, 0.0f, 0.0f, 0.0f, 0.0f },
			{ 0.0f, -1.0f,  0.0f,  0.0f,  0.0f },
			{ 0.0f,  0.0f, -1.0f,  0.0f,  0.0f },
			{ 0.0f,  0.0f,  0.0f,  1.0f,  0.0f },
			{ 1.0f,  1.0f,  1.0f,  0.0f,  1.0f }
		}};
		//{{ // Normal
		//	{ 1.0f, 0.0f,  0.0f,  0.0f,  0.0f },
		//	{ 0.0f, 1.0f,  0.0f,  0.0f,  0.0f },
		//	{ 0.0f,  0.0f, 1.0f,  0.0f,  0.0f },
		//	{ 0.0f,  0.0f,  0.0f,  1.0f,  0.0f },
		//	{ 0.0f,  0.0f,  0.0f,  0.0f,  0.0f }
		//}};
		/*{{ // Custom
			{ 2.0f, 0.0f,  0.0f,  0.0f,  0.0f },
			{ 0.0f, 2.0f,  0.0f,  0.0f,  0.0f },
			{ 0.0f,  0.0f, 1.0f,  0.0f,  0.0f },
			{ 0.0f,  0.0f,  0.0f,  1.0f,  0.0f },
			{ -1.0f,  -1.0f,  0.0f,  0.0f,  0.0f }
		}};*/
		

        ret = MagSetColorEffect(hwndMag,&magEffectInvert);
    }

    return ret;  
}


//
// FUNCTION: UpdateMagWindow()
//
// PURPOSE: Sets the source rectangle and updates the window. Called by a timer.
//
void CALLBACK UpdateMagWindow(HWND /*hwnd*/, UINT /*uMsg*/, UINT_PTR /*idEvent*/, DWORD /*dwTime*/)
{
    POINT mousePoint;
    GetCursorPos(&mousePoint);

    int width = (int)((magWindowRect.right - magWindowRect.left) / MAGFACTOR);
    int height = (int)((magWindowRect.bottom - magWindowRect.top) / MAGFACTOR);

	// Screen metrics
	long screenY = GetSystemMetrics(SM_CYSCREEN);
	long screenX = GetSystemMetrics(SM_CXSCREEN);

    RECT sourceRect;
	RECT windowRect;
	GetWindowRect(hwndHost, &windowRect);
	sourceRect.left = (long) (10 + windowRect.left + (width) * (windowRect.left) / (screenX - MAGFACTOR * width));
	// ^ original: mousePoint.x - width / 2;
    sourceRect.top = (long) (windowRect.top + (height) * (windowRect.top) / (screenY - MAGFACTOR * height));
	// ^ original: mousePoint.y -  height / 2;


    sourceRect.right = sourceRect.left + width;
    sourceRect.bottom = sourceRect.top + height;

    // Set the source rectangle for the magnifier control.
    MagSetWindowSource(hwndMag, sourceRect);

    // Reclaim topmost status, to prevent unmagnified menus from remaining in view. 
    SetWindowPos(hwndHost, HWND_TOPMOST, 0, 0, 0, 0, 
        SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE );

    // Force redraw.
    InvalidateRect(hwndMag, NULL, TRUE);
}
