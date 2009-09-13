#include "linebreak.h"

extern propval_t *gcstring_rules[];
extern size_t gcstring_rulessiz;
extern linebreak_t *linebreak_new();
extern void linebreak_charprop(linebreak_t *, unichar_t,
			       propval_t *, propval_t *, propval_t *,
			       propval_t *);
extern linebreak_t *linebreak_copy(linebreak_t *);
extern void linebreak_destroy(linebreak_t *);

static
propval_t _gbrule(propval_t b_idx, propval_t a_idx)
{
    propval_t result = PROP_UNKNOWN;

    if (b_idx < 0 || gcstring_rulessiz <= b_idx ||
	a_idx < 0 || gcstring_rulessiz <= a_idx)
	;
    else
	result = gcstring_rules[b_idx][a_idx];
    if (result == PROP_UNKNOWN)
	return DIRECT;
    return result;
}

#define eaw2col(e) (((e) == EA_F || (e) == EA_W)? 2: (((e) == EA_Z)? 0: 1))

static
void _gcinfo(linebreak_t *obj, unistr_t *str, size_t pos,
	     size_t *glenptr, size_t *gcolptr, propval_t *glbcptr)
{
    propval_t glbc = PROP_UNKNOWN, ggbc, gscr;
    size_t glen, gcol;
    propval_t lbc, eaw, gbc, ngbc, scr;

    if (!str || !str->str || !str->len) {
	if (glbcptr) *glbcptr = PROP_UNKNOWN;
	if (glenptr) *glenptr = 0;
	if (gcolptr) *gcolptr = 0;
	return;
    }

    linebreak_charprop(obj, str->str[pos], &lbc, &eaw, &gbc, &scr);
    pos++;
    glen = 1;
    gcol = eaw2col(eaw);

    glbc = lbc;
    ggbc = gbc;
    gscr = scr;

    if (lbc == LB_BK || lbc == LB_NL || gbc == GB_LF) {
	;
    } else if (gbc == GB_CR) {
	if (pos < str->len) {
	    linebreak_charprop(obj, str->str[pos], NULL, NULL, &gbc, NULL);
	    if (gbc == GB_LF) {
		pos++;
		glen++;
	    }
	}
    } else if (lbc == LB_SP || lbc == LB_ZW || lbc == LB_WJ) {
	while (1) {
	    if (str->len <= pos)
		break;
	    linebreak_charprop(obj, str->str[pos], &lbc, &eaw, NULL, NULL);
 	    if (lbc != glbc)
		break;
	    pos++;
	    glen++;
	    gcol += eaw2col(eaw);
        }
    }
    else {
	size_t pcol = 0, ecol = 0;
	while (1) {
	    if (str->len <= pos)
		break;
	    linebreak_charprop(obj, str->str[pos], &lbc, &eaw, &ngbc, &scr);
	    if (_gbrule(gbc, ngbc) != DIRECT) {
		pos++;
		glen++;

		if (gbc == GB_Prepend) {
		    glbc = lbc;
		    ggbc = ngbc;
		    gscr = scr;

		    pcol += gcol;
		    gcol = eaw2col(eaw);
		}
		/*
		 * Assume hangul syllable block is always wide, while most of
		 * isolated junseong (V) and jongseong (T) are narrow.
		 */
		else if ((ngbc == GB_L || ngbc == GB_V || ngbc == GB_T ||
			   ngbc == GB_LV || ngbc == GB_LVT) &&
			   (gbc == GB_L || gbc == GB_V || gbc == GB_T ||
			    gbc == GB_LV || gbc == GB_LVT))
		    gcol = 2;
		/*
		 * Some morbid sequences such as <L Extend V T> are allowed.
		 */
		else if (ngbc == GB_Extend || ngbc == GB_SpacingMark) {
		    ecol += eaw2col(eaw);
		    continue;
		}
		else
		    gcol += eaw2col(eaw);

		gbc = ngbc;
	    } else
		break;
	}
	gcol += pcol + ecol;
    }

    if (glbc == LB_SA) {
#ifdef USE_LIBTHAI
	if (gscr != SC_Thai)
#endif
	    glbc = (ggbc == GB_Extend || ggbc == GB_SpacingMark)? LB_CM: LB_AL;
    }
    if (glenptr) *glenptr = glen;
    if (gcolptr) *gcolptr = gcol;
    if (glbcptr) *glbcptr = glbc;
}

/*
 * Exports
 */

/*
 *
 */
gcstring_t *gcstring_new(unistr_t *unistr, linebreak_t *lbobj)
{
    gcstring_t *gcstr;
    size_t pos, len;
    size_t glen, gcol;
    propval_t glbc;
    gcchar_t gc = {0, 0, 0, PROP_UNKNOWN, 0};

    if ((gcstr = malloc(sizeof(gcstring_t))) == NULL)
	return NULL;
    memset(gcstr, 0, sizeof(gcstring_t));

    if (unistr == NULL || unistr->str == NULL || unistr->len == 0)
	return gcstr;
    gcstr->str = unistr->str;
    gcstr->len = len = unistr->len;
    if (lbobj == NULL)
	gcstr->lbobj = linebreak_new();
    else
	gcstr->lbobj = linebreak_copy(lbobj);

    for (pos = 0; pos < len; pos += glen) {
	if ((gcstr->gcstr =
	     realloc(gcstr->gcstr, sizeof(gcchar_t) * (pos + 1))) == NULL)
	    return NULL;
	_gcinfo(gcstr->lbobj, unistr, pos, &glen, &gcol, &glbc);
	gc.idx = pos;
	gc.len = glen;
	gc.col = gcol;
	gc.lbc = glbc;
	memcpy(gcstr->gcstr + gcstr->gclen, &gc, sizeof(gcchar_t));
	gcstr->gclen++;
    }

    return gcstr;
}

gcstring_t *gcstring_copy(gcstring_t *obj)
{
    gcstring_t *newobj;
    unichar_t *newstr;
    gcchar_t *newgcstr;

    if ((newobj = malloc(sizeof(gcstring_t))) == NULL)
	return NULL;
    memcpy(newobj, obj, sizeof(gcstring_t));

    if (obj->str && obj->len) {
	if ((newstr = malloc(sizeof(unichar_t) * obj->len)) == NULL) {
	    free(newobj);
	    return NULL;
	}
	memcpy(newstr, obj->str, sizeof(unichar_t) * obj->len);
	newobj->str = newstr;
    }
    if (obj->gcstr && obj->gclen) {
	if ((newgcstr = malloc(sizeof(gcchar_t) * obj->gclen)) == NULL) {
	    if (newobj->str) free(newobj->str);
	    free(newobj);
	    return NULL;
	}
	memcpy(newgcstr, obj->gcstr, sizeof(gcchar_t) * obj->gclen);
	newobj->gcstr = newgcstr;
    }
    if (obj->lbobj != NULL)
	newobj->lbobj = linebreak_copy(obj->lbobj);
    else
	newobj->lbobj = linebreak_new();

    return newobj;
}

void gcstring_destroy(gcstring_t *gcstr)
{
    if (gcstr == NULL)
	return;
    if (gcstr->str) free(gcstr->str);
    if (gcstr->gcstr) free(gcstr->gcstr);
    if (gcstr->lbobj) linebreak_destroy(gcstr->lbobj);
    free(gcstr);
}

gcstring_t *gcstring_append(gcstring_t *gcstr, gcstring_t *appe)
{
    unistr_t ustr = {0, 0};

    if (gcstr == NULL)
	return NULL;
    if (appe == NULL || appe->str == NULL || appe->len == 0)
	return gcstr;
    if (gcstr->gclen && appe->gclen) {
	size_t aidx, alen, blen, newlen, newgclen, i;
	unsigned char bflag;
	gcstring_t *cstr;

	aidx = gcstr->gcstr[gcstr->gclen - 1].idx;
	alen = gcstr->gcstr[gcstr->gclen - 1].len;
	blen = appe->gcstr[0].len;
	bflag = appe->gcstr[0].flag;

	if ((ustr.str = malloc(sizeof(unichar_t) * (alen + blen))) == NULL)
	    return NULL;
	memcpy(ustr.str, gcstr->str + aidx, sizeof(unichar_t) * alen);
	memcpy(ustr.str + alen, appe->str, sizeof(unichar_t) * blen);
	ustr.len = alen + blen;
	cstr = gcstring_new(&ustr, gcstr->lbobj);

	newlen = gcstr->len + appe->len;
	newgclen = gcstr->gclen - 1 + cstr->gclen + appe->gclen - 1;
	if ((gcstr->str = realloc(gcstr->str,
				  sizeof(unichar_t) * newlen)) == NULL ||
	    (gcstr->gcstr = realloc(gcstr->gcstr,
				    sizeof(gcchar_t) * newgclen)) == NULL) {
	    gcstring_destroy(cstr);
	    return NULL;
	}
	memcpy(gcstr->str + gcstr->len, appe->str,
	       sizeof(unichar_t) * appe->len);
	for (i = 0; i < cstr->gclen; i++) {
	    gcchar_t *gc = gcstr->gcstr + gcstr->gclen - 1 + i;

	    gc->idx = cstr->gcstr[i].idx + aidx;
	    gc->len = cstr->gcstr[i].len;
	    gc->col = cstr->gcstr[i].col;
	    gc->lbc = cstr->gcstr[i].lbc;
	    if (aidx + alen == gc->idx) /* Restore flag if possible */
		gc->flag = bflag;
	}
	for (i = 1; i < appe->gclen; i++) {
	    gcchar_t *gc =
		gcstr->gcstr + gcstr->gclen - 1 + cstr->gclen + i - 1;
	    gc->idx = appe->gcstr[i].idx - blen + aidx + cstr->len;
	    gc->len = appe->gcstr[i].len;
	    gc->col = appe->gcstr[i].col;
	    gc->lbc = appe->gcstr[i].lbc;
	    gc->flag = appe->gcstr[i].flag;
	}

	gcstr->len = newlen;
	gcstr->gclen = newgclen;
	gcstring_destroy(cstr);
    } else if (appe->gclen) {
	if ((gcstr->str = malloc(sizeof(unichar_t) * appe->len)) == NULL)
	    return NULL;
	if ((gcstr->gcstr = malloc(sizeof(gcchar_t) * appe->gclen)) == NULL) {
	    free(gcstr->str);
	    return NULL;
	}
	memcpy(gcstr->str, appe->str, sizeof(unichar_t) * appe->len);
	gcstr->len = appe->len;
	memcpy(gcstr->gcstr, appe->gcstr, sizeof(gcchar_t) * appe->gclen);
	gcstr->gclen = appe->gclen;
    }

    return gcstr;
}

size_t gcstring_columns(gcstring_t *gcstr)
{
    size_t col, i;

    if (gcstr == NULL)
	return 0;
    for (col = 0, i = 0; i < gcstr->gclen; i++)
	col += gcstr->gcstr[i].col;
    return col;
}

gcstring_t *gcstring_concat(gcstring_t *gcstr, gcstring_t *appe)
{
    gcstring_t *new;

    if (gcstr == NULL)
	return NULL;
    if ((new = gcstring_copy(gcstr)) == NULL)
	return NULL;
    return gcstring_append(new, appe);
}
