/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * context-cairo.cpp
 *
 * Copyright 2010 Novell, Inc. (http://www.novell.com)
 *
 * See the LICENSE file included with the distribution for details.
 *
 */

#include <config.h>

#include "context-cairo.h"

namespace Moonlight {

CairoContext::CairoContext (CairoSurface *surface)
{
	AbsoluteTransform transform = AbsoluteTransform ();
	Surface           *cs = new Surface (surface, Rect ());

	Stack::Push (new Context::Node (cs, &transform.m, NULL));
	cs->unref ();
}

void
CairoContext::Push (Group extents)
{
	cairo_surface_t *parent = Top ()->GetSurface ()->Cairo ();
	Rect            r = extents.r.RoundOut ();
        cairo_surface_t *data =
          cairo_surface_create_similar (parent,
                                        CAIRO_CONTENT_COLOR_ALPHA,
                                        r.width,
                                        r.height);
        MoonSurface     *surface = new CairoSurface (data);
        Surface         *cs = new Surface (surface, extents.r);
        cairo_matrix_t  matrix;

	Top ()->GetMatrix (&matrix);

	Stack::Push (new Context::Node (cs, &matrix, &extents.r));
	cs->unref ();
	surface->unref ();
	cairo_surface_destroy (data);
}

};
