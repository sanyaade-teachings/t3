#define _WIN32_WINNT	0x0501
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t3.h"

T3_face		face;
T3_bm		bm={0, 0, 0};
HBITMAP		bitmap;

clearBM(T3_bm bm, T3_col col) {
	int	i;
	for (i = 0; i < bm.x * bm.y; i++)
		bm.bm[i] = col;
	return 1;
}

HBITMAP
initBM(T3_bm *bm, int x, int y) {
	BITMAPINFOHEADER bmih={sizeof(bmih),0,0,1,32,0,0,0,0,-1,-1};
	HBITMAP		bitmap;
	
	bm->x = x;
	bm->y = y;
	bmih.biWidth = x;
	bmih.biHeight = -y;
	bmih.biSizeImage = x * y * 4;
	bitmap = CreateDIBSection(0,(void*)&bmih,
		DIB_RGB_COLORS, &bm->bm, 0, 0);
	return bitmap;
}

void*
loadFile(char *fn, int *size) {
	FILE	*f=fopen(fn,"rb");
	void	*dat;
	size_t	sz;
	if (!f)
		return 0;
	fseek(f,0,SEEK_END);
	sz=ftell(f);
	rewind(f);
	dat=malloc(sz);
	fread(dat,1,sz,f);
	fclose(f);
	if (size) *size = sz;
	return dat;
}

loadFace(T3_face *face, char *fn, int index, float height) {
	void	*dat = loadFile(fn, 0);
	if (t3_initFace(face,dat,index,height))
		return 1;
	free(dat);
	return 0;
}

redraw(T3_bm bm) {
	wchar_t	txt[128];
	FILE	*f = fopen("prologue.txt", "rb");
	T3_pt	pt = t3_pt(0,0);
	T3_col	col = t3_rgb(32,32,32);
	
	clearBM(bm, t3_rgb(255,255,230));
	while (fgetws(txt, 128, f)) {
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
	fclose(f);
}

init() {
	loadFace(&face, "georgiai.ttf", 0,
		15*96/72.0);
	return 1;
}

paint(HWND hwnd) {
	PAINTSTRUCT	ps;
	HDC		cdc;
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
}

LRESULT CALLBACK
WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	switch (msg) {
	case WM_PAINT:
		paint(hwnd);
		return 0;
	case WM_ERASEBKGND:
		return 1;
	case WM_SIZE:
		DeleteObject(bitmap);
		bitmap = initBM(&bm, LOWORD(lparam), HIWORD(lparam));
		redraw(bm);
		break;
	case WM_DESTROY:
		DeleteObject(bitmap);
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd,msg,wparam,lparam);
}

int WINAPI
WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show) {
	MSG		msg;
	HWND		hwnd;
	WNDCLASS	wc={ CS_DBLCLKS|CS_HREDRAW|CS_VREDRAW,
		WndProc,0,0,0,0,0,(HBRUSH)(COLOR_WINDOW+1),0,
		L"Window"};
	wc.hIcon=LoadIcon(0,IDI_APPLICATION);
	wc.hCursor=LoadCursor(0,IDC_ARROW);
	RegisterClass(&wc);
	
	init();
	
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

main() {
	return WinMain(GetModuleHandle(0),0,0,0);
}
