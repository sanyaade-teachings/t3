#define _WIN32_WINNT	0x0501
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t3.h"

T3_face		face[19];
float		scroll = 0;
BITMAPINFOHEADER bmih={sizeof(bmih),0,0,1,32,0,0,0,0,256,256};
T3_col		bmd[800*600];
T3_bm		bm={bmd, 800,600};

loadttf(T3_face *face, char *fn, int index, float height) {
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
	if (t3_initFace(face,dat,index,height))
		return 1;
	free(dat);
	return 0;
}

gen(T3_bm bm,  T3_pt pt) {
	wchar_t	txt[]=L"xxpt The quick brown fox jumped over the lazy dog";
	float	r;
	int	i;
	
	for (i = 0; i < bm.x * bm.y; i++)
//		bm.bm[i] = t3_rgb(64,64,64);
		bm.bm[i] = t3_rgb(255,255,230);
	for (i = 0; i < 19; i++) {
		float	px = 9+i;
		int	n;
		txt[0] = (int) px / 10 + '0';
		txt[1] = (int) px % 10 + '0';
		n = t3_fitUnicode(face+i, bm.x, txt, -1, 0);
		t3_drawUnicode(face+i, bm, pt, txt, n,
//			t3_rgb(255,127,39));
			t3_rgb(80,80,128));
		pt.y += px*96/72;
	}
}

#include "dumpbm.c"
init(HWND hwnd) {
	HDC	dc;
	int	i;
	
	bmih.biWidth=bm.x;
	bmih.biHeight=-bm.y;
	bmih.biSizeImage=bm.x*bm.y;
	for (i = 0; i < 19; i++)
		loadttf(face+i,
			"c:/windows/fonts/segoeuii.ttf", 0,
			(i+9)*96/72.0);

	gen(bm, t3_pt(0,0));
	dump32bm("out.bmp",bm.bm,bm.x,bm.y);
	return 1;
}

paint(HWND hwnd) {
	PAINTSTRUCT	ps;
	RECT		rt={0,};
	BeginPaint(hwnd,&ps);
	AdjustWindowRect(&rt, WS_OVERLAPPEDWINDOW, 0);
	SetDIBits(ps.hdc,
		GetCurrentObject(ps.hdc,OBJ_BITMAP),
		rt.top, bm.y, bm.bm, (void*)&bmih, DIB_RGB_COLORS);
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
	case WM_CREATE:
		return !init(hwnd);
	case WM_TIMER:
		gen(bm, t3_pt(scroll,0));
		scroll += 1;
		InvalidateRect(hwnd,0,0);
		break;
	case WM_DESTROY:
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
	hwnd=CreateWindow(L"Window", L"",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT,CW_USEDEFAULT,
		CW_USEDEFAULT,CW_USEDEFAULT,
		NULL,NULL,inst,NULL);
	SetTimer(hwnd,0,1000/60,0);
	while (GetMessage(&msg,NULL,0,0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return (int) msg.wParam;
}

main() {
	return WinMain(GetModuleHandle(0),0,0,0);
}
