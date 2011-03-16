#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include "t3.h"

typedef unsigned char	u8;
typedef unsigned short	u16;
typedef unsigned int	u32;

#define MAXPT	256
#define MAXSEG	4096

typedef struct {
	float	x;
	int	dir;
} Edge;

#undef max
#undef min
#define max(x,y) ((x<y)? y: x)
#define min(x,y) ((x<y)? x: y)
#define LOADS(p,n) loads((u8*)((u16*)(p)+(n)))
#define LOADI(p,n) loadi((u8*)((u32*)(p)+(n)))
loads(u8 *p) {
	return (p[0] << 8) + p[1];
}
loadi(u8 *p) {
	return (p[0] << 24) + (p[1] << 16) + (p[2] << 8) + p[3];
}

static void*
unpack(void **datp, char *fmt, ...) {
	static char sz[1 << CHAR_BIT];
	va_list ap;
	u8 *dat = *datp;
	void *p;

	if (!sz['c'])		/* Initialise field size table */
		sz['c'] = 1, sz['s'] = 2, sz['i'] = 4;

	va_start(ap, fmt);
	for (; *fmt; dat += sz[*fmt++]) {
		if (*fmt != ':')	/* Unassigned field */
			continue;
		switch (*++fmt) {	/* Assigned field */
		case 'c':
			*va_arg(ap, u8 *) = *dat;
			break;
		case 's':
			*va_arg(ap, u16 *) = loads(dat);
			break;
		case 'i':
			*va_arg(ap, u32 *) = loadi(dat);
			break;
		}
	}
	va_end(ap);
	return *datp = dat;
}

static
cmp_edge(const void *a, const void *b) {	/* sort edges by left */
	float i = ((Edge *) a)->x, j = ((Edge *) b)->x;
	if (i < j)
		return -1;
	if (i > j)
		return 1;
	return 0;
}

static
cmp_seg(const void *a, const void *b) {	/* sort by top */
	float i = ((T3_seg *) a)->a.y, j = ((T3_seg *) b)->a.y;
	if (i < j)
		return -1;
	if (i > j)
		return 1;
	return 0;
}

static
docmap(T3_face *face, void *dat) {
	void *org = dat;
	u16 ntab, plat, enc, nseg;
	void *st, *et, *dt, *ot;	/* start,end,delta,offset table */
	u32 off;
	int i, c, c2;
	unpack(&dat, "s:s", &ntab);
	while (ntab--) {	/* Scan encoding table */
		unpack(&dat, ":s:s:i", &plat, &enc, &off);
		if (plat == 3 && enc == 1)	/* Windows/Unicode(BMP) */
			break;
	}
	if (ntab+1 == 0)
		return 0;
	dat = (char *)org + off;
	unpack(&dat, "sss:ssss", &nseg);
	nseg /= 2;		/* stored as 2n */
	et = dat;
	st = (u16 *) dat + 1 + nseg;
	dt = (u16 *) st + nseg;
	ot = (u16 *) dt + nseg;
	for (i = 0; i < nseg; i++) {
		c = LOADS(st, i), c2 = LOADS(et, i);
		if (LOADS(ot, i))
			for (; c <= c2; c++)
				face->cmap[c] = LOADS(ot,
					LOADS(ot, i) / 2 +
					c - LOADS(st, i) + i);
		else
			for (; c <= c2; c++)
				face->cmap[c] = LOADS(dt, i) + c;
	}
	return 1;
}

static
dohead(T3_face *face, void *dat) {
	short longloc;
	unpack(&dat, "iiiissiiiisssssss:ss", &longloc);
	face->longloc = longloc;
	return 1;
}

static
dohhea(T3_face *face, void *dat, u16 *nhm) {
	short	a, d;
	unpack(&dat, "i:s:ssssssssssssss:s", &a, &d, nhm);
	face->ascender = a;
	face->descender = d;
	face->scale.x /= a - d;
	face->scale.y /= a - d;
	return 1;
}

static
dohmtx(T3_face *face, void *dat, int n) {
	u16 *src = dat;
	int i;
	for (i = 0; i < n; i++, src += 2)
		face->adv[i] = LOADS(src, 0);
	for (src -= 2; i < 65536; i++)	/* Extend last entry */
		face->adv[i] = LOADS(src, 0);
	return 1;
}

static
domaxp(T3_face *face, void *dat) {
	unsigned short n;
	unpack(&dat, "i:s", &n);
	face->nglyph = n;
	return 1;
}

static void*
findtab(void *start, void *dat, u32 want, int ntab) {
	u32 tag, off, len;
	while (ntab--) {
		unpack(&dat, ":ii:i:i", &tag, &off, &len);
		if (tag == want)
			return (u8 *) start + off;
	}
	return 0;
}

t3_initFace(T3_face *face, void *dat, int index, float height) {
	u16	ntab, longform, nhm;
	void	*start = dat;
	
	memset(face, 0, sizeof face);
	face->dat = dat;
	memset(face->cmap, 0, sizeof(u16) * 65536);
	face->scale = t3_pt(height, height);
	
	if (LOADI(dat, 0) == 'ttcf') { /* TrueType collection */
		if (LOADI(dat, 2) <= index || index < 0)
			return 0;
		dat = (u8*)dat + LOADI(dat, 3+index);
	}
	unpack(&dat, "i:ssss", &ntab);	/* table header */

	if ( !docmap(face, findtab(start, dat, 'cmap', ntab))
	  || !dohead(face, findtab(start, dat, 'head', ntab))
	  || !dohhea(face, findtab(start, dat, 'hhea', ntab), &nhm)
	  || !dohmtx(face, findtab(start, dat, 'hmtx', ntab), nhm)
	  || !domaxp(face, findtab(start, dat, 'maxp', ntab))
	  || !(face->glyf = findtab(start, dat, 'glyf', ntab))
	  || !(face->loca = findtab(start, dat, 'loca', ntab)))
		return 0;
	return 1;
}

t3_close(T3_face *face) {
	t3_uncache(face, 0, face->nglyph + 1);
	return 0;
}


static T3_col
blend(T3_col src, T3_col col, float a) {
	T3_col al = a*255;
	T3_col rb = ((col & 0xff00ff) * al) +
		((src & 0xff00ff) * (255-al)) & 0xff00ff00;
	T3_col g = ((col & 0xff00) * al) +
		((src & 0xff00) * (255-al)) & 0xff0000;
	return 0xff000000 + ((rb | g) >> 8);
}

static void
rasterize(T3_bm bm, T3_pt at, float my, T3_seg *seg, T3_seg *eos, T3_col col) {
	Edge	edges[MAXSEG], *e, *eoe;
	float	i,y,sy;
	int	w,j, x, x2;
	T3_seg	*s;

	bm.bm += (int)at.y * bm.x;
	sy = (at.y + my >= bm.y)? bm.y - at.y: my;

	y = at.y;
	for (i = 0; i < sy; i++, y++, bm.bm += bm.x) {
		while (seg < eos && seg->b.y < i)
			seg++;
		if (seg == eos)
			break;
			
		/* Get edges that intersect this line */
		eoe=edges;
		for (s=seg; s < eos; s++)
			if (s->a.y < i && i <= s->b.y) {
				eoe->x = at.x + s->a.x
				  + s->m * (i - s->a.y);
				eoe++->dir = s->dir;
			}
			
		/* Remove overlapping edges */
		e=edges;
		qsort(e, eoe - e, sizeof *e, cmp_edge);
		for (w = 0, e++; e < eoe; e++)
			if (e->x == e[-1].x)
				w--;
			else
				e[w] = *e;
		eoe += w;
		
		/* Render edges */
		for (w = 0, e = edges; e < eoe; e++) {
			if (!(w += e->dir) || eoe <= e + 1)
				continue;
			
			x = e->x;
			x2 = min(bm.x, e[1].x);
			for (j=max(x+1, 0); j<x2; j++)
				bm.bm[j] = col;
			if (x >= 0 && x < bm.x)
				bm.bm[x] = blend(bm.bm[x], col,
					1 - (e->x - x));
			if (x2 >= 0 && x2 < bm.x)
				bm.bm[x2] = blend(bm.bm[x2], col,
					e[1].x - x2);
		}
	}
}

static void
bezier(T3_seg **seg, T3_pt a, T3_pt b, T3_pt c, float flat) {
	T3_pt m = { (a.x + 2 * b.x + c.x) / 4, (a.y + 2 * b.y + c.y) / 4 };
	T3_pt d = { (a.x + c.x) / 2 - m.x, (a.y + c.y) / 2 - m.y };
	if (d.x * d.x + d.y * d.y > flat) {
		bezier(seg, a, t3_pt((a.x + b.x) / 2, (a.y + b.y) / 2), m, flat);
		bezier(seg, m, t3_pt((b.x + c.x) / 2, (b.y + c.y) / 2), c, flat);
	} else
		*(*seg)++ = t3_seg(a, c);
}

static T3_seg*
inflate(T3_seg *seg, T3_pt *p, u8 *flags, int np, void *ends) {
	T3_pt start, pp, cp;
	int curve, close = 0, nc = 0, i;
	float flat = .05;

	for (i = 0; i < np; i++)
		if (close == i) {	/* end of contour */
			if (nc && curve)
				bezier(&seg, pp, cp, start, flat);
			else if (nc)
				*seg++ = t3_seg(pp, start);
			i--; /* counter act for i++ */
			close = LOADS(ends, nc++) + 1;
			start = pp = *p;
			curve = 0;
		} else if (~*flags++ & 1) {	/* off curve */
			if (curve) {
				T3_pt q = { (cp.x + p->x) / 2,
					(cp.y + p->y) / 2
				};
				bezier(&seg, pp, cp, q, flat);
				pp = q;
			}
			cp = *p++;
			curve = 1;
		} else {	/* point on curve */
			if (curve)
				bezier(&seg, pp, cp, *p, flat);
			else
				*seg++ = t3_seg(pp, *p);
			curve = 0;
			pp = *p++;
		}
	if (nc && curve)
		bezier(&seg, pp, cp, start, flat);
	else if (nc)
		*seg++ = t3_seg(pp, start);
	return seg;
}

t3_getShape(T3_face *face, T3_seg *segs, int g) {
	u8	flags[MAXPT];
	T3_pt	points[MAXPT];
	u8	*fileflag, *coord, *fp;
	short	nc, ax, ay, bx, by;
	int	np, i, v, nseg, top;
	void	*ends;
	T3_pt	*pp;
	
	/* Lookup 'glyf' offset of g in 'loca' */
	if (face->longloc)
		i = LOADI(face->loca, g), v = LOADI(face->loca, g + 1);
	else
		i = LOADS(face->loca, g) * 2, v = LOADS(face->loca, g + 1) * 2;

	if (i == v)		/* Glyph has no contours */
		return 0;
	ends = (u8 *) face->glyf + i;
	unpack(&ends, ":s:s:s:s:s", &nc, &ax, &ay, &bx, &by);

	top = face->ascender;

	if (MAXPT <= (np = LOADS(ends, nc - 1) + 1))	/* last pt of last contour */
		return 0;
	fileflag = (u8 *) ((u16 *) ends + nc + 1) +	/* instructions start */
		LOADS((u16 *) ends + nc, 0);		/* instruction count */
	for (fp = flags, i = np; i; i--)
		if ((*fp++ = *fileflag++) & 8)	/* repeat */
			for (v = *fileflag++; v--; i--)
				*fp++ = fileflag[-2];
	coord = fileflag;	/* x co-ordinates array */
	for (v = 0, i = np, fp = flags, pp = points; i--; fp++) {
		if (*fp & 2)	/* byte flag */
			v += *coord++ * (*fp & 16 ? 1 : -1);
		else if (~*fp & 16)	/* repeat flag */
			v += (short)loads(coord), coord += 2;
		pp++->x = v;
	}
	for (v = 0, i = np, fp = flags, pp = points; i--; fp++) {
		if (*fp & 4)	/* byte flag */
			v += *coord++ * (*fp & 32 ? 1 : -1);
		else if (~*fp & 32)	/* repeat flag */
			v += (short)loads(coord), coord += 2;
		pp++->y = top - v;
	}
	return inflate(segs, points, flags, np, ends) - segs;
}

void
t3_uncache(T3_face *face, int lo, int hi) {
	T3_seg	**p = face->seg + max(0, lo);
	T3_seg	**end = face->seg + min(face->nglyph + 1, hi);
	for ( ; p < end; p++)
		if (*p)
			free(*p);
}

void
t3_cache(T3_face *face, int lo, int hi) {
	T3_seg	segs[MAXSEG], *sp;
	int	n, g;
	
	hi = min(face->nglyph + 1, hi);
	for (g = max(0, lo); g < hi; g++) {
		n = t3_getShape(face, segs, g);
		qsort(segs, n, sizeof *segs, cmp_seg);
		for (sp = segs; sp < segs+n; sp++) {
			sp->a.x *= face->scale.x;
			sp->b.x *= face->scale.x;
			sp->a.y *= face->scale.y;
			sp->b.y *= face->scale.y;
		}
		face->seg[g] = memcpy(
			malloc(n * sizeof(T3_seg)),
			segs, n * sizeof(T3_seg));
		face->nseg[g] = n;
	}
}

t3_drawGlyph(T3_face *face, T3_bm bm, T3_pt at, int g, T3_col col) {
	if (g < 0 || g >= face->nglyph)
		return 0;
	if (at.x < -t3_getWidth(face,g) || at.x >= bm.x)
		return 0;
	if (at.y < -(face->ascender - face->descender) || at.y >= bm.y)
		return 0;

	if (!face->seg[g])
		t3_cache(face,g,g+1);
	rasterize(bm, at, t3_getHeight(face) + 1,
		face->seg[g], face->seg[g] + face->nseg[g], col);
	return 1;
}

t3_drawChar(T3_face *face, T3_bm bm, T3_pt at, int c, T3_col col) {
	if (c < 0 && 65536 <= c)
		return 0;
	return t3_drawGlyph(face, bm, at, t3_getGlyph(face, c), col);
}

t3_drawString(T3_face *face, T3_bm bm, T3_pt at, char *s, int n, T3_col col) {
	for ( ; n && *s; n--, s++) {
		int	g = t3_getGlyph(face, *s++);
		t3_drawGlyph(face, bm, at, g, col);
		at.x += t3_getWidth(face, g);
	}
	return 1;
}

t3_drawUnicode(T3_face *face, T3_bm bm, T3_pt at, wchar_t *s, int n, T3_col col) {
	for ( ; n && *s; n--, s++) {
		int	g = t3_getGlyph(face, *s);
		t3_drawGlyph(face, bm, at, g, col);
		at.x += t3_getWidth(face, g);
	}
	return 1;
}

t3_fitString(T3_face *face, float width, char *s, int n, float *ptotal) {
	float	w, vx = 0;
	int	g, count = 0;
	for ( ; n && *s; n--, count++, vx += w) {
		g = t3_getGlyph(face, *s++);
		w = t3_getWidth(face, g);
		if (vx + w >= width)
			break;
	}
	if (ptotal)
		*ptotal = vx;
	return count;
}

t3_fitUnicode(T3_face *face, float width, wchar_t *s, int n, float *ptotal) {
	float	w, vx = 0;
	int	g, count = 0;
	for ( ; n && *s; n--, count++, vx += w) {
		g = t3_getGlyph(face, *s++);
		w = t3_getWidth(face, g);
		if (vx + w >= width)
			break;
	}
	if (ptotal)
		*ptotal = vx;
	return count;
}

t3_hitString(T3_face *face, float x, char *s, int n) {
	float	w, vx = 0;
	int	g, count = 0;
	for ( ; n && *s; n--, count++, vx += w) {
		g = t3_getGlyph(face, *s++);
		w = t3_getWidth(face, g);
		if (vx + w >= x)
			return count;
	}
	return count + 1;
}

t3_hitUnicode(T3_face *face, float x, char *s, int n) {
	float	w, vx = 0;
	int	g, count = 0;
	for ( ; n && *s; n--, count++, vx += w) {
		g = t3_getGlyph(face, *s++);
		w = t3_getWidth(face, g);
		if (vx + w >= x)
			return count;
	}
	return count + 1;
}