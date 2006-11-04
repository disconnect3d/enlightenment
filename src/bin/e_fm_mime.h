/*
 * vim:ts=8:sw=3:sts=8:noexpandtab:cino=>5n-3f0^-2{2
 */
#ifdef E_TYPEDEFS

#else
#ifndef E_FM_MIME_H
#define E_FM_MIME_H

EAPI const char *e_fm_mime_filename_get(const char *fname);
EAPI const char *e_fm_mime_icon_get(const char *mime);
EAPI void e_fm_mime_icon_cache_flush(void);
    
#endif
#endif
