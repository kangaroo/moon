/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * surface-gallium.cpp
 *
 * Copyright 2010 Novell, Inc. (http://www.novell.com)
 *
 * See the LICENSE file included with the distribution for details.
 *
 */

#include <config.h>

#define __MOON_GALLIUM__

#include "surface-gallium.h"

#ifdef CLAMP
#undef CLAMP
#endif
#include "util/u_inlines.h"
#include "util/u_sampler.h"

namespace Moonlight {

GalliumSurface::Transfer::Transfer (pipe_context  *context,
				    pipe_resource *texture)
{
	resource = NULL;
	pipe     = context;

	transfer = pipe_get_transfer (pipe,
				      texture,
				      0,
				      0,
				      0,
				      PIPE_TRANSFER_READ_WRITE,
				      0, 0,
				      texture->width0,
				      texture->height0);

	pipe_resource_reference (&resource, texture);
}

GalliumSurface::Transfer::~Transfer ()
{
	pipe->transfer_destroy (pipe, transfer);
	pipe_resource_reference (&resource, NULL);
}

void *
GalliumSurface::Transfer::Map ()
{
	return pipe->transfer_map (pipe, transfer);
}

void
GalliumSurface::Transfer::Unmap ()
{
	return pipe->transfer_unmap (pipe, transfer);
}

GalliumSurface::GalliumSurface (pipe_context *context,
				int          width,
				int          height)
{
	struct pipe_screen       *screen = context->screen;
	struct pipe_resource     pt, *texture;
	struct pipe_sampler_view view_templ;

	pipe   = context;
	mapped = NULL;

	memset (&pt, 0, sizeof (pt));
	pt.target = PIPE_TEXTURE_2D;
	pt.format = PIPE_FORMAT_B8G8R8A8_UNORM;
	pt.width0 = width;
	pt.height0 = height;
	pt.depth0 = 1;
	pt.last_level = 0;
	pt.bind = PIPE_BIND_SAMPLER_VIEW | PIPE_BIND_RENDER_TARGET |
		PIPE_BIND_TRANSFER_WRITE | PIPE_BIND_TRANSFER_READ;

	g_assert (screen->is_format_supported (screen,
					       pt.format,
					       pt.target,
					       0,
					       pt.bind,
					       0));

	texture = screen->resource_create (screen, &pt);

	g_assert (texture);

	u_sampler_view_default_template (&view_templ, texture, texture->format);
	sampler_view = pipe->create_sampler_view (pipe, texture, &view_templ);

	pipe_resource_reference (&texture, NULL);
}

GalliumSurface::~GalliumSurface ()
{
	if (mapped)
		cairo_surface_destroy (mapped);

	pipe_sampler_view_reference (&sampler_view, NULL);
}

void
GalliumSurface::CairoDestroy (void *data)
{
	Transfer *transfer = (Transfer *) data;

	transfer->Unmap ();
	delete transfer;
}

cairo_surface_t *
GalliumSurface::Cairo ()
{
	static cairo_user_data_key_t key;
	struct pipe_resource         *texture = sampler_view->texture;
	Transfer                     *transfer;
	unsigned char                *data;

	if (!pipe) {
		g_warning ("GalliumSurface::Cairo called with invalid "
			   "surface instance.");
		return NULL;
	}

	if (mapped)
		return cairo_surface_reference (mapped);

	transfer = new Transfer (pipe, texture);
	data     = (unsigned char *) transfer->Map ();

	mapped = cairo_image_surface_create_for_data (data,
						      CAIRO_FORMAT_ARGB32,
						      texture->width0,
						      texture->height0,
						      texture->width0 * 4);

	cairo_surface_set_user_data (mapped,
				     &key,
				     (void *) transfer,
				     CairoDestroy);

	return cairo_surface_reference (mapped);
}

void
GalliumSurface::Sync ()
{
	if (mapped) {
		cairo_surface_destroy (mapped);
		mapped = NULL;
	}
}

struct pipe_sampler_view *
GalliumSurface::SamplerView ()
{
	Sync ();

	return sampler_view;
}

struct pipe_resource *
GalliumSurface::Texture ()
{
	struct pipe_resource *texture = sampler_view->texture;

	Sync ();

	return texture;
}

};
