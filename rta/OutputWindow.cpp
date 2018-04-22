
#include "stdafx.h"

HWND hwndOutputWindow = NULL;
HANDLE hWindowThread = NULL;
HDC hdcDoubleBuffer = NULL;
HBITMAP bitmapDoubleBuffer = NULL;
HGDIOBJ gdiobjOldBitmap = NULL;
BYTE* bitsDoubleBuffer = NULL;

// external stuff
DWORD dwNumOutputWindowBuffers = 0;
LPWINDOW_OUTPUT_BUFFER lpOutputWindowBuffers;

void PaintWindow(HWND hWnd)
{
	RECT r;

	GetClientRect(hWnd, &r);

	// fade white pixels to gray
	BITMAPINFO bmi = { 0 };
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = r.right - r.left;
	bmi.bmiHeader.biHeight = r.bottom - r.top;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biSizeImage = 0;

	int iResult = GetDIBits(hdcDoubleBuffer, bitmapDoubleBuffer, 0, r.bottom - r.top, 
		(LPVOID)bitsDoubleBuffer, &bmi, DIB_RGB_COLORS);
	if (iResult > 0) {
		for (LONG p = 0; p < bmi.bmiHeader.biWidth * bmi.bmiHeader.biHeight * 4; p+=4) {
			if (bitsDoubleBuffer[p] > 0) bitsDoubleBuffer[p] = MulDiv(bitsDoubleBuffer[p], 9, 10);
			if (bitsDoubleBuffer[p + 1] > 0) bitsDoubleBuffer[p+1] = MulDiv(bitsDoubleBuffer[p + 1], 9, 10);
			if (bitsDoubleBuffer[p + 2] > 0) bitsDoubleBuffer[p+2] = MulDiv(bitsDoubleBuffer[p + 2], 9, 10);
			if (bitsDoubleBuffer[p + 3] > 0) bitsDoubleBuffer[p+3] = MulDiv(bitsDoubleBuffer[p + 3], 9, 10);
		}
		iResult = SetDIBits(hdcDoubleBuffer, bitmapDoubleBuffer, 0, iResult, 
			(LPVOID)bitsDoubleBuffer, &bmi, DIB_RGB_COLORS);
	}

	HGDIOBJ oldObj = SelectObject(hdcDoubleBuffer, (HBRUSH)GetStockObject(WHITE_PEN));

	if (dwNumOutputWindowBuffers > 0) {
		if (lpOutputWindowBuffers != NULL) {
			for (DWORD i = 0; i < dwNumOutputWindowBuffers; i++) {
				if (lpOutputWindowBuffers[i].BufferPointer != NULL) {

					FLOAT_FRAME_2CH* SampleBuffer = reinterpret_cast<FLOAT_FRAME_2CH*>(lpOutputWindowBuffers[i].BufferPointer);
					MoveToEx(hdcDoubleBuffer, 0, 100, NULL);
					for (DWORD sample = 0; sample < lpOutputWindowBuffers[i].SamplesPerBuffer; sample++) 
					{
						LineTo(hdcDoubleBuffer, sample, static_cast<int>(SampleBuffer[sample].left * 100.0f) + 100);
					}
					MoveToEx(hdcDoubleBuffer, 0, 300, NULL);
					for (DWORD sample = 0; sample < lpOutputWindowBuffers[i].SamplesPerBuffer; sample++)
					{
						LineTo(hdcDoubleBuffer, sample, static_cast<int>(SampleBuffer[sample].right * 100.0f) + 300);
					}
				}
			}
		}
	}

	SelectObject(hdcDoubleBuffer, oldObj);
}

void RebuildDoubleBuffer(HWND hWnd, LPARAM lParam)
{
	int w = LOWORD(lParam);
	int h = HIWORD(lParam);

	HDC tdc = GetDC(hWnd);
	if (hdcDoubleBuffer == NULL) {
		hdcDoubleBuffer = CreateCompatibleDC(tdc);
	}
	else {
		SelectObject(hdcDoubleBuffer, gdiobjOldBitmap);
		DeleteObject(bitmapDoubleBuffer);
		if (bitsDoubleBuffer != NULL) {
			free(bitsDoubleBuffer);
			bitsDoubleBuffer = NULL;
		}
	}
	bitmapDoubleBuffer = CreateCompatibleBitmap(tdc, w, h);
	ReleaseDC(hWnd, tdc);

	gdiobjOldBitmap = SelectObject(hdcDoubleBuffer, bitmapDoubleBuffer);
	BITMAP bm;
	GetObject(bitmapDoubleBuffer, sizeof(BITMAP), &bm);
	bitsDoubleBuffer = (BYTE*)malloc(bm.bmHeight * bm.bmWidth * 4);
	ZeroMemory(bitsDoubleBuffer, bm.bmHeight * bm.bmWidth * 4);
}

void ReleaseDoubleBuffer()
{
	if (hdcDoubleBuffer != NULL) {
		SelectObject(hdcDoubleBuffer, gdiobjOldBitmap);
		DeleteObject(bitmapDoubleBuffer);
		DeleteDC(hdcDoubleBuffer);
	}
	if (bitsDoubleBuffer != NULL) {
		free(bitsDoubleBuffer);
		bitsDoubleBuffer = NULL;
	}
}

LRESULT CALLBACK OutputWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HDC hdc = NULL;
	PAINTSTRUCT ps;
	RECT r;

	switch (uMsg) {
	case WM_CREATE:
		break;
	case WM_DESTROY:
		ReleaseDoubleBuffer();
		PostQuitMessage(0);
		break;
	case WM_SIZE:
		RebuildDoubleBuffer(hWnd, lParam);
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		GetClientRect(hWnd, &r);
		BitBlt(hdc, 0, 0, r.right - r.left, r.bottom - r.top, hdcDoubleBuffer, 0, 0, SRCCOPY);
		EndPaint(hWnd, &ps);
		break;
	case RTAM_CLOSE_WINDOW:
		DestroyWindow(hWnd);
		break;
	case RTAM_UPDATE_WINDOW:
		PaintWindow(hWnd);
		InvalidateRect(hWnd, NULL, FALSE);
		break;
	default:
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
	return 0;
}


DWORD WINAPI OutputWindowThread(LPVOID lpParameter)
{
	BOOL bRet;
	MSG msg;
	WNDCLASS wc = {
		CS_HREDRAW | CS_VREDRAW | CS_NOCLOSE, // style
		OutputWindowProc, // window proc
		0, // cls extra
		0, // wnd extra
		GetModuleHandle(NULL), // hinstance
		(HICON)NULL, // icon
		LoadCursor(NULL, IDC_ARROW),
		(HBRUSH)(COLOR_WINDOW + 1),
		(LPCWSTR)NULL,
		L"ASIO_OUTPUT_WINDOW"
	};
	ATOM cls = RegisterClass(&wc);
	if (0 == cls) return 0;
	hwndOutputWindow = CreateWindow(
		L"ASIO_OUTPUT_WINDOW",
		L"ASIO_OUTPUT",
		// no WS_THICKFRAME
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, 640, 480,
		NULL, // parent
		NULL, // menu
		GetModuleHandle(NULL),
		(LPVOID)NULL); // LPARAM WM_CREATE
	if (hwndOutputWindow == NULL) return 0;
	ShowWindow(hwndOutputWindow, SW_SHOW);

	while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
	{
		if (bRet == -1) {
			// handle error and exit
			break;
		}
		else if (msg.message == WM_QUIT) {
			// quit
			break;
		}
		else {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	hwndOutputWindow = NULL;

	return 0;
}

HANDLE RtaStartOutputWindow(DWORD p_dwNumOutputWindowBuffers, LPWINDOW_OUTPUT_BUFFER p_lpOutputWindowBuffers)
{
	dwNumOutputWindowBuffers = p_dwNumOutputWindowBuffers;
	lpOutputWindowBuffers = p_lpOutputWindowBuffers;
	hWindowThread = CreateThread(NULL, 0, OutputWindowThread, (LPVOID)NULL, 0, NULL);
	return hWindowThread;
}

void RtaStopOutputWindow()
{
	// send message to window to terminate itself
	if (hwndOutputWindow != NULL) {
		SendMessage(hwndOutputWindow, RTAM_CLOSE_WINDOW, 0, 0);
	}
}

void RtaUpdateOutputWindow()
{
	// send a message to the window that new data is available
	if (hwndOutputWindow != NULL) {
		SendMessage(hwndOutputWindow, RTAM_UPDATE_WINDOW, 0, 0);
	}
}

void RtaCleanupOutputWindow()
{
	if (hWindowThread != NULL) {
		CloseHandle(hWindowThread);
		hWindowThread = NULL;
	}
}

DWORD RtaGetNumOutputWindowBuffers()
{
	return dwNumOutputWindowBuffers;
}

LPWINDOW_OUTPUT_BUFFER RtaGetWindowOutputBuffers()
{
	return lpOutputWindowBuffers;
}
