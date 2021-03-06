/*
 * CompositionTarget.cs.
 *
 * Contact:
 *   Moonlight List (moonlight-list@lists.ximian.com)
 *
 * Copyright 2008 Novell, Inc. (http://www.novell.com)
 *
 * See the LICENSE file included with the distribution for details.
 * 
 */

using System;
using System.Runtime.InteropServices;
using Mono;

namespace System.Windows.Media
{
	public static class CompositionTarget
	{
		private static EventHandlerList EventList = new EventHandlerList ();

		public static event EventHandler Rendering {
			add {
				RegisterEvent (EventIds.TimeManager_RenderEvent, value, Events.CreateRenderingEventHandlerDispatcher (value));
			}
			remove {
				UnregisterEvent (EventIds.TimeManager_RenderEvent, value);
			}
		}

		private static void RegisterEvent (int eventId, Delegate managedHandler, UnmanagedEventHandler nativeHandler)
		{
			if (managedHandler == null)
				return;

			int token = -1;
			GDestroyNotify dtor_action = (data) => {
				EventList.RemoveHandler (eventId, token);
			};

			IntPtr t = NativeMethods.surface_get_time_manager (Deployment.Current.Surface.Native);
			token = Events.AddHandler (t, eventId, nativeHandler, dtor_action);
			EventList.AddHandler (eventId, token, managedHandler, nativeHandler, dtor_action);
		}


		private static void UnregisterEvent (int eventId, Delegate managedHandler)
		{
			UnmanagedEventHandler nativeHandler = EventList.LookupHandler (eventId, managedHandler);

			if (nativeHandler == null)
				return;

			IntPtr t = NativeMethods.surface_get_time_manager (Deployment.Current.Surface.Native);
			Events.RemoveHandler (t, eventId, nativeHandler);
		}

	}
}
