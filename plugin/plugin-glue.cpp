/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * plugin-glue.cpp: MoonLight browser plugin.
 *
 * Contact:
 *   Moonlight List (moonlight-list@lists.ximian.com)
 *
 * Copyright 2007 Novell, Inc. (http://www.novell.com)
 *
 * See the LICENSE file included with the distribution for details.
 *
 */

#include <config.h>

#include "moonlight.h"
#include "plugin.h"
#include "plugin-class.h"
#include "plugin-downloader.h"

using namespace Moonlight;

/* the number of plugin instances running in the browser */
static int browser_plugins = 0;

NPError
MOON_NPP_New (NPMIMEType pluginType, NPP instance, uint16_t mode, int16_t argc, char *argn[], char *argv[], NPSavedData *saved)
{
	if (!instance)
		return NPERR_INVALID_INSTANCE_ERROR;

	NPError err = NPERR_NO_ERROR;
	NPNToolkitType toolkit = (NPNToolkitType) 0;

	// GTK+ ?
	err = MOON_NPN_GetValue (instance,
				 NPNVToolkit,
				 (void *) &toolkit);
	
	if (err != NPERR_NO_ERROR
#if PAL_GTK_WINDOWING
	    || toolkit != NPNVGtk2
#elif PAL_COCOA_WINDOWING
// What does NPN_GetValue give on osx?
#else
#error "no PAL windowing system"
#endif
	    ) {
		g_warning ("It appears your browser does not support the compiled in PAL toolkit: "
#if PAL_GTK_WINDOWING
			   "GTK2"
#elif PAL_COCOA_WINDOWING
			   "Cococa"
#else
#error "no PAL backend windowing system"
#endif
			   );
	}

	PluginInstance *plugin = new PluginInstance (instance, mode);
	if (plugin == NULL)
		return NPERR_OUT_OF_MEMORY_ERROR;

	browser_plugins++;

	plugin->Initialize (argc, argn, argv);
	instance->pdata = plugin;

	return NPERR_NO_ERROR;
}

NPError
MOON_NPP_Destroy (NPP instance, NPSavedData **save)
{
	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	PluginInstance *plugin = (PluginInstance *) instance->pdata;
	Deployment::SetCurrent (plugin->GetDeployment ());

	plugin->Shutdown ();
	instance->pdata = NULL;
	plugin->unref ();

	browser_plugins--;

	return NPERR_NO_ERROR;
}

NPError
MOON_NPP_SetWindow (NPP instance, NPWindow *window)
{
	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;
	
	PluginInstance *plugin = (PluginInstance *) instance->pdata;
	
	return plugin->SetWindow (window);
}

NPError
MOON_NPP_NewStream (NPP instance, NPMIMEType type, NPStream *stream, NPBool seekable, uint16_t *stype)
{
	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;
	
	PluginInstance *plugin = (PluginInstance *) instance->pdata;
	
	return plugin->NewStream (type, stream, seekable, stype);
}

NPError
MOON_NPP_DestroyStream (NPP instance, NPStream *stream, NPError reason)
{
	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;
	
	PluginInstance *plugin = (PluginInstance *) instance->pdata;
	
	return plugin->DestroyStream (stream, reason);
}

void
MOON_NPP_StreamAsFile (NPP instance, NPStream *stream, const char *fname)
{
	if (instance == NULL)
		return;
	
	PluginInstance *plugin = (PluginInstance *) instance->pdata;
	
	plugin->StreamAsFile (stream, fname);
}

int32_t
MOON_NPP_WriteReady (NPP instance, NPStream *stream)
{
	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;
	
	PluginInstance *plugin = (PluginInstance *) instance->pdata;
	
	return plugin->WriteReady (stream);
}

int32_t
MOON_NPP_Write (NPP instance, NPStream *stream, int32_t offset, int32_t len, void *buffer)
{
	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;
	
	PluginInstance *plugin = (PluginInstance *) instance->pdata;
	
	return plugin->Write (stream, offset, len, buffer);
}

void
MOON_NPP_Print (NPP instance, NPPrint *platformPrint)
{
	if (instance == NULL)
		return;
	
	PluginInstance *plugin = (PluginInstance *) instance->pdata;
	
	plugin->Print (platformPrint);
}

void
MOON_NPP_URLNotify (NPP instance, const char *url, NPReason reason, void *notifyData)
{
	if (instance == NULL)
		return;
	
	PluginInstance *plugin = (PluginInstance *) instance->pdata;
	
	plugin->UrlNotify (url, reason, notifyData);
}


int16_t
MOON_NPP_HandleEvent (NPP instance, void *event)
{
	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	PluginInstance *plugin = (PluginInstance *) instance->pdata;
	return plugin->EventHandle (event);
}

NPError
MOON_NPP_GetValue (NPP instance, NPPVariable variable, void *result)
{
	NPError err = NPERR_NO_ERROR;

	switch (variable) {
	case NPPVpluginNeedsXEmbed:
		*((NPBool *)result) = true;
		break;
	case NPPVpluginNameString:
		*((char **)result) = (char *) PLUGIN_NAME;
		break;
	case NPPVpluginDescriptionString:
		*((char **)result) = (char *) PLUGIN_DESCRIPTION;
		break;
	default:
		if (instance == NULL)
			return NPERR_INVALID_INSTANCE_ERROR;
		
		PluginInstance *plugin = (PluginInstance *) instance->pdata;
		err = plugin->GetValue (variable, result);
		break;
	}
	
	return err;
}

NPError
MOON_NPP_SetValue (NPP instance, NPNVariable variable, void *value)
{
	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	PluginInstance *plugin = (PluginInstance *) instance->pdata;
	return plugin->SetValue (variable, value);
}

char *
MOON_NPP_GetMIMEDescription (void)
{
	return (char *) (MIME_TYPES_HANDLED);
}

static bool runtime_initialized = false;

NPError
MOON_NPP_Initialize (void)
{
	// We dont need to initialize mono vm and gtk more than one time.
	if (!g_thread_supported ()) {
		g_warning ("host has not initialized threads");
		//g_thread_init (NULL);
	} 

	if (!runtime_initialized) {
		runtime_initialized = true;
		runtime_init_browser (get_plugin_dir (), false);
	}

	plugin_init_classes ();

	return NPERR_NO_ERROR;
}

static bool
shutdown_moonlight (gpointer data)
{
	/* check if we still should be shutting down */
	if (browser_plugins != 0)
		return false; /* another plugin instance has been created after shutting down the last one earlier */
	
	/* check if all deployments and all plugins have been freed. */
	if (Deployment::GetDeploymentCount () != 0 || PluginInstance::GetPluginCount () != 0) {
		// printf ("shutdown_moonlight (): there are %i deployments and %i plugins left, postponing shutdown a bit.\n", Deployment::GetDeploymentCount (), PluginInstance::GetPluginCount ());
		runtime_get_windowing_system ()->AddTimeout (MOON_PRIORITY_IDLE,
							     100,
							     shutdown_moonlight,
							     NULL);
		return false;
	}
	
	// printf ("shutdown_moonlight (): proceeding with shutdown, there are no more deployments nor plugins left.\n");

	plugin_destroy_classes ();
	runtime_shutdown ();
	runtime_initialized = false;

	return false;
}

void
MOON_NPP_Shutdown (void)
{
	shutdown_moonlight (NULL);
}
