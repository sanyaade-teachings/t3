typedef unsigned T3_col;

typedef struct {
	float	x, y;
} T3_pt;

typedef struct {
	T3_pt	a, b;
	int	dir;
	float	m;
} T3_seg;

typedef struct {
	T3_col	*bm;
	int	x, y;
} T3_bm;

typedef struct {
	unsigned short cmap[65536]; /* map unicode to glyph */
	short	adv[65536];	/* glyph advance width */
	T3_seg	*seg[65536];	/* glyph contour cache */
	int	nseg[65536];	/* segments in contour */
	void	*dat;		/* font file data */
	T3_pt	scale;		/* scale */
	int	longloc;	/* 'loca' table holds longs */
	int	nglyph;		/* number of glyphs */
	int	ascender;	/* ascender */
	int	descender;	/* descender */
	void	*loca;		/* pointer to 'loca' table */
	void	*glyf;		/* 'glyf' table */
} T3_face;

static T3_col
t3_rgb(unsigned char r, unsigned char g, unsigned char b) {
	return (r<<16) + (g<<8) + b;
}

static T3_pt
t3_pt(float x, float y) {
	T3_pt p;
	p.x=x;
	p.y=y;
	return p;
}

static T3_seg
t3_seg(T3_pt a, T3_pt b) {
	T3_seg s;
	int dir=a.y<b.y;
	s.a=dir? a: b;
	s.b=dir? b: a;
	s.m=(a.y==b.y)? 0: (b.x-a.x)/(b.y-a.y);
	s.dir=dir? 1: -1;
	return s;
}

static
t3_getGlyph(T3_face *face, int c) {
	return (c >= 0 && c<65536)? face->cmap[c]: 0;
}

static float
t3_getWidth(T3_face *face, int g) {
	return (g >= 0 && g < face->nglyph)?
		face->adv[g] * face->scale.x:
		0;
}

int	t3_initFace(T3_face *face, void *dat, int index, float height);
int	t3_close(T3_face *face);
void	t3_uncache(T3_face *face, int lo, int hi);
void	t3_cache(T3_face *face, int lo, int hi);

int	t3_getGlyph(T3_face *face, int c);
int	t3_getShape(T3_face *face, T3_seg *segs, int g);
int	t3_drawGlyph(T3_face *face, T3_bm bm, T3_pt at, int g, T3_col col);
int	t3_drawChar(T3_face *face, T3_bm bm, T3_pt at, int c, T3_col col);
int	t3_drawString(T3_face *face, T3_bm bm, T3_pt at, char *s, int n, T3_col col);
int	t3_drawUnicode(T3_face *face, T3_bm bm, T3_pt at, wchar_t *s, int n, T3_col col);
int	t3_fitString(T3_face *face, float width, char *s, int n, float *ptotal);
int	t3_fitUnicode(T3_face *face, float width, wchar_t *s, int n, float *ptotal);
int	t3_hitString(T3_face *face, float x, char *s, int n);
int	t3_hitUnicode(T3_face *face, float x, char *s, int n);