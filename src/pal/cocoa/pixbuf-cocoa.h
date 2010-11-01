/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#ifndef MOON_PIXBUF_COCOA_H
#define MOON_PIXBUF_COCOA_H

#include "pal.h"

namespace Moonlight {

class MoonPixbufCocoa : public MoonPixbuf {
public:
	MoonPixbufCocoa (void *pixbuf, bool crc_error);
	virtual ~MoonPixbufCocoa ();

	virtual gint GetWidth ();
	virtual gint GetHeight ();
	virtual gint GetRowStride ();
	virtual gint GetNumChannels ();
	virtual guchar *GetPixels ();

	virtual gpointer GetPlatformPixbuf ();

private:
	bool crc_error;
	void *pixbuf;
};

class MoonPixbufLoaderCocoa : public MoonPixbufLoader {
public:
	MoonPixbufLoaderCocoa (const char *imageType);
	MoonPixbufLoaderCocoa ();
	virtual ~MoonPixbufLoaderCocoa ();

	virtual void Write (const guchar *buffer, int buflen, MoonError **error);
	virtual void Close (MoonError **error);
	virtual MoonPixbuf *GetPixbuf ();

private:
	bool crc_error;
};

};
#endif /* MOON_PIXBUF_COCOA_H */
