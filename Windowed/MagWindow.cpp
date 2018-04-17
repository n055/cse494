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
#include "resource1.h"

// Ensure that the following definition is in effect before winuser.h is included.
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501    
#endif

#define RESTOREDWINDOWSTYLES WS_SIZEBOX | WS_SYSMENU | WS_CLIPCHILDREN | WS_CAPTION

// Global variables and strings.
float				magFactor = 2.0f;
HINSTANCE           hInst;
const TCHAR         WindowClassName[]= TEXT("MagnifierWindow");
const TCHAR         WindowTitle[]= TEXT("LOCKED. To unlock magnifier: ALT-TAB to window, press ESC");
const TCHAR			WindowTitleMoveable[] = TEXT("UNLOCKED. To lock magnifier: click window, press ESC");
const UINT          timerInterval = 16; // close to the refresh rate @60hz
HWND                hwndMag;
HWND                hwndHost;
HINSTANCE			filterInst;
HWND				hwndFilter;
RECT                magWindowRect;
RECT                hostWindowRect;
HWND hDlgCurrent = NULL;

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

#define ID_LOCK 141

#define ID_LOAD 151

#define ID_MENU 161

#define INPUT_Y 23
#define INPUT_X 50

// User-defined presets
struct preset {
	std::string name = "";
	float rf = 1, gf = 1, bf = 1, ro = 0, go = 0, bo = 0, zoom = 2;
};
std::vector<preset> presets;


// Forward declarations.
ATOM                RegisterHostWindowClass(HINSTANCE hInstance);
BOOL                SetupMagnifier(HINSTANCE hinst);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
void CALLBACK       UpdateMagWindow(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
void SetupToolbarWindow(HINSTANCE hInstance);
BOOL                isMouseTransparent = FALSE;
bool updateMagColors(float rf, float gf, float bf, float ro, float bo, float go);
void loadSettings(char filename[MAX_PATH]);

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
		if (NULL == hDlgCurrent || !IsDialogMessage(hDlgCurrent, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
    }

    // Shut down.
    KillTimer(NULL, timerId);
    MagUninitialize();
    return (int) msg.wParam;
}

MAGTRANSFORM updateMagFactor(float mf) {
	magFactor = mf;
	// Set the magnification factor.
	MAGTRANSFORM matrix;
	memset(&matrix, 0, sizeof(matrix));
	matrix.v[0][0] = magFactor;
	matrix.v[1][1] = magFactor;
	matrix.v[2][2] = 1.0f;
	return matrix;
}

void toggleLock() {
	if (isMouseTransparent)
	{
		SetWindowLongPtr(hwndHost, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_LAYERED);
		SetWindowText(hwndHost, WindowTitleMoveable);
		CheckDlgButton(hwndFilter, ID_LOCK, BST_UNCHECKED);
	}
	else {
		SetWindowLongPtr(hwndHost, GWL_EXSTYLE, WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_LAYERED);
		SetWindowText(hwndHost, WindowTitle);
		CheckDlgButton(hwndFilter, ID_LOCK, BST_CHECKED);
	}
	isMouseTransparent = !isMouseTransparent;
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
			toggleLock();
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

void changeMag(HWND hWnd, WPARAM wParam) {
	if (LOWORD(wParam) == TB_ENDTRACK) {
		DWORD dwPos;    // current position of slider 
		dwPos = (DWORD)SendMessage(GetDlgItem(hWnd, ID_ZOOM_SLIDER), TBM_GETPOS, 0, 0);
		MAGTRANSFORM mf = updateMagFactor((float)dwPos / 2);
		MagSetWindowTransform(hwndMag, &mf);
	}
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
	case WM_ACTIVATE:
		if (0 == wParam)             // becoming inactive
			hDlgCurrent = NULL;
		else                         // becoming active
			hDlgCurrent = hWnd;

		return FALSE;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_HSCROLL:
		changeMag(hWnd, wParam);
		break;

	case WM_COMMAND:
		if (LOWORD(wParam) == ID_LOCK) {
			toggleLock();
		}
		else if (LOWORD(wParam) == ID_LOAD) {
			OPENFILENAME ofn;
			char szFileName[MAX_PATH] = "";

			ZeroMemory(&ofn, sizeof(ofn));

			ofn.lStructSize = sizeof(ofn); // SEE NOTE BELOW
			ofn.hwndOwner = hWnd;
			ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
			ofn.lpstrFile = szFileName;
			ofn.nMaxFile = MAX_PATH;
			ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
			ofn.lpstrDefExt = "txt";

			if (GetOpenFileName(&ofn))
			{
				loadSettings(szFileName);
				// Do something usefull with the filename stored in szFileName 
			}
		}
		else if (HIWORD(wParam) == CBN_SELCHANGE) {
			preset p = presets[SendMessage(GetDlgItem(hwndFilter, ID_MENU), CB_GETCURSEL, 0, 0)];
			// Retrieve data from given preset p
			SendMessage(GetDlgItem(hwndFilter, ID_RED_MULT), WM_SETTEXT, 0, (LPARAM)std::to_string(p.rf).substr(0, 5).c_str());
			SendMessage(GetDlgItem(hwndFilter, ID_RED_OFFSET), WM_SETTEXT, 0, (LPARAM)std::to_string(p.ro).substr(0, 5).c_str());
			SendMessage(GetDlgItem(hwndFilter, ID_GREEN_MULT), WM_SETTEXT, 0, (LPARAM)std::to_string(p.gf).substr(0, 5).c_str());
			SendMessage(GetDlgItem(hwndFilter, ID_GREEN_OFFSET), WM_SETTEXT, 0, (LPARAM)std::to_string(p.go).substr(0, 5).c_str());
			SendMessage(GetDlgItem(hwndFilter, ID_BLUE_MULT), WM_SETTEXT, 0, (LPARAM)std::to_string(p.bf).substr(0, 5).c_str());
			SendMessage(GetDlgItem(hwndFilter, ID_BLUE_OFFSET), WM_SETTEXT, 0, (LPARAM)std::to_string(p.bo).substr(0, 5).c_str());
			SendMessage(
				GetDlgItem(hwndFilter, ID_ZOOM_SLIDER),
				TBM_SETPOS,
				(WPARAM)TRUE,
				(LPARAM)(p.zoom * 2.0f)
			);
			changeMag(hWnd, TB_ENDTRACK);
			
		} else if (HIWORD(wParam) == EN_CHANGE) {
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
		}

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
	wcex.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
	wcex.hIconSm = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, ::GetSystemMetrics(SM_CXICON),
		::GetSystemMetrics(SM_CYICON), 0);

    return RegisterClassEx(&wcex);
}

void loadSettings(char filename[MAX_PATH]) {
	std::ifstream settings;
	settings.open(filename); //TODO fix
	if (settings) {

		std::string line;
		// iterate over lines of settings
		while (std::getline(settings, line, '\n')) {
			preset p;
			std::istringstream iss(line);
			std::string token;
			std::getline(iss, token, ',');
			p.name = token;
			std::getline(iss, token, ',');
			p.rf = stof(token);
			std::getline(iss, token, ',');
			p.gf = stof(token);
			std::getline(iss, token, ',');
			p.bf = stof(token);
			std::getline(iss, token, ',');
			p.ro = stof(token);
			std::getline(iss, token, ',');
			p.go = stof(token);
			std::getline(iss, token, ',');
			p.bo = stof(token);
			std::getline(iss, token, ',');
			p.zoom = stof(token);
			presets.push_back(p);
		}
	}
	settings.close();

	// Populate dropdown menu
	for each (preset p in presets)
	{
		LPCSTR name = p.name.c_str();
		SendMessage(GetDlgItem(hwndFilter, ID_MENU), CB_ADDSTRING, (WPARAM)0, (LPARAM)name);
	}
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
	wcex.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
	wcex.hIconSm = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, ::GetSystemMetrics(SM_CXICON),
		::GetSystemMetrics(SM_CYICON), 0);
	RegisterClassEx(&wcex);

	hwndFilter = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		"toolbar",
		"Toolbar",
		RESTOREDWINDOWSTYLES,
		50, GetSystemMetrics(SM_CYSCREEN) - 450, 120, 360, //GetSystemMetrics(SM_CYSCREEN),
		NULL, NULL, hInstance, NULL
	);

	// Create Toolbar GUI controls

	CreateWindowEx(0, "STATIC", "R Factor/Offset", WS_CHILD | WS_VISIBLE | SS_LEFT,
		5, 5, INPUT_X * 3, 15, hwndFilter, (HMENU)ID_RED_TEXT, GetModuleHandle(NULL), NULL);
	CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "1", WS_TABSTOP | WS_CHILD | WS_VISIBLE,
		5, 25, INPUT_X, INPUT_Y, hwndFilter, (HMENU)ID_RED_MULT, GetModuleHandle(NULL), NULL);
	CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "0", WS_TABSTOP | WS_CHILD | WS_VISIBLE,
		60, 25, INPUT_X, INPUT_Y, hwndFilter, (HMENU)ID_RED_OFFSET, GetModuleHandle(NULL), NULL);

	CreateWindowEx(0, "STATIC", "G Factor/Offset", WS_CHILD | WS_VISIBLE | SS_LEFT,
		5, 65, INPUT_X * 3, 15, hwndFilter, (HMENU)ID_GREEN_TEXT, GetModuleHandle(NULL), NULL);
	CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "1", WS_TABSTOP | WS_CHILD | WS_VISIBLE,
		5, 85, INPUT_X, INPUT_Y, hwndFilter, (HMENU)ID_GREEN_MULT, GetModuleHandle(NULL), NULL);
	CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "0", WS_TABSTOP | WS_CHILD | WS_VISIBLE,
		60, 85, INPUT_X, INPUT_Y, hwndFilter, (HMENU)ID_GREEN_OFFSET, GetModuleHandle(NULL), NULL);

	CreateWindowEx(0, "STATIC", "B Factor/Offset", WS_CHILD | WS_VISIBLE | SS_LEFT,
		5, 125, INPUT_X * 3, 15, hwndFilter, (HMENU)ID_BLUE_TEXT, GetModuleHandle(NULL), NULL);
	CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "1", WS_TABSTOP | WS_CHILD | WS_VISIBLE,
		5, 145, INPUT_X, INPUT_Y, hwndFilter, (HMENU)ID_BLUE_MULT, GetModuleHandle(NULL), NULL);
	CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "0", WS_TABSTOP | WS_CHILD | WS_VISIBLE,
		60, 145, INPUT_X, INPUT_Y, hwndFilter, (HMENU)ID_BLUE_OFFSET, GetModuleHandle(NULL), NULL);

	HWND hwndZoom = CreateWindowEx(0, TRACKBAR_CLASS, "Zoom", WS_TABSTOP | WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
		5, 185, 105, 30, hwndFilter, (HMENU)ID_ZOOM_SLIDER, GetModuleHandle(NULL), NULL);
	SendMessage(
		hwndZoom,
		TBM_SETRANGE,
		(WPARAM)TRUE,                   // redraw flag 
		(LPARAM)MAKELONG(2, 8)  // min. & max. positions
	);
	SendMessage(
		hwndZoom,
		TBM_SETPOS,
		(WPARAM)TRUE,
		(LPARAM)4 // 200% zoom
	);

	CreateWindowEx(0, "BUTTON", "Lock Window", WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
		5, 225, 110, INPUT_Y, hwndFilter, (HMENU)ID_LOCK, GetModuleHandle(NULL), NULL);

	CreateWindowEx(0, "BUTTON", "Load Presets", WS_TABSTOP | WS_CHILD | WS_VISIBLE,
		5, 255, 105, INPUT_Y, hwndFilter, (HMENU)ID_LOAD, GetModuleHandle(NULL), NULL);

	CreateWindowEx(0, WC_COMBOBOX, "(Presets)", WS_TABSTOP | CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_CHILD | WS_VISIBLE,
		5, 285, 105, INPUT_Y, hwndFilter, (HMENU)ID_MENU, GetModuleHandle(NULL), NULL);
	
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
    hostWindowRect.bottom = GetSystemMetrics(SM_CYSCREEN) / 3;  // ninth of screen
    hostWindowRect.left = 200;
    hostWindowRect.right = GetSystemMetrics(SM_CXSCREEN) / 3;

    // Create the host window.
    RegisterHostWindowClass(hinst);
    hwndHost = CreateWindowEx(WS_EX_TOPMOST | WS_EX_LAYERED,
        WindowClassName, WindowTitleMoveable, 
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

	MAGTRANSFORM matrix = updateMagFactor(magFactor);

    BOOL ret = MagSetWindowTransform(hwndMag, &matrix);

    if (ret)
    {
		return updateMagColors(1, 1, 1, 0, 0, 0);
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

	// Screen metrics
	long screenY = GetSystemMetrics(SM_CYSCREEN);
	long screenX = GetSystemMetrics(SM_CXSCREEN);

    RECT sourceRect;
	RECT windowRect;
	RECT magWindowRectRelToScreen;

	GetWindowRect(hwndHost, &windowRect);
	GetWindowRect(hwndMag, &magWindowRectRelToScreen);

	int srcWidth = (int)((magWindowRect.right - magWindowRect.left) / magFactor);
	int srcHeight = (int)((magWindowRect.bottom - magWindowRect.top) / magFactor);
	int width = (int)(magWindowRectRelToScreen.right - magWindowRectRelToScreen.left);
	int height = (int)(magWindowRectRelToScreen.bottom - magWindowRectRelToScreen.top);


	sourceRect.left = (long) (magWindowRectRelToScreen.left 
		+ (width - srcWidth) * magWindowRectRelToScreen.left
		/ (screenX - width));
    sourceRect.top = (long) (windowRect.top
		+ (height - srcHeight) * windowRect.top
		/ (screenY - height));


	// ^ original: mousePoint.x - srcWidth / 2;
	// ^ original: mousePoint.y -  srcHeight / 2;


    sourceRect.right = sourceRect.left + srcWidth;
    sourceRect.bottom = sourceRect.top + srcHeight;

    // Set the source rectangle for the magnifier control.
    MagSetWindowSource(hwndMag, sourceRect);

    // Reclaim topmost status, to prevent unmagnified menus from remaining in view. 
    SetWindowPos(hwndHost, HWND_TOPMOST, 0, 0, 0, 0, 
        SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE );

    // Force redraw.
    InvalidateRect(hwndMag, NULL, TRUE);
}
