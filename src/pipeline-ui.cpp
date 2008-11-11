/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * pipeline-ui.cpp: 
 *
 * Contact:
 *   Moonlight List (moonlight-list@lists.ximian.com)
 *
 * Copyright 2007 Novell, Inc. (http://www.novell.com)
 *
 * See the LICENSE file included with the distribution for details.
 */

#include <config.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "pipeline-ui.h"
#include "downloader.h"
#include "utils.h"
#include "pipeline.h"
#include "debug.h"
#include "codec-url.h"

#define EULA_URL "http://go.microsoft.com/fwlink/?LinkId=000000"

bool CodecDownloader::running = false;

CodecDownloader::CodecDownloader (Surface *surf)
{
	surface = surf;
	eula = NULL;
	state = 0;
	dl = NULL;
	dialog = NULL;
	vbox = NULL;
	header_label = NULL;
	message_label = NULL;
	progress_bar = NULL;
	eula_scrollwindow = NULL;
	eula_buffer = NULL;
	eula_view = NULL;
	accept_button = NULL;
	cancel_button = NULL;
	icon = NULL;
	dont_ask = NULL;
}

CodecDownloader::~CodecDownloader ()
{
	g_free (eula);
	if (dl != NULL) {
		dl->unref ();
	}
	running = false;
}

void
CodecDownloader::ShowUI (Surface *surface)
{
	if (running) {
		return;
	}

	if (!(moonlight_flags & RUNTIME_INIT_ENABLE_MS_CODECS))
		return;

	CodecDownloader *cd = new CodecDownloader (surface);
	cd->Show ();
	cd->unref ();
}

// ----- Event Proxies -----

void
CodecDownloader::ResponseEventHandler (GtkDialog *dialog, gint response, gpointer data)
{
	((CodecDownloader *) data)->ResponseEvent (dialog, (GtkResponseType)response);
}

void
CodecDownloader::DownloadFailedHandler (EventObject *sender, EventArgs *args, gpointer closure)
{
	((CodecDownloader *) closure)->DownloadFailed (sender, args);
}

void
CodecDownloader::DownloadCompletedHandler (EventObject *sender, EventArgs *args, gpointer closure)
{
	((CodecDownloader *) closure)->DownloadCompleted (sender, args);
}

void
CodecDownloader::DownloadProgressChangedHandler (EventObject *sender, EventArgs *args, gpointer closure)
{
	((CodecDownloader *) closure)->DownloadProgressChanged (sender, args);
}

// ----- Event Handlers -----

void
CodecDownloader::ResponseEvent (GtkDialog *dialog, GtkResponseType response)
{
	LOG_UI ("CodecDownloader::ResponseEvent (%d)\n", response);

	switch (response) {
	case GTK_RESPONSE_CANCEL:
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dont_ask))) {
			LOG_UI ("Setting DontInstallMSCodecs\n");
			configuration.SetBooleanValue ("Codecs", "DontInstallMSCodecs", true);
			configuration.Save ();
		}

		state = 5;
		Close ();
		return;
	case GTK_RESPONSE_DELETE_EVENT:
		Close ();
		return;
	case GTK_RESPONSE_OK:
		AcceptClicked ();
		return;
	default:
		return;
	}
}

void
CodecDownloader::DownloadProgressChanged (EventObject *sender, EventArgs *args)
{
	double progress = dl->GetDownloadProgress ();
	LOG_UI ("CodecDownloader::DownloadProgressChanged (): %.2f\n", progress);	
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress_bar), progress);
}

void
CodecDownloader::DownloadFailed (EventObject *sender, EventArgs *args)
{
	ErrorEventArgs *eea = (ErrorEventArgs *) args;
	gchar *msg;
	
	LOG_UI ("CodecDownloader::DownloadFailed ()\n");

	msg = g_strdup_printf ("An error occurred while downloading the %s", state == 1 
		? "End User License Agreement." 
		: "add-on software."); 

	SetHeader ((const gchar *)msg);
	SetMessage (eea->error_message);

	ToggleProgress (false);

	gtk_image_set_from_stock (GTK_IMAGE (icon), GTK_STOCK_DIALOG_ERROR, GTK_ICON_SIZE_DIALOG);
	gtk_button_set_label (GTK_BUTTON (accept_button), GTK_STOCK_CLOSE);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, true);
	gtk_widget_hide (cancel_button);
	
	g_free (msg);
	
	state = 6;
}

void
CodecDownloader::DownloadCompleted (EventObject *sender, EventArgs *args)
{
	guint64 size;

	gchar *downloaded_file = NULL;
	gchar *codec_path = NULL;
	gchar *codec_dir = NULL;
	int codec_fd = 0;
	GtkTextIter iter = { 0 };
	
	LOG_UI ("CodecDownloader::DownloadCompleted ()\n");
	
	ToggleProgress (false);

	switch (state) {
	case 1: // downloading eula, we're now finished downloading the eula
		eula = dl->GetResponseText (NULL, &size);

		SetHeader ("End User License Agreement");
		SetMessage ("Before the required software can be installed, you must first agree "
			"to the End User License Agreement below.");
		ToggleEula (true);

		gtk_button_set_label (GTK_BUTTON (accept_button), "_Accept");
		gtk_text_buffer_set_text (eula_buffer, eula, strlen (eula));
		gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (eula_view), &iter, 0, 0);
		gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (eula_view), &iter, 0, true, 0, 0);

		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, true);

		state = 2;
		break;
	case 3: // downloading codec, we're now finished downloading the codec
		codec_path = g_build_filename (g_get_home_dir (), ".mozilla", "plugins", "moonlight", CODEC_LIBRARY_NAME, NULL);
		codec_dir = g_path_get_dirname (codec_path);

		downloaded_file = dl->GetDownloadedFilename (NULL);

		errno = 0;

		if (g_mkdir_with_parents (codec_dir, 0700) == -1 ||
			(codec_fd = open (codec_path, O_CREAT | O_TRUNC | O_WRONLY, 0700)) == -1 ||
			CopyFileTo (downloaded_file, codec_fd) == -1) {
			SetHeader ("An error occurred when installing the software");
			SetMessage (strerror (errno));
		} else {
			SetHeader ("Software successfully downloaded and installed!");
			SetMessage ("Please refresh the web page you were viewing to allow the new software to take effect.");

			configuration.SetStringValue ("Codecs", "MSCodecsPath", codec_path);
			configuration.Save ();
			Media::RegisterMSCodecs ();
		}
		
		g_free (codec_path);
		g_free (codec_dir);
		g_free (downloaded_file);

		gtk_widget_hide (cancel_button);
		gtk_button_set_label (GTK_BUTTON (accept_button), GTK_STOCK_CLOSE);
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, true);

		state = 4;
		break;
	default:
		printf ("CodecDownloader::DownloadCompleted (): Invalid state: %i\n", state);
		break;
	}
}

void
CodecDownloader::AcceptClicked ()
{
	LOG_UI ("CodecDownloader::AcceptClicked\n");
	
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress_bar), 0.0);
	ToggleProgress (true);

	CreateDownloader ();
	
	switch (state) {
	case 0: // initial, waiting for user input
		SetHeader ("Downloading license agreement...");
		HideMessage ();
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, false);
		
		dl->Open ("GET", EULA_URL, NoPolicy);
		dl->Send ();

		state = 1;
		break;
	case 2: // eula downloaded, waiting for user input
		char *env_url;
		SetHeader ("Downloading the required software...");
		HideMessage ();
		ToggleEula (false);
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, false);

		env_url = getenv ("MOONLIGHT_CODEC_URL");
		if (env_url != NULL)
			dl->Open ("GET", env_url, NoPolicy);
		else {
			char *codec_url = g_strdup_printf("%s/%s", CODEC_URL, CODEC_LIBRARY_NAME);
			dl->Open ("GET", codec_url, NoPolicy);
			g_free (codec_url);
		}
		dl->Send ();

		state = 3;
		break;
	case 4:
	case 6:
		Close ();
		break;
	default:
		printf ("CodecDownloader::AcceptClicked (): Invalid state: %i\n", state);
		break;
	}
}

// ----- Utilities -----

void
CodecDownloader::CreateDownloader ()
{
	if (dl == NULL) {
		dl = surface->CreateDownloader ();
		dl->AddHandler (Downloader::DownloadProgressChangedEvent, DownloadProgressChangedHandler, this);
		dl->AddHandler (Downloader::DownloadFailedEvent, DownloadFailedHandler, this);
		dl->AddHandler (Downloader::CompletedEvent, DownloadCompletedHandler, this);
	}
}

void
CodecDownloader::DestroyDownloader ()
{
	if (dl != NULL) {
		dl->RemoveHandler (Downloader::DownloadProgressChangedEvent, DownloadProgressChangedHandler, this);
		dl->RemoveHandler (Downloader::DownloadFailedEvent, DownloadFailedHandler, this);
		dl->RemoveHandler (Downloader::CompletedEvent, DownloadCompletedHandler, this);
		dl->unref ();
		dl = NULL;
	}
}

void
CodecDownloader::SetHeader (const gchar *message)
{
	gchar *message_full = g_strdup_printf ("<big><b>%s</b></big>", message);
	gtk_label_set_markup (GTK_LABEL (header_label), message_full);
	g_free (message_full);
}

void
CodecDownloader::SetMessage (const gchar *message)
{
	gtk_label_set_text (GTK_LABEL (message_label), message);
	gtk_widget_show (message_label);
}

void
CodecDownloader::HideMessage ()
{
	gtk_widget_hide (message_label);
}

void
CodecDownloader::ToggleEula (bool show)
{
	if (show) {
		gtk_widget_show_all (eula_scrollwindow);
	} else {
		gtk_widget_hide (eula_scrollwindow);
	}
}

void
CodecDownloader::ToggleProgress (bool show)
{
	if (show) {
		gtk_image_set_from_stock (GTK_IMAGE (icon), GTK_STOCK_SAVE, GTK_ICON_SIZE_DIALOG);
		gtk_widget_hide (dont_ask);
		gtk_widget_show_all (progress_bar);
	} else {
		gtk_image_set_from_stock (GTK_IMAGE (icon), GTK_STOCK_DIALOG_QUESTION, GTK_ICON_SIZE_DIALOG);
		gtk_widget_hide (progress_bar);
	}
}

void
CodecDownloader::AdaptToParentWindow ()
{
	// try to find a parent for our window
	// there must be a better way of doing this though :|
	GList *toplevels = gtk_window_list_toplevels ();
	GList *current = toplevels;
	GtkWindow *parent = NULL;

	while (current != NULL) {
		const char *title = gtk_window_get_title (GTK_WINDOW (current->data));
		if (title != NULL && strstr (title, "Mozilla Firefox") != NULL) {
			parent = GTK_WINDOW (current->data);
			break;
		}

		current = current->next;
	}
	g_list_free (toplevels);

	if (parent != NULL) {
		gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
		gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	} else {
		// If no parent could be found, just center in the screen
		gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
	}
}

// ----- Dialog Create/Destroy -----

void
CodecDownloader::Show ()
{
	if (configuration.GetBooleanValue ("Codecs", "DontInstallMSCodecs")) {
		state = 5;
		return;
	}

	if (state != 0) {
		fprintf (stderr, "CodecDownloader::Show (): Can't call Show more than once.\n");
		state = 6;
		return;
	}
	
	gint label_width = 400;

	// Build HIG Dialog Box
	dialog = gtk_dialog_new_with_buttons ("Moonlight Codecs Installer", NULL, (GtkDialogFlags)
		(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR), NULL);
	cancel_button = gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	accept_button = gtk_dialog_add_button (GTK_DIALOG (dialog), "_Install Codecs", GTK_RESPONSE_OK);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	
	AdaptToParentWindow ();
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_object_set (GTK_OBJECT (dialog), "resizable", false, NULL);

	// HIG HBox
	GtkWidget *hbox = gtk_hbox_new (false, 12);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, true, true, 0);

	// Message box icon
	icon = gtk_image_new_from_stock (GTK_STOCK_DIALOG_QUESTION, GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment (GTK_MISC (icon), 0.5f, 0.0f);
	gtk_box_pack_start (GTK_BOX (hbox), icon, false, false, 0);

	// Contents container
	vbox = gtk_vbox_new (false, 0);
	gtk_box_set_spacing (GTK_BOX (vbox), 10);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, true, true, 0);

	// Header Label
	header_label = gtk_label_new (NULL);
	SetHeader ("Would you like to install the required add-on to play the content on this page?");
	gtk_label_set_line_wrap (GTK_LABEL (header_label), true);
	gtk_label_set_justify (GTK_LABEL (header_label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (header_label), 0.0f, 0.5f);
	gtk_widget_set_size_request (header_label, label_width, -1);
	gtk_box_pack_start (GTK_BOX (vbox), header_label, false, false, 0);

	// Secondary Label
	message_label = gtk_label_new (NULL);
	SetMessage ("This page requires the Microsoft Media Pack "
		    "to be installed to play multimedia content.\n\n"
		    "If you choose to install it, the software will be "
		    "automatically downloaded and installed "
		    "from Microsoft's web site.");
	gtk_label_set_line_wrap (GTK_LABEL (message_label), true);
	gtk_label_set_justify (GTK_LABEL (message_label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (message_label), 0.0f, 0.5f);
	gtk_widget_set_size_request (message_label, label_width, -1);
	gtk_box_pack_start (GTK_BOX (vbox), message_label, false, false, 0);

	dont_ask = gtk_check_button_new_with_label ("Do not ask me to install this add-on again");
	gtk_box_pack_start (GTK_BOX (vbox), dont_ask, false, false, 0);

	// Other elements
	progress_bar = gtk_progress_bar_new ();
	gtk_box_pack_start (GTK_BOX (vbox), progress_bar, false, false, 0);
	
	// EULA
	eula_buffer = gtk_text_buffer_new (NULL);
	eula_view = gtk_text_view_new_with_buffer (eula_buffer);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (eula_view), FALSE);

	eula_scrollwindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (eula_scrollwindow), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (eula_scrollwindow), eula_view);
	gtk_widget_set_size_request (eula_scrollwindow, -1, 225);
	gtk_box_pack_end (GTK_BOX (vbox), eula_scrollwindow, false, false, 0);

	// Connect and go
	g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (ResponseEventHandler), this);

	gtk_object_set (GTK_OBJECT (accept_button), "has-focus", true, "has-default", true, NULL);

	gtk_widget_show_all (dialog);
	ToggleProgress (false);
	ToggleEula (false);

	ref (); // We manage our lifetime ourself
	running = true;
}

void
CodecDownloader::Close ()
{
	LOG_UI ("CodecDownloader::Close ()\n");

	if (dl != NULL) {
		dl->Abort ();
		DestroyDownloader ();
	}

	gtk_widget_destroy (dialog);
	unref ();
	running = false;
}

