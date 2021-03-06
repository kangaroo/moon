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

#ifndef __MOON_CONFIG_DIALOG_GTK_H__
#define __MOON_CONFIG_DIALOG_GTK_H__

#include "window-gtk.h"
#include "moonlightconfiguration.h"

namespace Moonlight {

class ConfigDialogPage;

class MoonConfigDialogGtk {
public:
	MoonConfigDialogGtk (MoonWindowGtk *window, Surface *surface, Deployment *deployment);
	~MoonConfigDialogGtk ();

	void Show ();

	MoonWindowGtk *GetWindow () { return window; }
	Surface *GetSurface() { return surface; }
	Deployment *GetDeployment() { return deployment; }

private:
	void AddNotebookPage (const char *label, ConfigDialogPage *page);

	static void notebook_switch_page (GtkNotebook     *notebook,
					  GtkNotebookPage *page,
					  guint            page_num,
					  MoonConfigDialogGtk *dialog);

	ArrayList* pages;

	GtkWidget *dialog, *notebook, *treeview;

	MoonWindowGtk *window;
	Surface *surface;
	Deployment *deployment;
};


class ConfigDialogPage {
public:
	ConfigDialogPage () { }
	virtual ~ConfigDialogPage () { }

	virtual GtkWidget* GetContentWidget () = 0;
	virtual void PageActivated () { }

	void SetDialog (MoonConfigDialogGtk *dialog) { this->dialog = dialog; }
	MoonConfigDialogGtk *GetDialog () { return dialog; }

private:
	MoonConfigDialogGtk *dialog;
};


class AboutConfigDialogPage : public ConfigDialogPage {
public:
	AboutConfigDialogPage ();
	virtual ~AboutConfigDialogPage ();

	virtual GtkWidget* GetContentWidget ();
};

class PlaybackConfigDialogPage : public ConfigDialogPage {
public:
	PlaybackConfigDialogPage ();
	virtual ~PlaybackConfigDialogPage ();

	virtual GtkWidget* GetContentWidget ();

private:
	static void install_media_pack (PlaybackConfigDialogPage *page);

};

class StorageConfigDialogPage : public ConfigDialogPage {
public:
	StorageConfigDialogPage ();
	virtual ~StorageConfigDialogPage ();

	virtual void PageActivated ();
	virtual GtkWidget* GetContentWidget ();

private:

	void PopulateModel ();

	static void selection_changed (StorageConfigDialogPage *page);
	static void delete_selected_storage (StorageConfigDialogPage *page);
	static void delete_all_storage (StorageConfigDialogPage *page);

	GtkListStore *model;
	GtkTreeSelection* selection;
	GtkTreeView *treeview;
	GtkWidget *delete_button;
	GtkWidget *delete_all_button;
};

class PermissionsConfigDialogPage : public ConfigDialogPage {
public:
	PermissionsConfigDialogPage ();
	virtual ~PermissionsConfigDialogPage ();

	virtual void PageActivated ();
	virtual GtkWidget* GetContentWidget ();

private:

	void PopulateModel ();

	static void selection_changed (PermissionsConfigDialogPage *page);
	static void remove_selected_permission (PermissionsConfigDialogPage *page);
	static void allow_selected_permission (PermissionsConfigDialogPage *page);
	static void deny_selected_permission (PermissionsConfigDialogPage *page);

	void UpdateButtonSensitivity ();

	MoonlightConfiguration *configuration;
	GtkTreeStore *model;
	GtkTreeSelection* selection;
	GtkTreeView *treeview;
	GHashTable *website_path_hash;
	GtkWidget *remove_button;
	GtkWidget *allow_button;
	GtkWidget *deny_button;
};

class ApplicationConfigDialogPage : public ConfigDialogPage {
public:
	ApplicationConfigDialogPage ();
	virtual ~ApplicationConfigDialogPage ();

	virtual GtkWidget* GetContentWidget ();
};

class DebugConfigDialogPage : public ConfigDialogPage {
public:
	DebugConfigDialogPage ();
	virtual ~DebugConfigDialogPage ();

	virtual GtkWidget* GetContentWidget ();
};

class AdvancedConfigDialogPage : public ConfigDialogPage {
public:
	AdvancedConfigDialogPage ();
	virtual ~AdvancedConfigDialogPage ();

	virtual GtkWidget* GetContentWidget ();
};

};
#endif /* __MOON_CONFIG_DIALOG_GTK_H__ */

