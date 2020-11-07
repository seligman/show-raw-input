#include "framework.h"
#include "RawInput.h"
//#include "hidsdi.h"
#include <stdio.h>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
HWND hWndMain;
HWND hWndEdit;
HWND hWndStartStop;
BOOL bRunning;
HBRUSH hEditBack;

ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
void				Resize(HWND hwnd);
void				StartStop();
void				RegisterRawInput(BOOL bNewState);
void				UpdateStartStopButton();
void				LogMessage(const TCHAR* msg);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_RAWINPUT, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	bRunning = FALSE;

	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_RAWINPUT));

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_RAWINPUT));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_RAWINPUT);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

void RegisterRawInput(BOOL bNewState)
{
	RAWINPUTDEVICE Rid[2];

	for (int i = 0; i < 2; i++)
	{
		Rid[i].usUsagePage = 0x01;
		if (bNewState)
		{
			Rid[i].dwFlags = /*RIDEV_NOLEGACY | */ RIDEV_INPUTSINK;
			Rid[i].hwndTarget = hWndMain;
		}
		else
		{
			Rid[i].dwFlags = RIDEV_REMOVE;
			Rid[i].hwndTarget = NULL;
		}

	}
	Rid[0].usUsage = 0x06; // adds HID keyboard
	Rid[1].usUsage = 0x02; // adds HID mouse

	if (RegisterRawInputDevices(Rid, 2, sizeof(Rid[0])) == FALSE) {
		//registration failed. Call GetLastError for the cause of the error
		MessageBox(NULL, _T("There was an error with registration"), _T("ERROR"), MB_ICONERROR);
	}
	else
	{
		bRunning = bNewState;
		if (bRunning)
		{
			LogMessage(_T("Starting logging\r\n"));
		}
		else
		{
			LogMessage(_T("Logging stopped\r\n"));
		}
	}

	UpdateStartStopButton();

}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance;
	hWndMain = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

	if (!hWndMain)
	{
		return FALSE;
	}

	hEditBack = CreateSolidBrush(RGB(255, 255, 255));

	hWndStartStop = CreateWindowEx(WS_EX_CLIENTEDGE, _T("Button"), _T("Stop"),
		WS_CHILD | WS_VISIBLE, 0, 0, 20, 60, hWndMain, NULL, NULL, NULL);
	hWndEdit = CreateWindowEx(WS_EX_CLIENTEDGE, _T("Edit"), _T(""),
		WS_CHILD | WS_VISIBLE | ES_READONLY | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_HSCROLL, 0, 20, 200,
		200, hWndMain, NULL, NULL, NULL);

	SendMessage(hWndEdit, EM_LIMITTEXT, 0x7FFFFFFE, 0);
	HFONT hFont = CreateFont(
		-MulDiv(10, GetDeviceCaps(GetDC(hWndEdit), LOGPIXELSY), 72),
		0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET,
		OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE, TEXT("Courier New"));
	SendMessage(hWndEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

	Resize(hWndMain);

	ShowWindow(hWndMain, nCmdShow);
	UpdateWindow(hWndMain);

	StartStop();

	return TRUE;
}

void LogMessage(const TCHAR* msg)
{
	int index = GetWindowTextLength(hWndEdit);
	// Escape hatch for the input box getting too big
	if (SendMessage(hWndEdit, WM_GETTEXTLENGTH, 0, 0) > 10 * 1024 * 1024)
	{
		SendMessage(hWndEdit, WM_SETTEXT, (WPARAM)0, (LPARAM)_T(""));
	}
	SendMessage(hWndEdit, EM_SETSEL, (WPARAM)index, (LPARAM)index);
	SendMessage(hWndEdit, EM_REPLACESEL, 0, (LPARAM)msg);

	char utf8[2000] = { 0 };
	int utf8Length = WideCharToMultiByte(CP_UTF8, 0, msg, (int)_tcslen(msg), utf8, (int)(sizeof(utf8)), NULL, NULL);

	if (utf8Length > 0)
	{
		HANDLE hFile = CreateFile(_T("RawInput.log"), FILE_APPEND_DATA, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (hFile != NULL && hFile != INVALID_HANDLE_VALUE)
		{
			SYSTEMTIME st = { 0 };
			GetSystemTime(&st);

			char header[50] = { 0 };
			sprintf_s(header, "%d-%02d-%02d %2d:%02d:%02d.%04d: ",
				st.wYear, st.wMonth, st.wDay,
				st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

			SetFilePointer(hFile, 0, NULL, FILE_END);
			WriteFile(hFile, header, (DWORD)(strlen(header)), NULL, 0);
			WriteFile(hFile, utf8, (DWORD)(strlen(utf8)), NULL, NULL);
			CloseHandle(hFile);
		}
	}
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_SIZE:
	{
		Resize(hWnd);
		break;
	}
	case WM_CTLCOLORSTATIC:
	{
		return (INT_PTR)hEditBack;
	}
	case WM_INPUT:
	{
		UINT dwSize = 0;
		GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize,
			sizeof(RAWINPUTHEADER));
		LPBYTE lpb = new BYTE[dwSize];
		if (lpb == NULL)
		{
			return 0;
		}

		if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize,
			sizeof(RAWINPUTHEADER)) != dwSize)
		{
			OutputDebugString(_T("GetRawInputData does not return correct size !\n"));
		}
		RAWINPUT* raw = (RAWINPUT*)lpb;
		if (raw->header.dwType == RIM_TYPEKEYBOARD)
		{
			const TCHAR* szMap[] = {
				_T("--"), _T("LBUTTON"), _T("RBUTTON"), _T("CANCEL"), _T("MBUTTON"), _T("XBUTTON1"), _T("XBUTTON2"), _T("--"),
				_T("BACK"), _T("TAB"), _T("--"), _T("--"), _T("CLEAR"), _T("RETURN"), _T("--"), _T("--"), _T("SHIFT"), _T("CONTROL"),
				_T("MENU"), _T("PAUSE"), _T("CAPITAL"), _T("KANA"), _T("--"), _T("JUNJA"), _T("FINAL"), _T("HANJA"), _T("--"),
				_T("ESCAPE"), _T("CONVERT"), _T("NONCONVERT"), _T("ACCEPT"), _T("MODECHANGE"), _T("SPACE"), _T("PRIOR"), _T("NEXT"),
				_T("END"), _T("HOME"), _T("LEFT"), _T("UP"), _T("RIGHT"), _T("DOWN"), _T("SELECT"), _T("PRINT"), _T("EXECUTE"),
				_T("SNAPSHOT"), _T("INSERT"), _T("DELETE"), _T("HELP"), _T("0"), _T("1"), _T("2"), _T("3"), _T("4"), _T("5"), _T("6"),
				_T("7"), _T("8"), _T("9"), _T("--"), _T("--"), _T("--"), _T("--"), _T("--"), _T("--"), _T("--"), _T("A"), _T("B"),
				_T("C"), _T("D"), _T("E"), _T("F"), _T("G"), _T("H"), _T("I"), _T("J"), _T("K"), _T("L"), _T("M"), _T("N"), _T("O"),
				_T("P"), _T("Q"), _T("R"), _T("S"), _T("T"), _T("U"), _T("V"), _T("W"), _T("X"), _T("Y"), _T("Z"), _T("LWIN"),
				_T("RWIN"), _T("APPS"), _T("--"), _T("SLEEP"), _T("NUMPAD0"), _T("NUMPAD1"), _T("NUMPAD2"), _T("NUMPAD3"),
				_T("NUMPAD4"), _T("NUMPAD5"), _T("NUMPAD6"), _T("NUMPAD7"), _T("NUMPAD8"), _T("NUMPAD9"), _T("MULTIPLY"), _T("ADD"),
				_T("SEPARATOR"), _T("SUBTRACT"), _T("DECIMAL"), _T("DIVIDE"), _T("F1"), _T("F2"), _T("F3"), _T("F4"), _T("F5"),
				_T("F6"), _T("F7"), _T("F8"), _T("F9"), _T("F10"), _T("F11"), _T("F12"), _T("F13"), _T("F14"), _T("F15"), _T("F16"),
				_T("F17"), _T("F18"), _T("F19"), _T("F20"), _T("F21"), _T("F22"), _T("F23"), _T("F24"), _T("NAVIGATION_VIEW"),
				_T("NAVIGATION_MENU"), _T("NAVIGATION_UP"), _T("NAVIGATION_DOWN"), _T("NAVIGATION_LEFT"), _T("NAVIGATION_RIGHT"),
				_T("NAVIGATION_ACCEPT"), _T("NAVIGATION_CANCEL"), _T("NUMLOCK"), _T("SCROLL"), _T("OEM_NEC_EQUAL"),
				_T("OEM_FJ_MASSHOU"), _T("OEM_FJ_TOUROKU"), _T("OEM_FJ_LOYA"), _T("OEM_FJ_ROYA"), _T("--"), _T("--"), _T("--"),
				_T("--"), _T("--"), _T("--"), _T("--"), _T("--"), _T("--"), _T("LSHIFT"), _T("RSHIFT"), _T("LCONTROL"),
				_T("RCONTROL"), _T("LMENU"), _T("RMENU"), _T("BROWSER_BACK"), _T("BROWSER_FORWARD"), _T("BROWSER_REFRESH"),
				_T("BROWSER_STOP"), _T("BROWSER_SEARCH"), _T("BROWSER_FAVORITES"), _T("BROWSER_HOME"), _T("VOLUME_MUTE"),
				_T("VOLUME_DOWN"), _T("VOLUME_UP"), _T("MEDIA_NEXT_TRACK"), _T("MEDIA_PREV_TRACK"), _T("MEDIA_STOP"),
				_T("MEDIA_PLAY_PAUSE"), _T("LAUNCH_MAIL"), _T("LAUNCH_MEDIA_SELECT"), _T("LAUNCH_APP1"), _T("LAUNCH_APP2"), _T("--"),
				_T("--"), _T("OEM_1_COLON"), _T("OEM_PLUS"), _T("OEM_COMMA"), _T("OEM_MINUS"), _T("OEM_PERIOD"), _T("OEM_2_SLASH"),
				_T("OEM_3_TILDE"), _T("--"), _T("--"), _T("GAMEPAD_A"), _T("GAMEPAD_B"), _T("GAMEPAD_X"), _T("GAMEPAD_Y"),
				_T("GAMEPAD_RIGHT_SHOULDER"), _T("GAMEPAD_LEFT_SHOULDER"), _T("GAMEPAD_LEFT_TRIGGER"), _T("GAMEPAD_RIGHT_TRIGGER"),
				_T("GAMEPAD_DPAD_UP"), _T("GAMEPAD_DPAD_DOWN"), _T("GAMEPAD_DPAD_LEFT"), _T("GAMEPAD_DPAD_RIGHT"), _T("GAMEPAD_MENU"),
				_T("GAMEPAD_VIEW"), _T("GAMEPAD_LEFT_THUMBSTICK_BUTTON"), _T("GAMEPAD_RIGHT_THUMBSTICK_BUTTON"),
				_T("GAMEPAD_LEFT_THUMBSTICK_UP"), _T("GAMEPAD_LEFT_THUMBSTICK_DOWN"), _T("GAMEPAD_LEFT_THUMBSTICK_RIGHT"),
				_T("GAMEPAD_LEFT_THUMBSTICK_LEFT"), _T("GAMEPAD_RIGHT_THUMBSTICK_UP"), _T("GAMEPAD_RIGHT_THUMBSTICK_DOWN"),
				_T("GAMEPAD_RIGHT_THUMBSTICK_RIGHT"), _T("GAMEPAD_RIGHT_THUMBSTICK_LEFT"), _T("OEM_4_OPEN_BRACKET"),
				_T("OEM_5_BACKSLASH"), _T("OEM_6_CLOSE_BRACKET"), _T("OEM_7_QUOTE"), _T("OEM_8"), _T("--"), _T("OEM_AX"),
				_T("OEM_102"), _T("ICO_HELP"), _T("ICO_00"), _T("PROCESSKEY"), _T("ICO_CLEAR"), _T("PACKET"), _T("--"),
				_T("OEM_RESET"), _T("OEM_JUMP"), _T("OEM_PA1"), _T("OEM_PA2"), _T("OEM_PA3"), _T("OEM_WSCTRL"), _T("OEM_CUSEL"),
				_T("OEM_ATTN"), _T("OEM_FINISH"), _T("OEM_COPY"), _T("OEM_AUTO"), _T("OEM_ENLW"), _T("OEM_BACKTAB"), _T("ATTN"),
				_T("CRSEL"), _T("EXSEL"), _T("EREOF"), _T("PLAY"), _T("ZOOM"), _T("NONAME"), _T("PA1"), _T("OEM_CLEAR"), _T("--"),
			};

			const TCHAR* szExtra = _T("");
			switch (raw->data.keyboard.Message) {
			case WM_KEYDOWN:
				szExtra = _T("Down");
				break;
			case WM_KEYUP:
				szExtra = _T("Up");
				break;
			case WM_DEADCHAR:
				szExtra = _T("Dead");
				break;
			case WM_SYSKEYUP:
				szExtra = _T("Sys Up");
				break;
			case WM_SYSKEYDOWN:
				szExtra = _T("Sys Down");
				break;
			case WM_SYSDEADCHAR:
				szExtra = _T("Sys Dead");
				break;
			}

			TCHAR szTempOutput[1000];
			_stprintf_s(szTempOutput, _T("Keybd: Dev:%08I64x Make:%04x Flags:%04x Rsvd:%04x Extra:%08x Msg:%04x(%-8s) VK:%02x(%s)\n"),
				(UINT64)raw->header.hDevice,
				raw->data.keyboard.MakeCode,
				raw->data.keyboard.Flags,
				raw->data.keyboard.Reserved,
				raw->data.keyboard.ExtraInformation,
				raw->data.keyboard.Message,
				szExtra,
				raw->data.keyboard.VKey,
				raw->data.keyboard.VKey < 256 ? szMap[raw->data.keyboard.VKey] : _T("--"));
			LogMessage(szTempOutput);
		}
		else if (raw->header.dwType == RIM_TYPEMOUSE)
		{
			// Only log mouse button presses
			if (raw->data.mouse.usButtonFlags > 0)
			{
				const TCHAR* szButtonInfo = _T("--");
				switch (raw->data.mouse.ulButtons)
				{
				case RI_MOUSE_LEFT_BUTTON_DOWN:		szButtonInfo = _T("LButtonDown"); break;
				case RI_MOUSE_LEFT_BUTTON_UP:		szButtonInfo = _T("LButtonUp"); break;
				case RI_MOUSE_MIDDLE_BUTTON_DOWN:	szButtonInfo = _T("MButtonDown"); break;
				case RI_MOUSE_MIDDLE_BUTTON_UP:		szButtonInfo = _T("MButtonUp"); break;
				case RI_MOUSE_RIGHT_BUTTON_DOWN:	szButtonInfo = _T("RButtonDown"); break;
				case RI_MOUSE_RIGHT_BUTTON_UP:		szButtonInfo = _T("RButtonUp"); break;
				case RI_MOUSE_BUTTON_4_DOWN:		szButtonInfo = _T("Button4Down"); break;
				case RI_MOUSE_BUTTON_4_UP:			szButtonInfo = _T("Button4Up"); break;
				case RI_MOUSE_BUTTON_5_DOWN:		szButtonInfo = _T("Button5Down"); break;
				case RI_MOUSE_BUTTON_5_UP:			szButtonInfo = _T("Button5Up"); break;
				case RI_MOUSE_WHEEL:				szButtonInfo = _T("Wheel"); break;
				case RI_MOUSE_HWHEEL:				szButtonInfo = _T("HWhell"); break;
				}

				TCHAR szTempOutput[1000];
				_stprintf_s(szTempOutput, _T("Mouse: Dev:%08I64x Flags:%04x Buttons:%08x(%-12s) Flags:%04x Data:%04x Raw:%04x LastX:%04x LastY:%04x Extra:%04x\r\n"),
					(UINT64)raw->header.hDevice,
					raw->data.mouse.usFlags,
					raw->data.mouse.ulButtons,
					szButtonInfo,
					raw->data.mouse.usButtonFlags,
					raw->data.mouse.usButtonData,
					raw->data.mouse.ulRawButtons,
					raw->data.mouse.lLastX,
					raw->data.mouse.lLastY,
					raw->data.mouse.ulExtraInformation);
				LogMessage(szTempOutput);
			}
		}

		delete[] lpb;
	}
	return 0;

	case WM_COMMAND:
	{
		int cmd = HIWORD(wParam);
		switch (cmd)
		{
		case BN_CLICKED:
			HWND hWndSource = (HWND)lParam;
			if (hWndSource == hWndStartStop)
			{
				StartStop();
			}

			break;
		}

		int wmId = LOWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
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
		// TODO: Add any drawing code that uses hdc here...
		EndPaint(hWnd, &ps);
	}
	break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

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

#define START_HEIGHT 25
#define START_WIDTH 60
#define START_VERT_MARGIN 5
#define START_HORZ_MARGIN 5


void Resize(HWND hWnd)
{
	RECT rt;
	GetClientRect(hWnd, &rt);
	MoveWindow(hWndStartStop, START_HORZ_MARGIN + rt.left, rt.top + START_VERT_MARGIN, START_WIDTH, START_HEIGHT, TRUE);

	int topDist = START_HEIGHT + (START_VERT_MARGIN * 2);

	MoveWindow(hWndEdit, rt.left, rt.top + topDist, rt.right - rt.left, rt.bottom - rt.top - topDist, TRUE);
}

void StartStop()
{
	RegisterRawInput(!bRunning);
}

void UpdateStartStopButton()
{
	if (bRunning)
	{
		SetWindowText(hWndStartStop, _T("Stop"));
	}
	else
	{
		SetWindowText(hWndStartStop, _T("Start"));
	}
}
