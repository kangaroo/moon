/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * media.h:
 *
 * Contact:
 *   Moonlight List (moonlight-list@lists.ximian.com)
 *
 * Copyright 2007 Novell, Inc. (http://www.novell.com)
 *
 * See the LICENSE file included with the distribution for details.
 */

#ifndef __MEDIA_H__
#define __MEDIA_H__

#include <glib.h>

#include "value.h"
#include "brush.h"
#include "frameworkelement.h"
#include "error.h"
#include "downloader.h"
#include "bitmapsource.h"

namespace Moonlight {

/* @Namespace=System.Windows.Media */
class MediaAttribute : public DependencyObject {
 protected:
	virtual ~MediaAttribute () {}

 public:
 	/* @PropertyType=string,GenerateAccessors */
	const static int ValueProperty;
	
 	/* @GenerateCBinding,GeneratePInvoke */
	MediaAttribute () { SetObjectType (Type::MEDIAATTRIBUTE); }
	
	//
	// Property Accessors
	//
	const char *GetValue();
	void SetValue (const char *value);
};


/* @Namespace=System.Windows.Media */
class MediaAttributeCollection : public DependencyObjectCollection {
 protected:
	virtual ~MediaAttributeCollection () {}

 public:
 	/* @GenerateCBinding,GeneratePInvoke */
	MediaAttributeCollection () { SetObjectType (Type::MEDIAATTRIBUTE_COLLECTION); }
	
	virtual Type::Kind GetElementType () { return Type::MEDIAATTRIBUTE; }
	
	/* @GenerateCBinding,GeneratePInvoke */
	MediaAttribute *GetItemByName (const char *name);
};


/*
 * This collection is always sorted by the time value of the markers.
 * We override AddToList to add the node where it's supposed to be, keeping the
 * collection sorted at all times.
 * We also override Insert to ignore the index and behave just like Add.
 */
/* @Namespace=System.Windows.Media */
class TimelineMarkerCollection : public DependencyObjectCollection {
 protected:
	virtual ~TimelineMarkerCollection () {}
	
 public:
 	/* @GenerateCBinding,GeneratePInvoke */
	TimelineMarkerCollection () { SetObjectType (Type::TIMELINEMARKER_COLLECTION); }
	
	virtual Type::Kind GetElementType () { return Type::TIMELINEMARKER; }
	
	virtual int AddWithError (Value *value, MoonError *error);
	virtual bool InsertWithError (int index, Value *value, MoonError *error);
};

/* @Namespace=None */
/* @ManagedDependencyProperties=None */
/* @ManagedEvents=Manual */
class MediaBase : public FrameworkElement {
 private:
	static void set_source_async (EventObject *user_data);
	void SetSourceAsyncCallback ();

 protected:
	struct {
		Downloader *downloader;
		char *part_name;
		bool queued;
	} source;
	
	Downloader *downloader;
	char *part_name;
	
	int allow_downloads:1;
	int source_changed:1;
	
	virtual ~MediaBase ();

	void DownloaderAbort ();
	
	virtual void OnEmptySource () { }
	
	void SetDownloadProgress (double progress);
	
	virtual DownloaderAccessPolicy GetDownloaderPolicy (const char *uri) { return MediaPolicy; }

 public:
 	/* @PropertyType=string,AlwaysChange,GenerateAccessors */
	const static int SourceProperty;
 	/* @PropertyType=double,DefaultValue=0.0,GenerateAccessors */
	const static int DownloadProgressProperty;
	
	/* @GenerateCBinding,GeneratePInvoke */
	MediaBase ();

	virtual void SetSourceInternal (Downloader *downloader, char *PartName);
	virtual void SetSource (Downloader *downloader, const char *PartName);
	/* @GenerateCBinding,GeneratePInvoke */
	void SetSource (const char *uri);
	
	void SetAllowDownloads (bool allow);
	bool AllowDownloads () { return allow_downloads; }
	
	virtual void OnPropertyChanged (PropertyChangedEventArgs *args, MoonError *error);
	
	virtual void OnIsAttachedChanged (bool attached);
	
	//
	// Property Accessors
	//
	double GetDownloadProgress ();
	
	const char *GetSource ();
	
	//
	// Events
	//
	const static int DownloadProgressChangedEvent;
};


/* @Namespace=System.Windows.Controls */
class Image : public MediaBase {
 private:
	void DownloadProgress ();
	void ImageOpened (RoutedEventArgs *args);
	void ImageFailed (ImageErrorEventArgs *args);
	void SourcePixelDataChanged ();

	static void download_progress (EventObject *sender, EventArgs *calldata, gpointer closure);
	static void image_opened (EventObject *sender, EventArgs *calldata, gpointer closure);
	static void image_failed (EventObject *sender, EventArgs *calldata, gpointer closure);
	static void source_pixel_data_changed (EventObject *sender, EventArgs *calldata, gpointer closure);

 protected:
	virtual ~Image () {}
	
 public:
 	/* @PropertyType=ImageSource,AutoCreator=Image::CreateDefaultImageSource,GenerateAccessors */
	const static int SourceProperty;
	/* @PropertyType=Stretch,DefaultValue=StretchUniform,GenerateAccessors */
	const static int StretchProperty;
	
 	/* @GenerateCBinding,GeneratePInvoke */
	Image ();

	static Value *CreateDefaultImageSource (Type::Kind kind, DependencyProperty *property, DependencyObject *forObj);
	
	virtual void Dispose ();
	
	virtual void Render (cairo_t *cr, Region *region, bool path_only = false);
	
	virtual void SetSourceInternal (Downloader *downloader, char *PartName);
	virtual void SetSource (Downloader *downloader, const char *PartName);
	
	virtual void OnPropertyChanged (PropertyChangedEventArgs *args, MoonError *error);
	virtual void OnSubPropertyChanged (DependencyProperty *prop, DependencyObject *obj, PropertyChangedEventArgs *subobj_args);

	int GetImageHeight () { return GetSource () ? GetSource ()->GetPixelHeight () : 0; };
	int GetImageWidth  () { return GetSource () ? GetSource ()->GetPixelWidth () : 0; };
	
	virtual Rect GetCoverageBounds ();

	virtual Size MeasureOverrideWithError (Size availableSize, MoonError *error);
	virtual Size ArrangeOverrideWithError (Size finalSize, MoonError *error);
	virtual Size ComputeActualSize ();
	virtual bool CanFindElement () { return true; }

	virtual bool InsideObject (cairo_t *cr, double x, double y);
	
	//
	// Property Accessors
	//
	/* @GenerateCBinding,GeneratePInvoke */
	void SetSource (ImageSource *source);
	ImageSource *GetSource ();

	void SetStretch (Stretch stretch);
	Stretch GetStretch ();
	
	//
	//Events
	//
	/* @DelegateType=EventHandler<ExceptionRoutedEventArgs> */
	const static int ImageFailedEvent;
	/* @DelegateType=EventHandler<RoutedEventArgs> */
	const static int ImageOpenedEvent;
};

};
#endif /* __MEDIA_H__ */
