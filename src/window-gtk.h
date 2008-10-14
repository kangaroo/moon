/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * window-gtk.h: MoonWindow implementation using gtk widgets.
 *
 * Contact:
 *   Moonlight List (moonlight-list@lists.ximian.com)
 *
 * Copyright 2007 Novell, Inc. (http://www.novell.com)
 *
 * See the LICENSE file included with the distribution for details.
 * 
 */

#ifndef __MOON_WINDOW_GTK_H__
#define __MOON_WINDOW_GTK_H__

#include <glib.h>

G_BEGIN_DECLS

#include <gtk/gtk.h>

#include "window.h"
#include "runtime.h"

/* @Namespace=System.Windows */
class MoonWindowGtk : public MoonWindow {
public:
	/* @GenerateCBinding,GeneratePInvoke */
	MoonWindowGtk (bool fullscreen, int w = -1, int h = -1, void* parent = NULL);

	virtual ~MoonWindowGtk ();

	virtual void Resize (int width, int height);
	virtual void SetCursor (GdkCursor *cursor);
	virtual void Invalidate (Rect r);
	virtual void ProcessUpdates ();
	virtual gboolean HandleEvent (XEvent *event);
	virtual void Show ();
	virtual void Hide ();
	virtual void EnableEvents (bool first);
	virtual void DisableEvents ();

	virtual void GrabFocus ();
	virtual bool HasFocus ();

	/* @GenerateCBinding,GeneratePInvoke */
	GtkWidget* GetWidget() { return widget; }

	virtual bool IsFullScreen () { return fullscreen; }

	virtual GdkWindow* GetGdkWindow ();

private:
	GtkWidget *widget;
	bool fullscreen;

	static gboolean expose_event (GtkWidget *widget, GdkEventExpose *event, gpointer user_data);
	static gboolean motion_notify (GtkWidget *widget, GdkEventMotion *event, gpointer user_data);
	static gboolean crossing_notify (GtkWidget *widget, GdkEventCrossing *event, gpointer user_data);
	static gboolean key_press (GtkWidget *widget, GdkEventKey *event, gpointer user_data);
	static gboolean key_release (GtkWidget *widget, GdkEventKey *event, gpointer user_data);
	static gboolean button_release (GtkWidget *widget, GdkEventButton *event, gpointer user_data);
	static gboolean button_press (GtkWidget *widget, GdkEventButton *event, gpointer user_data);
	static gboolean focus_in (GtkWidget *widget, GdkEventFocus *event, gpointer user_data);
	static gboolean focus_out (GtkWidget *widget, GdkEventFocus *event, gpointer user_data);
	static gboolean realized (GtkWidget *widget, gpointer user_data);
	static gboolean unrealized (GtkWidget *widget, gpointer user_data);

	static void widget_size_allocate (GtkWidget *widget, GtkAllocation *allocation, gpointer user_data);
	static void widget_destroyed (GtkWidget *widget, gpointer user_data);

	void InitializeFullScreen (void *parent);
	void InitializeNormal ();
	void InitializeCommon ();
};

G_END_DECLS

#endif /* __MOON_WINDOW_GTK_H__ */
