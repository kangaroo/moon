/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * im-cocoa.cpp
 *
 * Copyright 2010 Novell, Inc. (http://www.novell.com)
 *
 * See the LICENSE file included with the distribution for details.
 *
 */

#include <config.h>
#include "im-cocoa.h"
#include <cairo.h>

using namespace Moonlight;

MoonIMContextCocoa::MoonIMContextCocoa ()
{
	g_assert_not_reached ();
}

MoonIMContextCocoa::~MoonIMContextCocoa ()
{
	g_assert_not_reached ();
}

void
MoonIMContextCocoa::SetUsePreedit (bool flag)
{
	g_assert_not_reached ();
}

void
MoonIMContextCocoa::SetClientWindow (MoonWindow* window)
{
	g_assert_not_reached ();
}

bool
MoonIMContextCocoa::FilterKeyPress (MoonKeyEvent* event)
{
	g_assert_not_reached ();
}

void
MoonIMContextCocoa::SetSurroundingText (const char *text, int offset, int length)
{
	g_assert_not_reached ();
}

void
MoonIMContextCocoa::Reset ()
{
	g_assert_not_reached ();
}


void
MoonIMContextCocoa::FocusIn ()
{
	g_assert_not_reached ();
}

void
MoonIMContextCocoa::FocusOut ()
{
	g_assert_not_reached ();
}

void
MoonIMContextCocoa::SetCursorLocation (Rect r)
{
	g_assert_not_reached ();
}

void
MoonIMContextCocoa::SetRetrieveSurroundingCallback (MoonCallback cb, gpointer data)
{
	g_assert_not_reached ();
}

void
MoonIMContextCocoa::SetDeleteSurroundingCallback (MoonCallback cb, gpointer data)
{
	g_assert_not_reached ();
}

void
MoonIMContextCocoa::SetCommitCallback (MoonCallback cb, gpointer data)
{
	g_assert_not_reached ();
}

gpointer
MoonIMContextCocoa::GetPlatformIMContext ()
{
	g_assert_not_reached ();
}

