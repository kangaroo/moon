/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <config.h>
#include <string.h>
#include "pal-cocoa.h"

#include "runtime.h"

#include "pixbuf-cocoa.h"

using namespace Moonlight;

MoonPixbufLoaderCocoa::MoonPixbufLoaderCocoa (const char *imageType)
{
	g_assert_not_reached ();
}

MoonPixbufLoaderCocoa::MoonPixbufLoaderCocoa ()
{
	g_assert_not_reached ();
}

MoonPixbufLoaderCocoa::~MoonPixbufLoaderCocoa ()
{
	g_assert_not_reached ();
}

void
MoonPixbufLoaderCocoa::Write (const guchar *buffer, int buflen, MoonError **error)
{
	g_assert_not_reached ();
}

void
MoonPixbufLoaderCocoa::Close (MoonError **error)
{
	g_assert_not_reached ();
}

MoonPixbuf*
MoonPixbufLoaderCocoa::GetPixbuf ()
{
	g_assert_not_reached ();
}


MoonPixbufCocoa::MoonPixbufCocoa (void *pixbuf, bool crc_error)
{
	g_assert_not_reached ();
}

MoonPixbufCocoa::~MoonPixbufCocoa ()
{
	g_assert_not_reached ();
}

gint
MoonPixbufCocoa::GetWidth ()
{
	g_assert_not_reached ();
}

gint
MoonPixbufCocoa::GetHeight ()
{
	g_assert_not_reached ();
}

gint
MoonPixbufCocoa::GetRowStride ()
{
	g_assert_not_reached ();
}

gint
MoonPixbufCocoa::GetNumChannels ()
{
	g_assert_not_reached ();
}

guchar*
MoonPixbufCocoa::GetPixels ()
{
	g_assert_not_reached ();
}

gpointer
MoonPixbufCocoa::GetPlatformPixbuf ()
{
	g_assert_not_reached ();
}
