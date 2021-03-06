/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * eventargs.h
 *
 * Copyright 2007 Novell, Inc. (http://www.novell.com)
 *
 * See the LICENSE file included with the distribution for details.
 * 
 */

#ifndef __EVENTARGS_H__
#define __EVENTARGS_H__

#include <glib.h>

#include <cairo.h>
#include "dependencyobject.h"
#include "keyboard.h"
#include "enums.h"
#include "pal.h"
#include "error.h"

namespace Moonlight {

class StylusInfo;
class StylusPointCollection;
class UIElement;

/*
 * EventArgs needs to be ref counted since js can keep objects around until
 * after the event has been emitted.
 */

/* @Namespace=None */
class EventArgs : public DependencyObject {
public:
	EventArgs ();

protected:
	virtual ~EventArgs ();
	/* @SkipFactories */
	EventArgs (Type::Kind type);
};

/* @Namespace=None */
class PropertyChangedEventArgs : public EventArgs {
public:
	PropertyChangedEventArgs (DependencyProperty *p, int pid, Value *ov, Value *nv) : obj (p), id (pid), old_value(ov), new_value (nv)
	{
		if (new_value)
			new_value = new Value (*new_value);
		if (old_value)
			old_value = new Value (*old_value);
	}

	PropertyChangedEventArgs () : obj (NULL), id (0), old_value(NULL), new_value (NULL) { }


	~PropertyChangedEventArgs ()
	{
		delete new_value;
		delete old_value;
	}

	/* @GenerateCBinding,GeneratePInvoke */
	DependencyProperty *GetProperty () { return obj; }
	/* @GenerateCBinding,GeneratePInvoke */
	int GetId () { return id; }
	/* @GenerateCBinding,GeneratePInvoke */
	Value* GetOldValue () { return old_value; }
	/* @GenerateCBinding,GeneratePInvoke */
	Value* GetNewValue () { return new_value; }

	void SetProperty (DependencyProperty *prop) { obj = prop; }
	void SetId (int id) { this->id = id; }
	void SetOldValue (Value* old_value) { delete this->old_value; this->old_value = old_value ? new Value (*old_value) : NULL; }
	void SetNewValue (Value* new_value) { delete this->new_value; this->new_value = new_value ? new Value (*new_value) : NULL; }
private:
	DependencyProperty *obj;
	int id;

	Value *old_value;
	Value *new_value;
};

/* @Namespace=None */
class RenderingEventArgs : public EventArgs {
public:
	RenderingEventArgs ();

	/* @GenerateCBinding,GeneratePInvoke */
	TimeSpan GetRenderingTime ();

	void SetRenderingTime (TimeSpan renderingTime);

protected:
	virtual ~RenderingEventArgs ();

private:

	TimeSpan renderingTime;
};

enum CollectionChangedAction {
	CollectionChangedActionAdd,
	CollectionChangedActionRemove,
	CollectionChangedActionReplace,
	CollectionChangedActionClearing,
	CollectionChangedActionCleared,
};

/* @Namespace=None */
class CollectionChangedEventArgs : public EventArgs {
public:
	/* @GenerateCBinding,GeneratePInvoke,SkipTypeConstructor */
	CollectionChangedEventArgs ();

	/* @SkipFactories */
	CollectionChangedEventArgs (CollectionChangedAction action, Value *new_value, Value *old_value, int index);
	
	/* @GenerateCBinding,GeneratePInvoke */
	void SetChangedAction (CollectionChangedAction action);
	
	/* @GenerateCBinding,GeneratePInvoke */
	CollectionChangedAction GetChangedAction ();
	
	/* @GenerateCBinding,GeneratePInvoke */
	void SetNewItem (Value *item);
	
	/* @GenerateCBinding,GeneratePInvoke */
	Value *GetNewItem ();
	
	/* @GenerateCBinding,GeneratePInvoke */
	void SetOldItem (Value *item);
	
	/* @GenerateCBinding,GeneratePInvoke */
	Value *GetOldItem ();
	
	/* @GenerateCBinding,GeneratePInvoke */
	void SetIndex (int index);
	
	/* @GenerateCBinding,GeneratePInvoke */
	int GetIndex ();

protected:
	virtual ~CollectionChangedEventArgs ();


private:
	CollectionChangedAction action;
	Value *old_item;
	Value *new_item;
	int index;
};

/* @Namespace=None */
class CollectionItemChangedEventArgs : public EventArgs {
public:
	CollectionItemChangedEventArgs (DependencyObject *collectionItem,
					DependencyProperty *property,
					Value *oldValue,
					Value *newValue)
		: EventArgs (Type::COLLECTIONITEMCHANGEDEVENTARGS)
	{
		this->collectionItem = collectionItem;
		this->property = property;
		this->oldValue = oldValue;
		this->newValue = newValue;
	}

	CollectionItemChangedEventArgs ()
		: EventArgs (Type::COLLECTIONITEMCHANGEDEVENTARGS)
	{
		this->collectionItem = NULL;
		this->property = NULL;
		this->oldValue = NULL;
		this->newValue = NULL;
	}

	DependencyObject*   GetCollectionItem() { return collectionItem; }
	DependencyProperty* GetProperty()       { return property; }
	Value*              GetOldValue ()      { return oldValue; }
	Value*              GetNewValue ()      { return newValue; }

	void SetCollectionItem(DependencyObject* collectionItem) { this->collectionItem = collectionItem; }
	void SetProperty(DependencyProperty* property)           { this->property = property; }
	void SetOldValue (Value* oldValue)                       { this->oldValue = oldValue; }
	void SetNewValue (Value* newValue)                       { this->newValue = newValue; }

private:
	DependencyObject *collectionItem;
	DependencyProperty *property;
	Value *oldValue;
	Value *newValue;
};

/* @Namespace=None */
class DownloadProgressEventArgs : public EventArgs {
 private:
	double progress;

 protected:
	virtual ~DownloadProgressEventArgs ();
 
 public:
	DownloadProgressEventArgs ();
	DownloadProgressEventArgs (double progress);
	
	void SetProgress (double progress);
	
	/* @GenerateCBinding,GeneratePInvoke */
	double GetProgress ();
};

/* @Namespace=None */
class RoutedEventArgs : public EventArgs {
public:
	/* @GenerateCBinding,GeneratePInvoke */
	DependencyObject *GetSource ();
	
	/* @GenerateCBinding,GeneratePInvoke */
	void SetSource(DependencyObject *el);
	
	/* @GenerateCBinding,GeneratePInvoke */
	void SetHandled (bool handled);
	
	/* @GenerateCBinding,GeneratePInvoke */
	bool GetHandled ();

	/* @SkipFactories */
	RoutedEventArgs (DependencyObject *source);

protected:
 	/* @GenerateCBinding,GeneratePInvoke */
	RoutedEventArgs ();

	/* @SkipFactories */
	RoutedEventArgs (DependencyObject *source, Type::Kind kind);

	/* @SkipFactories */
	RoutedEventArgs (Type::Kind kind);

	virtual ~RoutedEventArgs ();

	friend class MoonUnmanagedFactory;
	friend class MoonManagedFactory;

private:
	DependencyObject *source;
	bool handled;
};

/* @Namespace=None */
class KeyEventArgs : public RoutedEventArgs {
public:
 	/* @SkipFactories */
	KeyEventArgs (MoonKeyEvent *event);
	
	/* @GenerateCBinding,GeneratePInvoke */
	int GetKey ();
	
	/* @GenerateCBinding,GeneratePInvoke */
	int GetPlatformKeyCode ();
	
	MoonKeyEvent *GetEvent () { return event; }
protected:
 	/* @GenerateCBinding,GeneratePInvoke */
	KeyEventArgs ();

	virtual ~KeyEventArgs ();

	friend class MoonUnmanagedFactory;
	friend class MoonManagedFactory;
	
private:
	MoonKeyEvent *event;
};

/* @Namespace=None */
class MouseEventArgs : public RoutedEventArgs {
public:
	/* @SkipFactories */
	MouseEventArgs (MoonMouseEvent *event);
	
	MoonMouseEvent *GetEvent () { return event; }

	/* @GenerateCBinding,GeneratePInvoke */
	void GetPosition (UIElement *relative_to, double *x, double *y);
	
	/* @GenerateCBinding,GeneratePInvoke */
	StylusInfo *GetStylusInfo ();
	
	/* @GenerateCBinding,GeneratePInvoke */
	StylusPointCollection *GetStylusPoints (UIElement *ink_presenter);
	
protected:
 	/* @GenerateCBinding,GeneratePInvoke */
	MouseEventArgs ();

	/* @SkipFactories */
	MouseEventArgs (Type::Kind kind, MoonMouseEvent *event);

	virtual ~MouseEventArgs ();

	friend class MoonUnmanagedFactory;
	friend class MoonManagedFactory;
	
private:
	MoonMouseEvent *event;
};

/* @Namespace=None */
class LogReadyRoutedEventArgs : public RoutedEventArgs {
public:
	const char *GetLog () { return log; }
	LogSource GetLogSource () { return log_source; }

protected:
	/* @GenerateCBinding,GeneratePInvoke */
	LogReadyRoutedEventArgs ();
	
	virtual ~LogReadyRoutedEventArgs () {}

	friend class MoonUnmanagedFactory;
	friend class MoonManagedFactory;

private:
	const char *log;
	LogSource log_source;
};

/* @Namespace=None */
class MouseButtonEventArgs : public MouseEventArgs {
public:
 	/* @GenerateCBinding,GeneratePInvoke */
	MouseButtonEventArgs ();

	/* @SkipFactories */
	MouseButtonEventArgs (MoonButtonEvent *event);

protected:
	virtual ~MouseButtonEventArgs ();
};

/* @Namespace=None */
class MouseWheelEventArgs : public MouseEventArgs {
public:
 	/* @GenerateCBinding,GeneratePInvoke */
	MouseWheelEventArgs ();

	/* @SkipFactories */
	MouseWheelEventArgs (MoonScrollWheelEvent *event);
	
	/* @GenerateCBinding,GeneratePInvoke */
	int GetWheelDelta ();
	
protected:
	virtual ~MouseWheelEventArgs ();
};


/* @Namespace=None,ManagedDependencyProperties=None */
class ErrorEventArgs : public EventArgs  {
private:
	MoonError *error;
	void Initialize (Type::Kind kind, ErrorEventArgsType type, const MoonError &error, int extended_error_code, const char *extended_msg);
	int extended_code;
	char *extended_message;
	ErrorEventArgsType error_type;

protected:
	virtual ~ErrorEventArgs ();

public:
	/* @SkipFactories */
	ErrorEventArgs (Type::Kind kind, ErrorEventArgsType type, const MoonError error);
	/* @SkipFactories */
	ErrorEventArgs (ErrorEventArgsType type, MoonError error);
	/* @SkipFactories */
	ErrorEventArgs (ErrorEventArgsType type, MoonError error, int extended_code, const char *extended_msg);

	/* @GenerateCBinding,GeneratePInvoke */
	gpointer GetMoonError () { return error; }

	/* @GenerateCBinding,GeneratePInvoke */
	const char *GetErrorMessage () { return error->message; }
	/* @GenerateCBinding,GeneratePInvoke */
	int GetErrorCode () { return error->code; }
	/* @GenerateCBinding,GeneratePInvoke */
	int GetErrorType () { return error_type; }

	// To match SL behaviour we need to match SL error messages, which aren't all that helpful
	// This is here to keep extra error information we have, 
	// but don't want to report to the user.
	// extended_code:
	//  3 (MEDIA_UNKNOWN_CODEC): used by playlist to determine if we should raise a MediaFailed event or just continue to play the next entry.
	int GetExtendedCode () { return extended_code; }
	const char *GetExtendedMessage () { return extended_message; }
};

/* @Namespace=None,ManagedDependencyProperties=None */
class ImageErrorEventArgs : public ErrorEventArgs {
public:
	/* @SkipFactories */
	ImageErrorEventArgs (MoonError error);

protected:
	virtual ~ImageErrorEventArgs ();

};


/* @Namespace=None,ManagedDependencyProperties=None */
class MessageReceivedEventArgs : public EventArgs {
public:
	MessageReceivedEventArgs (const char* message,
				  ReceiverNameScope namescope,
				  const char* receiverName,
				  const char* senderDomain);

	/* @GenerateCBinding,GeneratePInvoke */
	const char *GetMessage () { return message; }

	/* @GenerateCBinding,GeneratePInvoke */
	ReceiverNameScope GetNamescope () { return namescope; }

	/* @GenerateCBinding,GeneratePInvoke */
	const char *GetReceiverName () { return receiverName; }

	/* @GenerateCBinding,GeneratePInvoke */
	const char *GetResponse () { return response; }

	/* @GenerateCBinding,GeneratePInvoke */
	void SetResponse (const char *value) { g_free (response); response = g_strdup (value); }

	/* @GenerateCBinding,GeneratePInvoke */
	const char *GetSenderDomain () { return senderDomain; }

protected:
	virtual ~MessageReceivedEventArgs ();

private:
	char *message;
	ReceiverNameScope namescope;
	char *receiverName;
	char *response;
	char *senderDomain;
};

/* @Namespace=None,ManagedDependencyProperties=None */
class SendCompletedEventArgs : public EventArgs {
public:
	SendCompletedEventArgs (MoonError *error,
				const char* message,
				const char *receiverName,
				const char* receiverDomain,
				const char *response,
				gpointer managedUserState);

	/* @GenerateCBinding,GeneratePInvoke */
	void GetError (MoonError *error) {
		if (this->error) {
			*error = MoonError (*this->error);
		}
		else {
			MoonError::FillIn (error, MoonError::NO_ERROR, "");
		}
	}

	/* @GenerateCBinding,GeneratePInvoke */
	const char *GetMessage () { return message; }

	/* @GenerateCBinding,GeneratePInvoke */
	const char *GetReceiverName () { return receiverName; }

	/* @GenerateCBinding,GeneratePInvoke */
	const char *GetReceiverDomain () { return receiverDomain; }

	/* @GenerateCBinding,GeneratePInvoke */
	const char *GetResponse () { return response; }

	/* @GenerateCBinding,GeneratePInvoke */
	const gpointer GetManagedUserState () { return managedUserState; }


protected:
	virtual ~SendCompletedEventArgs ();

private:
	MoonError *error;
	char *message;
	char *receiverName;
	char *receiverDomain;
	char *response;
	gpointer managedUserState;
};

/* @Namespace=None,ManagedDependencyProperties=None */
class CaptureImageCompletedEventArgs : public EventArgs {
public:
	CaptureImageCompletedEventArgs (MoonError *error,
					BitmapSource *source);

	/* @GenerateCBinding,GeneratePInvoke */
	void GetError (MoonError *error) {
		if (this->error) {
			*error = MoonError (*this->error);
		}
		else {
			MoonError::FillIn (error, MoonError::NO_ERROR, "");
		}
	}

	/* @GenerateCBinding,GeneratePInvoke */
	BitmapSource *GetSource () { return source; }

protected:
	virtual ~CaptureImageCompletedEventArgs ();

private:
	MoonError *error;
	BitmapSource *source;
};

/* @Namespace=None,ManagedDependencyProperties=None */
class ParserErrorEventArgs : public ErrorEventArgs {
protected:
	virtual ~ParserErrorEventArgs ();

public:
	ParserErrorEventArgs (const char *msg, const char *file,
			      int line, int column, int error_code, 
			      const char *element, const char *attribute);
	
	int char_position;
	int line_number;
	char *xaml_file;
	char *xml_element;
	char *xml_attribute;
};

/* @Namespace=None */
class TimelineMarkerRoutedEventArgs : public RoutedEventArgs {
	TimelineMarker *marker;
	
 protected:
	virtual ~TimelineMarkerRoutedEventArgs ();
	
 public:
	/* @GenerateCBinding,GeneratePInvoke */
	TimelineMarkerRoutedEventArgs (TimelineMarker *marker);

	/* @GenerateCBinding,GeneratePInvoke */
	TimelineMarker *GetMarker () { return marker; }
};

/* @Namespace=None */
class CheckAndDownloadUpdateCompletedEventArgs : public EventArgs {
	bool updated;
	char *error;
	
 protected:
	virtual ~CheckAndDownloadUpdateCompletedEventArgs ();
	
 public:
	/* @GenerateCBinding,GeneratePInvoke */
	CheckAndDownloadUpdateCompletedEventArgs (bool updated, const char *error);
	
	/* @GenerateCBinding,GeneratePInvoke */
	bool GetUpdateAvailable () { return updated; }
	
	/* @GenerateCBinding,GeneratePInvoke */
	const char *GetError () { return error; }
};

};
#endif /* __EVENTARGS_H__ */
