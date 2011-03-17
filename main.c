#define _WIN32_WINNT	0x0501
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t3.h"
#include "t3w.h"

T3_face		face;
T3_bm		bm;
HBITMAP		bitmap;
wchar_t		lines[20][4096];

init(wchar_t *fontname) {
	FILE	*f = fopen("prologue.txt", "rb");
	int	i = 0;
	while (fgetws(lines[i++], 4096, f));
	fclose(f);
	
	if (!fontname)
		fontname = L"Tahoma";
		
	if (!t3w_findFace(&face, fontname, 12*96/72.0))
		return 0;
	return 1;
}

redraw(T3_bm bm) {
	T3_pt	pt = t3_pt(0,0);
	T3_col	col = t3_rgb(32,32,32);
	wchar_t	*txt;
	int	l;
	
	t3_clearBM(bm, t3_rgb(255,255,230));
	for (l=0; (txt = lines[l])[0]; l++) {
		int	i,n,end = wcscspn(txt, L"\n");
		txt[end] = 0;
		for (i=0; i<end; i+=n) {
			n = t3_fitUnicode(&face, bm.x, txt+i, -1, 0);
			while (n>0 && txt[i+n] && !iswspace(txt[i+n]))
				n--;
			if (!n)
				n = 1;
			
			t3_drawUnicode(&face, bm, pt, txt+i, n, col);
			pt.y += t3_getHeight(&face);
			if (iswspace(txt[i+n]))
				n++;
		}
	}
}

LRESULT CALLBACK
WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	PAINTSTRUCT	ps;
	HDC		cdc;
	float		delta;
	
	switch (msg) {
	case WM_PAINT:
		BeginPaint(hwnd,&ps);
		cdc = CreateCompatibleDC(ps.hdc);
		SelectObject(cdc, bitmap);
		BitBlt(ps.hdc,
			ps.rcPaint.left,
			ps.rcPaint.top,
			ps.rcPaint.right - ps.rcPaint.left,
			ps.rcPaint.bottom - ps.rcPaint.top,
			cdc,
			ps.rcPaint.left,
			ps.rcPaint.top,
			SRCCOPY);
		DeleteDC(cdc);
		EndPaint(hwnd,&ps);
		return 0;
	case WM_ERASEBKGND:
		return 1;
	case WM_MOUSEWHEEL:
		delta = (short)HIWORD(wparam)/120.0;
		if (wparam & MK_CONTROL) {
			face.samples += delta * 1;
			if (face.samples < 1) face.samples = 1;
			printf("samples: %f\n",face.samples);
		} else {
			if (t3_getHeight(&face) + delta > 0)
				t3_rescale(&face,
					t3_getHeight(&face) + 96./72/2*delta,
					0);
			printf("scale: %f %fpx %fpt\n", face.scale.y,
				t3_getHeight(&face),
				t3_getHeight(&face) * 72/96);
		}
		redraw(bm);
		InvalidateRect(hwnd, 0, 0);
		return 1;
	case WM_SIZE:
		DeleteObject(bitmap);
		bitmap = t3w_initBM(&bm, LOWORD(lparam), HIWORD(lparam));
		redraw(bm);
		break;
	case WM_DESTROY:
		DeleteObject(bitmap);
		t3w_closeFace(&face);
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd,msg,wparam,lparam);
}

int WINAPI
wWinMain(HINSTANCE inst, HINSTANCE prev, LPTSTR cmd, int show) {
	MSG		msg;
	HWND		hwnd;
	WNDCLASS	wc={ CS_DBLCLKS|CS_HREDRAW|CS_VREDRAW,
		WndProc,0,0,0,0,0,(HBRUSH)(COLOR_WINDOW+1),0,
		L"Window"};
	wc.hIcon=LoadIcon(0,IDI_APPLICATION);
	wc.hCursor=LoadCursor(0,IDC_ARROW);
	RegisterClass(&wc);
	
	if (!init(cmd))
		return 0;
		
	hwnd=CreateWindow(L"Window", L"",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT,CW_USEDEFAULT,
		320,480,
		NULL,NULL,inst,NULL);
	while (GetMessage(&msg,NULL,0,0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return (int) msg.wParam;
}

wmain(int argc, wchar_t **argv) {
	return wWinMain(GetModuleHandle(0),0,argv[1],0);
}
