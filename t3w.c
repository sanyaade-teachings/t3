#define _WIN32_WINNT	0x0501
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "t3.h"
#include "t3w.h"

static void*
mapFile(wchar_t *fn, int *size) {
	HANDLE	f = CreateFile(fn, GENERIC_READ, FILE_SHARE_READ, 0,
		OPEN_EXISTING, 0, 0);
	HANDLE	m;
	DWORD	sz, read;
	char	*dat;
	
	if (f == INVALID_HANDLE_VALUE)
		return 0;

	m = CreateFileMapping(f, 0, PAGE_READONLY, 0,0, 0);
	if (m == INVALID_HANDLE_VALUE)
		return 0;
	sz = GetFileSize(f, 0);
	dat = MapViewOfFile(m, FILE_MAP_READ, 0,0, 0);
	CloseHandle(m);
	CloseHandle(f);
	if (size)
		*size = sz;
	return dat;
}

HBITMAP
t3w_initBM(T3_bm *bm, int x, int y) {
	BITMAPINFOHEADER bmih={sizeof(bmih),0,0,1,32,0,0,0,0,-1,-1};
	HBITMAP		bitmap;
	
	bm->x = x;
	bm->y = y;
	bmih.biWidth = x;
	bmih.biHeight = -y;
	bmih.biSizeImage = x * y * 4;
	bitmap = CreateDIBSection(0,(void*)&bmih,
		DIB_RGB_COLORS, &bm->pix, 0, 0);
	return bitmap;
}

t3w_loadFontFile(T3_font *font, wchar_t *fn, int index, float height) {
	void	*dat = mapFile(fn, 0);
	int	err = 1;
	if (!dat || (err=t3_initFont(font,dat,index,height))) {
		free(dat);
		return err;
	}
	return 0;
}

t3w_findFont(T3_font *font, wchar_t *name, float height) {
	wchar_t	*reg = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";
	HKEY	key;
	DWORD	n;
	
	if (RegOpenKey(HKEY_LOCAL_MACHINE, reg, &key))
		return 1;
	if (RegQueryInfoKey(key, 0,0, 0, 0,0, 0, &n, 0, 0, 0, 0))
	  	return 1;
	
	while (n--) {
		wchar_t	*j,*i,tag[1024];
		DWORD	nlen = 1024;
		int	sub;
		
		RegEnumValue(key, n, tag, &nlen, 0, 0,0,0);
		if (j = wcsstr(tag, L" (TrueType)"))
			*j = 0;
		else
			continue;			
			
		i = tag;
		sub = 0;
loop: 			/* Find subfonts in a TrueType Collection */
		j=wcsstr(i, L" & ");
		if (j)
			*j = 0;
		if (!wcsicmp(i, name)) {
			wchar_t fn[MAX_PATH];
			DWORD vlen = MAX_PATH - 7
				- GetWindowsDirectory(fn, MAX_PATH);
			wcscat(fn, L"\\Fonts\\");
			nlen = MAX_PATH;
			RegEnumValue(key, n, tag,&nlen, 0,
				0, (char*)(fn+wcslen(fn)), &vlen);
			return t3w_loadFontFile(font, fn,sub,height);
		}
		i = j + 3;
		sub++;
		if (j)
			goto loop;
	}
	return 2;
}

t3w_closeFont(T3_font *font) {
	t3_closeFont(font);
	UnmapViewOfFile(font->dat);
	return 0;
}
