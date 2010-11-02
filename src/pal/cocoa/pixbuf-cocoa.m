/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <config.h>
#include <string.h>
#include "pal-cocoa.h"

#include "runtime.h"

#include "pixbuf-cocoa.h"

#import <AppKit/AppKit.h>

using namespace Moonlight;

MoonPixbufLoaderCocoa::MoonPixbufLoaderCocoa (const char *imageType)
{
	this->data = [NSMutableData dataWithCapacity: getpagesize ()];
}

MoonPixbufLoaderCocoa::MoonPixbufLoaderCocoa ()
{
	this->data = [NSMutableData dataWithCapacity: getpagesize ()];
}

MoonPixbufLoaderCocoa::~MoonPixbufLoaderCocoa ()
{
	[this->data release];
}

void
MoonPixbufLoaderCocoa::Write (const guchar *buffer, int buflen, MoonError **error)
{
	[this->data appendBytes: buffer length: buflen];
}

void
MoonPixbufLoaderCocoa::Close (MoonError **error)
{
}

MoonPixbuf*
MoonPixbufLoaderCocoa::GetPixbuf ()
{
	return new MoonPixbufCocoa (this->data, FALSE);
}


MoonPixbufCocoa::MoonPixbufCocoa (void *pixbuf, bool crc_error)
{
	this->image = [[NSBitmapImageRep alloc] initWithData: [(NSData *) pixbuf retain]];
}

MoonPixbufCocoa::~MoonPixbufCocoa ()
{
	[this->image release];
}

gint
MoonPixbufCocoa::GetWidth ()
{
	return ((NSBitmapImageRep *) image).size.width;
}

gint
MoonPixbufCocoa::GetHeight ()
{
	return ((NSBitmapImageRep *) image).size.height;
}

gint
MoonPixbufCocoa::GetRowStride ()
{
	return ((NSBitmapImageRep *) image).bytesPerRow;
}

gint
MoonPixbufCocoa::GetNumChannels ()
{
	return ((NSBitmapImageRep *) image).numberOfPlanes;
}

guchar*
MoonPixbufCocoa::GetPixels ()
{
	return ((NSBitmapImageRep *) image).bitmapData;
}

gpointer
MoonPixbufCocoa::GetPlatformPixbuf ()
{
	g_assert_not_reached ();
}
