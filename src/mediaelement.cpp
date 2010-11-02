/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * mediaelement.cpp: 
 *
 * Contact:
 *   Moonlight List (moonlight-list@lists.ximian.com)
 *
 * Copyright 2007 Novell, Inc. (http://www.novell.com)
 *
 * See the LICENSE file included with the distribution for details.
 */

#include <config.h>

#include "geometry.h"
#include "runtime.h"
#include "media.h"
#include "downloader.h"
#include "playlist.h"
#include "pipeline.h"
#include "pipeline-ui.h"
#include "mediaelement.h"
#include "debug.h"
#include "deployment.h"
#include "mediaplayer.h"
#include "timeline.h"
#include "timemanager.h"
#include "factory.h"

namespace Moonlight {

/*
 * TimelineMarkerNode
 */
class TimelineMarkerNode : public List::Node {
private:
	MediaMarker *marker;
	
public:
	TimelineMarkerNode (MediaMarker *marker)
	{
		this->marker = marker;
		marker->ref ();
	}
	virtual ~TimelineMarkerNode ()
	{
		marker->unref ();
	}
	MediaMarker *GetTimelineMarker () { return marker; }
};

/*
 * MediaElementFlags
 */
 
enum MediaElementFlags {
	ErrorPending        = (1 << 1),
	PlayRequested       = (1 << 2),  // set if Play() has been requested prior to being ready
	BufferingFailed     = (1 << 3),  // set if TryOpen failed to buffer the media.
	DisableBuffering    = (1 << 4),  // set if we cannot give useful buffering progress
	MediaOpenedEmitted  = (1 << 9),  // set if MediaOpened has been emitted.
	MissingCodecs       = (1 << 11), // set if we have no video codecs
	AutoPlayed          = (1 << 12), // set if we've autoplayed
	UpdatingSizeFromMedia = (1 << 13),
	UseMediaHeight =        (1 << 14),
	UseMediaWidth =         (1 << 15),
	Initializing =          (1 << 16),
	BufferingDisabled =     (1 << 17),
};

/*
 * MediaElement
 */

MediaElement::MediaElement ()
{
	SetObjectType (Type::MEDIAELEMENT);
	
	quality_level = 0;
	last_quality_level_change_position = 0;
	streamed_markers = NULL;
	streamed_markers_queue = NULL;
	marker_closure = NULL;
	mplayer = NULL;
	playlist = NULL;
	error_args = NULL;
	flags = UseMediaWidth | UseMediaHeight;
	detached_state = MediaElementStateClosed; 
		
	marker_timeout = 0;
	mplayer = NULL;
	
	Reinitialize (false);
	
	delete providers.dynamicvalue;
	providers.dynamicvalue = new MediaElementPropertyValueProvider (this, PropertyPrecedence_DynamicValue);
	
	// Note: BufferingTime and Position need to be set in the ctor
	// so that ReadLocalValue() will get these values.
	SetValue (MediaElement::BufferingTimeProperty, Value (TimeSpan_FromSeconds (5), Type::TIMESPAN));
	SetValue (MediaElement::PositionProperty, Value (TimeSpan_FromSeconds (0), Type::TIMESPAN));
	
	GetDeployment ()->AddHandler (Deployment::ShuttingDownEvent, ShuttingDownCallback, this);
}

void
MediaElement::Dispose ()
{
	LOG_MEDIAELEMENT ("MediaElement::Dispose ()\n");
	VERIFY_MAIN_THREAD;
	
	GetDeployment ()->RemoveHandler (Deployment::ShuttingDownEvent, ShuttingDownCallback, this);

	Reinitialize (true);
	FrameworkElement::Dispose ();
}

void
MediaElement::ShuttingDownHandler (Deployment *sender, EventArgs *args)
{
	LOG_MEDIAELEMENT ("MediaElement::ShuttingDownHandler ()\n");
	
	Reinitialize (false);
}

const char *
MediaElement::GetStateName (MediaElementState state)
{
	return enums_int_to_str ("MediaElementState", state);
}


const char *
MediaElement::GetFlagNames (guint32 flags)
{
	static char *flag_names = NULL;
	const char *v [13];
	int i = 0;

	memset (v, 0, sizeof (char *) * 13);

	if (flags & PlayRequested)
		v [i++] = "PlayRequested";
	if (flags & BufferingFailed)
		v [i++] = "BufferingFailed";
	if (flags & DisableBuffering)
		v [i++] = "DisableBuffering";
	if (flags & MediaOpenedEmitted)
		v [i++] = "MediaOpenedEmitted";
	if (flags & MissingCodecs)
		v [i++] = "MissingCodecs";
	if (flags & AutoPlayed)
		v [i++] = "AutoPlayed";
	if (flags & UpdatingSizeFromMedia)
		v [i++] = "UpdatingSizeFromMedia";
	if (flags & UseMediaHeight)
		v [i++] = "UseMediaHeight";
	if (flags & UseMediaWidth)
		v [i++] = "UseMediaWidth";
	if (flags & BufferingDisabled)
		v [i++] = "BufferingDisabled";

	g_free (flag_names);
	flag_names = (char *) g_strjoinv (", ", (gchar **) v);

	return flag_names;
}

MediaResult
MediaElement::AddStreamedMarkerCallback (MediaClosure *c)
{
	MediaMarkerFoundClosure *closure = (MediaMarkerFoundClosure *) c;
	MediaElement *element = (MediaElement *) closure->GetContext ();
	MediaMarker *mmarker = closure->GetMarker ();
	
	// Thread-safe
	
	if (mmarker == NULL)
		return MEDIA_FAIL;
	
	element->AddStreamedMarker (mmarker);
	
	return MEDIA_SUCCESS;
}

void
MediaElement::AddStreamedMarker (MediaMarker *marker)
{
	g_return_if_fail (marker != NULL);
	
	mutex.Lock ();
	if (streamed_markers_queue == NULL)
		streamed_markers_queue = new List ();
	streamed_markers_queue->Append (new TimelineMarkerNode (marker));
	mutex.Unlock ();
}

void
MediaElement::ReadMarkers (Media *media, IMediaDemuxer *demuxer)
{
	LOG_MEDIAELEMENT ("MediaElement::ReadMarkers ()\n");
	VERIFY_MAIN_THREAD;
	
	g_return_if_fail (demuxer != NULL);
	g_return_if_fail (media != NULL);
	
	for (int i = 0; i < demuxer->GetStreamCount (); i++) {
		if (demuxer->GetStream (i)->IsMarker ()) {
			MarkerStream *stream = (MarkerStream *) demuxer->GetStream (i);
			
			if (marker_closure == NULL)
				marker_closure = new MediaMarkerFoundClosure (media, AddStreamedMarkerCallback, this);
			
			stream->SetCallback (marker_closure);
			
			MediaMarker *m = stream->Pop ();
			while (m != NULL) {
				AddStreamedMarker (m);
				m->unref ();
				m = stream->Pop ();
			}
			break;
		}
	}
	
	TimelineMarkerCollection *markers = NULL;
	MediaMarker::Node *current = (MediaMarker::Node *) media->GetMarkers ()->First ();
	
	if (current == NULL) {
		//printf ("MediaElement::ReadMarkers (): no markers.\n");
		return;
	}
	
	markers = new TimelineMarkerCollection ();
	while (current != NULL) {
		TimelineMarker *new_marker = new TimelineMarker ();
		MediaMarker *marker = current->marker;
		
		new_marker->SetText (marker->Text ());
		new_marker->SetType (marker->Type ());
		new_marker->SetTime (TimeSpan_FromPts (marker->Pts ()));

		Value v(new_marker);
		markers->Add (&v);

		new_marker->unref ();
		
		current = (MediaMarker::Node *) current->next;
	}
	
	// Docs says we overwrite whatever's been loaded already.
	LOG_MEDIAELEMENT ("MediaElement::ReadMarkers (): setting %d markers.\n", markers->GetCount ());
	SetMarkers (markers);
	markers->unref ();
}

bool
MediaElement::MarkerTimeout (gpointer context)
{
	VERIFY_MAIN_THREAD;
	((MediaElement *) context)->SetCurrentDeployment ();
	((MediaElement *) context)->CheckMarkers ();
	return true;
}

void
MediaElement::SetMarkerTimeout (bool start)
{
	TimeManager *tm;
	Surface *surface;
	
	VERIFY_MAIN_THREAD;
	
	surface = GetDeployment ()->GetSurface ();
	
	if (surface == NULL)
		return;
	
	tm = surface->GetTimeManager ();
	
	g_return_if_fail (tm != NULL);
	
	if (start) {
		if (marker_timeout == 0) {
			marker_timeout = tm->AddTimeout (MOON_PRIORITY_DEFAULT, 33, MarkerTimeout, this);
		}
	} else { // stop
		if (marker_timeout != 0) {
			tm->RemoveTimeout (marker_timeout);
			marker_timeout = 0;
		}
	}
}

void
MediaElement::CheckMarkers ()
{
	guint64 current_position = GetPosition ();
	
	LOG_MARKERS_EX ("MediaElement::CheckMarkers () current position: %" G_GUINT64_FORMAT ", previous position: %" G_GUINT64_FORMAT ")\n", current_position, previous_position);
	VERIFY_MAIN_THREAD;
	
	if (current_position > previous_position && seek_to_position == -1) {
		guint64 tmp = previous_position;
		// We need to set previous_position before calling CheckMarkers, 
		// as CheckMarkers may end up emitting events, causing seeks
		// which will change previous_position.
		previous_position = current_position; 
		CheckMarkers (tmp, current_position - 1);
	}
}

void
MediaElement::CheckMarkers (guint64 from, guint64 to)
{
	TimelineMarkerCollection *markers;
	
	LOG_MARKERS_EX ("MediaElement::CheckMarkers (%" G_GUINT64_FORMAT ", %" G_GUINT64_FORMAT ")\n", from, to);
	VERIFY_MAIN_THREAD;
	
	if (from == to) {
		LOG_MARKERS ("MediaElement::CheckMarkers (%" G_GUINT64_FORMAT ", %" G_GUINT64_FORMAT "). from == to\n", from, to);
		return;
	}
	
	if (!(markers = GetMarkers ())) {
		LOG_MARKERS ("MediaElement::CheckMarkers (%" G_GUINT64_FORMAT ", %" G_GUINT64_FORMAT "). No markers\n", from, to);
		return;
	}
	
	if (from > to) {
		// if from > to we've seeked backwards (last played position is after this one)
		LOG_MARKERS ("MediaElement::CheckMarkers (%" G_GUINT64_FORMAT ", %" G_GUINT64_FORMAT "). from > to (diff: %" G_GUINT64_FORMAT " = %" G_GUINT64_FORMAT " ms).\n", from, to, from - to, MilliSeconds_FromPts (from - to));
		return;
	}
	
	/* Check if we've got streamed markers */
	mutex.Lock ();
	if (streamed_markers_queue != NULL) {
		TimelineMarkerNode *node = (TimelineMarkerNode *) streamed_markers_queue->First ();
		while (node != NULL) {
			if (streamed_markers == NULL)
				streamed_markers = new TimelineMarkerCollection ();

			MediaMarker *mmarker = node->GetTimelineMarker ();
			TimelineMarker *marker = new TimelineMarker ();
			marker->SetText (mmarker->Text ());
			marker->SetType (mmarker->Type ());
			marker->SetTime (mmarker->Pts ());
			streamed_markers->Add (marker);
			marker->unref ();

			node = (TimelineMarkerNode *) node->next;
		}
		streamed_markers_queue->Clear (true);
	}
	mutex.Unlock ();
	
	CheckMarkers (from, to, markers, false);
	CheckMarkers (from, to, streamed_markers, true);	
}

void
MediaElement::CheckMarkers (guint64 from, guint64 to, TimelineMarkerCollection *markers, bool remove)
{
	TimelineMarker *marker;
	ArrayList emit_list;
	Value *val = NULL;
	guint64 pts;
	bool emit;
	
	LOG_MARKERS ("MediaElement::CheckMarkers (%" G_GUINT64_FORMAT ", %" G_GUINT64_FORMAT ", %p, %i). count: %i\n", from, to, markers, remove, markers ? markers->GetCount () : -1);
	VERIFY_MAIN_THREAD;
	
	// We might want to use a more intelligent algorithm here, 
	// this code only loops through all markers on every frame.
	
	if (markers != NULL) {
		// don't move this markers->GetCount() out of the loop
		// condition, as the loop actually modifies the number
		// of markers in the collection.
		for (int i = 0; i < markers->GetCount(); i++) {
			marker = markers->GetValueAt (i)->AsTimelineMarker ();
			
			if (!(val = marker->GetValue (TimelineMarker::TimeProperty)))
				break;
			
			pts = (guint64) val->AsTimeSpan ();
			
			LOG_MARKERS_EX ("MediaElement::CheckMarkers (%" G_GUINT64_FORMAT ", %" G_GUINT64_FORMAT "): Checking pts: %" G_GUINT64_FORMAT ", enqueued %i elements\n", from, to, pts, emit_list.GetCount ());
			
			emit = false;
			if (remove) {
				// Streamed markers. Emit these even if we passed them with up to 1 s.
				if (from <= MilliSeconds_ToPts (1000)) {
					emit = pts >= 0 && pts <= to;
				} else {
					emit = pts >= (from - MilliSeconds_ToPts (1000)) && pts <= to;
				}
				
				LOG_MARKERS_EX ("MediaElement::CheckMarkers (%" G_GUINT64_FORMAT ", %" G_GUINT64_FORMAT "): emit: %i, Checking pts: %" G_GUINT64_FORMAT " in marker with Text = %s, Type = %s (removed from from)\n",
						from <= MilliSeconds_ToPts (1000) ? 0 : from - MilliSeconds_ToPts (1000), to, emit, pts, marker->GetText (), marker->GetType ());
			} else {
				// Normal markers.
				emit = pts >= from && pts <= to;
				LOG_MARKERS_EX ("MediaElement::CheckMarkers (%" G_GUINT64_FORMAT ", %" G_GUINT64_FORMAT "): Checking pts: %" G_GUINT64_FORMAT " in marker with Text = %s, Type = %s\n",
						from, to, pts, marker->GetText (), marker->GetType ());
			}
			
			if (emit) {
				marker->ref ();
				emit_list.Add (marker);
				
				LOG_MARKERS ("MediaElement::CheckMarkers (%" G_GUINT64_FORMAT ", %" G_GUINT64_FORMAT "): Emitting: Text = %s, Type = %s, Time = %" G_GUINT64_FORMAT " = %" G_GUINT64_FORMAT " ms, count: %in",
					     from, to, marker->GetText (), marker->GetType (), marker->GetTime (), MilliSeconds_FromPts (marker->GetTime ()), emit_list.GetCount ());
			}
			
			if (remove && (pts <= to || emit)) {
				// Also delete markers we've passed by already
				markers->RemoveAt (i);
				i--;
			}
		}
	}
	
	for (int i = 0; i < emit_list.GetCount (); i++) {
		marker = (TimelineMarker *) emit_list [i];
		Emit (MarkerReachedEvent, new TimelineMarkerRoutedEventArgs (marker));
		marker->unref ();
	}
}

void
MediaElement::OnIsAttachedChanged (bool value)
{
	VERIFY_MAIN_THREAD;
	FrameworkElement::OnIsAttachedChanged (value);

	if (!value) {
		LOG_MEDIAELEMENT ("MediaElement::SetIsAttached (%i): Stopping media element since we're detached.\n", value);
		detached_state = state;
		if (mplayer)
			mplayer->Stop (); /* this is immediate */
		if (playlist)
			playlist->StopAsync ();
		flags &= ~MediaOpenedEmitted;
		SetBufferingProgress (0.0);
		SetCanPause (false);
		SetCanSeek (false);
		Duration duration = Duration (0);
		SetNaturalDuration (&duration);
	}

	if (value) {
		LOG_MEDIAELEMENT ("MediaElement reattached, detached state: %s state: %s\n", GetStateName (detached_state), GetStateName (state));
		if (detached_state == MediaElementStatePlaying) {
			Play ();
		}
	}
}

void
MediaElement::Reinitialize (bool is_shutting_down)
{
	LOG_MEDIAELEMENT ("MediaElement::Reinitialize ()\n");
	VERIFY_MAIN_THREAD;
	
	if (mplayer) {
		mplayer->Dispose ();
		mplayer->unref ();
		mplayer = NULL;
	}
		
	if (marker_closure) {
		marker_closure->Dispose ();
		marker_closure->unref ();
		marker_closure = NULL;
	}

	if (playlist != NULL) {
		playlist->Dispose ();
		playlist->unref ();
		playlist = NULL;
	}
	
	flags &= (PlayRequested | UseMediaHeight | UseMediaWidth);

	prev_state = MediaElementStateClosed;
	state = MediaElementStateClosed;
	
	first_pts = G_MAXUINT64;
	seek_to_position = -1;
	seeked_to_position = 0;
	paused_position = 0;
	buffering_mode = 0;
	
	mutex.Lock ();
	delete streamed_markers_queue;
	streamed_markers_queue = NULL;
	if (streamed_markers) {
		streamed_markers->unref ();
		streamed_markers = NULL;
	}
	if (error_args) {
		error_args->unref ();
		error_args = NULL;
	}
	mutex.Unlock ();
	
	previous_position = 0;

	SetMarkerTimeout (false);

	if (!is_shutting_down) {
	
		Value *v = GetValueNoAutoCreate (MediaElement::MarkersProperty);
		if (v && v->AsTimelineMarkerCollection())
			v->AsTimelineMarkerCollection()->Clear();
	
		v = GetValueNoAutoCreate (MediaElement::AttributesProperty);
		if (v && v->AsMediaAttributeCollection())
			v->AsMediaAttributeCollection()->Clear();
	}

	cairo_matrix_init_identity (&matrix);
}

bool
MediaElement::IsMissingCodecs ()
{
	VERIFY_MAIN_THREAD;
	return flags & MissingCodecs;
}

Point
MediaElement::GetTransformOrigin ()
{
	Point *user_xform_origin = GetRenderTransformOrigin ();
	double h = GetActualHeight();
	double w = GetActualWidth ();
	
	if (w == 0.0 && h == 0.0 && mplayer != NULL) {
		h = (double) mplayer->GetVideoHeight ();
		w = (double) mplayer->GetVideoWidth ();
	}
	
	return Point (user_xform_origin->x * w, user_xform_origin->y * h);
}

Size
MediaElement::ComputeActualSize ()
{
	Size result = FrameworkElement::ComputeActualSize ();
	Size specified = Size (GetWidth (), GetHeight ());
	UIElement *parent = GetVisualParent ();

	if (parent && !parent->Is (Type::CANVAS))
		if (LayoutInformation::GetPreviousConstraint (this) || ReadLocalValue (LayoutInformation::LayoutSlotProperty))
			return result;
		
	if (mplayer) {
		Size available = Size (INFINITY, INFINITY);
		available = available.Min (specified);
		// FIXME: Propagate this properly
		MoonError error;
		result = MeasureOverrideWithError (available, &error);
		result = ApplySizeConstraints (result);
	}

	//g_warning ("actual is %g,%g", result.width, result.height);

	return result;
}

Size
MediaElement::MeasureOverrideWithError (Size availableSize, MoonError *error)
{
	Size desired = availableSize;
	Rect shape_bounds = Rect ();
	double sx = 0.0;
	double sy = 0.0;

	if (mplayer)
		shape_bounds = Rect (0,0,
				     mplayer->GetVideoWidth (),
				     mplayer->GetVideoHeight ());

	/* don't stretch to infinite size */
	if (isinf (desired.width))
		desired.width = shape_bounds.width;
	if (isinf (desired.height))
		desired.height = shape_bounds.height;
	
	/* compute the scaling */
	if (shape_bounds.width > 0)
		sx = desired.width / shape_bounds.width;
	if (shape_bounds.height > 0)
		sy = desired.height / shape_bounds.height;

	/* don't use infinite dimensions as constraints */
	if (isinf (availableSize.width))
		sx = sy;
	if (isinf (availableSize.height))
		sy = sx;

	switch (GetStretch ()) {
	case StretchUniform:
		sx = sy = MIN (sx, sy);
		break;
	case StretchUniformToFill:
		sx = sy = MAX (sx, sy);
		break;
	case StretchFill:
		if (isinf (availableSize.width))
			sx = sy;
		if (isinf (availableSize.height))
			sy = sx;
		break;
	case StretchNone:
		sx = sy = 1.0;
		break;
	}

	desired = Size (shape_bounds.width * sx, shape_bounds.height * sy);

	return desired;
}

Size
MediaElement::ArrangeOverrideWithError (Size finalSize, MoonError *error)
{
	Size arranged = finalSize;
	Rect shape_bounds = Rect ();
	double sx = 1.0;
	double sy = 1.0;

	if (mplayer)
		shape_bounds = Rect (0, 0, 
				     mplayer->GetVideoWidth (), 
				     mplayer->GetVideoHeight ());

	/* compute the scaling */
	if (shape_bounds.width == 0)
		shape_bounds.width = arranged.width;
	if (shape_bounds.height == 0)
		shape_bounds.height = arranged.height;

	if (shape_bounds.width != arranged.width)
		sx = arranged.width / shape_bounds.width;
	if (shape_bounds.height != arranged.height)
		sy = arranged.height / shape_bounds.height;

	switch (GetStretch ()) {
	case StretchUniform:
		sx = sy = MIN (sx, sy);
		break;
	case StretchUniformToFill:
		sx = sy = MAX (sx, sy);
		break;
	case StretchNone:
		sx = sy = 1.0;
	default:
		break;
	}

	arranged = Size (shape_bounds.width * sx, shape_bounds.height * sy);

	return arranged;
}

Rect
MediaElement::GetCoverageBounds ()
{
	MediaPlayer *mplayer = GetMediaPlayer ();
	Stretch stretch = GetStretch ();

	if  (IsClosed () || !mplayer || !mplayer->HasRenderedFrame ())
		return Rect ();

	if (stretch == StretchFill || stretch == StretchUniformToFill)
		return bounds;

	Rect video = Rect (0, 0, mplayer->GetVideoWidth (), mplayer->GetVideoHeight ());
	cairo_matrix_t brush_xform = matrix;

	cairo_matrix_invert (&brush_xform);
	cairo_matrix_multiply (&brush_xform, &brush_xform, &absolute_xform);

	video = video.Transform (&brush_xform);
	video = video.Intersection (bounds);

	return video;
}

int
MediaElement::GetQualityLevel (int min, int max)
{
	guint64 current_pts;
	gint64 diff;
	
	if (IsPlaying ()) {
		current_pts = mplayer->GetPosition ();
		diff = (gint64) current_pts - (gint64) last_quality_level_change_position;
		
		// we check the absolute diff so that we'll still change quality if the user seeks backwards in the video
		if (llabs (diff) > (gint64) MilliSeconds_ToPts (1000)) {
			// not changed within a second, candidate for another change.
			double dropped_frames = mplayer->GetDroppedFramesPerSecond ();
			if (dropped_frames == 0) {
				if (quality_level < max) {
					// better quality
					quality_level++;
					last_quality_level_change_position = current_pts;
					LOG_MEDIAELEMENT ("MediaElement::GetQualityLevel (): increased rendering quality to %i (%i-%i, higher better) - no dropped frames\n", quality_level, min, max);
				}
			} else if (dropped_frames > 5.0) {
				if (quality_level > 0) {
					// worse quality
					quality_level--;
					last_quality_level_change_position = current_pts;
					LOG_MEDIAELEMENT ("MediaElement::GetQualityLevel (): decreased rendering quality to %i  (%i-%i, higher better) - %.2f dropped frames per second with current level\n", quality_level, min, max, dropped_frames);
				}
			}
		}
	}
	
	return MIN (max, quality_level + min);
}

void
MediaElement::Render (cairo_t *cr, Region *region, bool path_only)
{
	Stretch stretch = GetStretch ();
	cairo_surface_t *surface;
	cairo_pattern_t *pattern;
	
	if (!mplayer || !(surface = mplayer->GetCairoSurface ()))
		return;

	cairo_save (cr);

        Size specified (GetActualWidth (), GetActualHeight ());
	Size stretched = ApplySizeConstraints (specified);
	bool adjust = specified != GetRenderSize ();

	if (stretch != StretchUniformToFill)
		specified = specified.Min (stretched);

	Rect paint (0, 0, specified.width, specified.height);

	/*
	if (absolute_xform.xy == 0 && absolute_xform.yx == 0) {
		//cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
		cairo_matrix_t inv = absolute_xform;
		cairo_matrix_invert (&inv);
		paint = paint.Transform (&absolute_xform);
		paint = paint.RoundIn ();
		paint = paint.Transform (&inv);
	}
	*/

	if (!path_only) {
		Rect video (0, 0, mplayer->GetVideoWidth (), mplayer->GetVideoHeight ());

		if (GetStretch () == StretchNone)
			paint = paint.Union (video);

		if (video.width == 0.0 && video.height == 0.0) {
			cairo_restore (cr);
			return;
		}

		pattern = cairo_pattern_create_for_surface (surface);

		image_brush_compute_pattern_matrix (&matrix, 
						    paint.width, paint.height, 
						    video.width, video.height,
						    stretch, AlignmentXCenter,
						    AlignmentYCenter, NULL, NULL);

		cairo_pattern_set_matrix (pattern, &matrix);
#if MAKE_EVERYTHING_SLOW_AND_BUGGY
		cairo_pattern_set_extend (pattern, CAIRO_EXTEND_PAD);
#endif
		cairo_set_source (cr, pattern);
		cairo_pattern_destroy (pattern);
	}

	if (IsPlaying ()) {
		cairo_filter_t filter;
		switch (GetQualityLevel (0, 3)) {
		case 0: filter = CAIRO_FILTER_FAST; break;
		case 1: filter = CAIRO_FILTER_GOOD; break;
		case 2: filter = CAIRO_FILTER_BILINEAR; break;
		default: filter = CAIRO_FILTER_BEST; break;
		}
		cairo_pattern_set_filter (cairo_get_source (cr), filter);
	}

	if (adjust) {
		// FIXME: Propagate this properly
		MoonError error;
		specified = MeasureOverrideWithError (specified, &error);
		paint = Rect ((stretched.width - specified.width) * 0.5, (stretched.height - specified.height) * 0.5, specified.width, specified.height);
	}


	//if (!path_only)
	//	RenderLayoutClip (cr);

	paint = paint.Intersection (Rect (0, 0, stretched.width, stretched.height));
	paint.Draw (cr);

	if (!path_only)
		cairo_fill (cr);

	cairo_restore (cr);
}

bool
MediaElement::InsideObject (cairo_t *cr, double x, double y)
{
	if (!FrameworkElement::InsideObject (cr, x, y))
		return false;

	cairo_save (cr);
	cairo_new_path (cr);
	cairo_set_matrix (cr, &absolute_xform);

	double nx = x;
	double ny = y;

	TransformPoint (&nx, &ny);

	Render (cr, NULL, true);
	bool inside = cairo_in_fill (cr, nx, ny);
	cairo_restore (cr);

	if (inside)
		inside = InsideLayoutClip (x, y);

	if (inside)
		inside = InsideClip (cr, x, y);

	return inside;
}

void
MediaElement::BufferUnderflowHandler (PlaylistRoot *sender, EventArgs *args)
{
	LOG_MEDIAELEMENT ("MediaElement::BufferUnderflow (): Switching to 'Buffering', previous_position: %" G_GUINT64_FORMAT " ms, mplayer->GetPosition (): %" G_GUINT64_FORMAT " ms\n", 
		 MilliSeconds_FromPts (previous_position), MilliSeconds_FromPts (mplayer->GetPosition ()));
		
	flags |= PlayRequested;
	SetBufferingProgress (0.0);
	Emit (BufferingProgressChangedEvent);
	SetState (MediaElementStateBuffering);
	mplayer->Pause ();
	/* We need to inform the Media instance that we want more BufferingProgressChanged events.
	 * With a small BufferingTime the Media instance might already have refilled
	 * the buffer, and if we don't request events to be sent, we'll just keep waiting for them */
	mplayer->GetMedia ()->ClearBufferingProgress ();
}

void
MediaElement::EmitStateChangedAsync ()
{
	AddTickCall (EmitStateChanged);
}

void
MediaElement::EmitStateChanged (EventObject *obj)
{
	MediaElement *mel = (MediaElement *) obj;
	if (mel->IsAttached ())
		mel->Emit (CurrentStateChangedEvent);
}

void
MediaElement::SetState (MediaElementState state)
{
	bool emit = false;
	
	LOG_MEDIAELEMENT ("MediaElement::SetState (%d): New state: %s, old state: %s\n",
			  state, GetStateName (state), GetStateName (this->state));	

	// thread-safe
	
	mutex.Lock ();
	if (this->state != state) {
		prev_state = this->state;
		this->state = state;
		emit = true;
	}
	mutex.Unlock ();
	
	if (emit) // Don't emit with mutex locked.
		EmitStateChangedAsync ();
}

void
MediaElement::CreatePlaylist ()
{
	g_return_if_fail (mplayer == NULL);
	
	mplayer = new MediaPlayer (this);
	SetPlaylist (new PlaylistRoot (this));
}

void
MediaElement::SetPlaylist (PlaylistRoot *value)
{
	// if playlist is something, then value must be null (and vice versa)
	g_return_if_fail ((playlist == NULL) != (value == NULL));
	
	VERIFY_MAIN_THREAD;
	
	if (playlist != NULL) {
		playlist->RemoveAllHandlers (this);
		playlist->Dispose ();
		playlist->unref ();
		playlist = NULL;
	} else {
		playlist = value; // We assume the caller gives us a reference to the playlist
		playlist->AddHandler (PlaylistRoot::OpeningEvent, OpeningCallback, this);
		playlist->AddHandler (PlaylistRoot::OpenCompletedEvent, OpenCompletedCallback, this);
		playlist->AddHandler (PlaylistRoot::SeekingEvent, SeekingCallback, this);
		playlist->AddHandler (PlaylistRoot::SeekCompletedEvent, SeekCompletedCallback, this);
		playlist->AddHandler (PlaylistRoot::CurrentStateChangedEvent, CurrentStateChangedCallback, this);
		playlist->AddHandler (PlaylistRoot::MediaErrorEvent, MediaErrorCallback, this);
		playlist->AddHandler (PlaylistRoot::MediaEndedEvent, MediaEndedCallback, this);
		playlist->AddHandler (PlaylistRoot::BufferUnderflowEvent, BufferUnderflowCallback, this);
		playlist->AddHandler (PlaylistRoot::DownloadProgressChangedEvent, DownloadProgressChangedCallback, this);
		playlist->AddHandler (PlaylistRoot::BufferingProgressChangedEvent, BufferingProgressChangedCallback, this);
		playlist->AddHandler (PlaylistRoot::PlayEvent, PlayCallback, this);
		playlist->AddHandler (PlaylistRoot::PauseEvent, PauseCallback, this);
		playlist->AddHandler (PlaylistRoot::StopEvent, StopCallback, this);
		playlist->AddHandler (PlaylistRoot::EntryChangedEvent, EntryChangedCallback, this);
	}
}

void
MediaElement::EntryChangedHandler (PlaylistRoot *playlist, EventArgs *args)
{
	LOG_MEDIAELEMENT ("MediaElement::EntryChangedHandler ()\n");
	flags &= ~MediaOpenedEmitted;
}

void
MediaElement::OpeningHandler (PlaylistRoot *playlist, EventArgs *args)
{
	LOG_MEDIAELEMENT ("MediaElement::OpeningHandler ()\n");
	VERIFY_MAIN_THREAD;
	
	flags &= ~Initializing;
	SetState (MediaElementStateOpening);
}

void
MediaElement::OpenCompletedHandler (PlaylistRoot *playlist, EventArgs *args)
{
	IMediaDemuxer *demuxer;
	const char *demuxer_name;
	PlaylistEntry *entry;
	Media *media;
	
	VERIFY_MAIN_THREAD;
	
	g_return_if_fail (playlist != NULL);
	g_return_if_fail (mplayer != NULL);
	
	entry = playlist->GetCurrentPlaylistEntry ();
	
	g_return_if_fail (entry != NULL);
	
	media = entry->GetMedia ();
	
	g_return_if_fail (media != NULL);
	
	demuxer = media->GetDemuxerReffed ();
	demuxer_name = demuxer->GetTypeName ();
	
	if (demuxer->IsDrm ()) {
		LOG_MEDIAELEMENT ("MediaElement::OpenCompletedHandler () drm source\n");
		GetDeployment ()->GetSurface ()->ShowDrmMessage ();
		ErrorEventArgs *eea = new ErrorEventArgs (this, MediaError, MoonError (MoonError::EXCEPTION, 6000, "DRM_E_UNABLE_TO_PLAY_PROTECTED_CONTENT"));
		ReportErrorOccurred (eea);
		eea->unref ();
	}
	
	LOG_MEDIAELEMENT ("MediaElement::OpenCompletedHandler (%p), demuxer name: %s drm: %i\n", media, demuxer_name, demuxer->IsDrm ());
	
	// Try to figure out if we're missing codecs	
	for (int i = 0; i < demuxer->GetStreamCount (); i++) {
		IMediaStream *stream = demuxer->GetStream (i);
		IMediaDecoder *decoder = stream->GetDecoder ();
		if (decoder != NULL && decoder->GetObjectType () == Type::NULLDECODER) {
			flags |= MissingCodecs;
			break;
		}
	}
	
	demuxer->unref ();
	demuxer = NULL;

	// We need to check again since we might have emitted an event earlier (drm error message)
	if (playlist == NULL || mplayer == NULL)  {
		LOG_MEDIAELEMENT ("MediaElement::OpenCompletedHandler (): open cancelled somehow\n");
		return;
	}

	// check if we're missing the codecs *and* if they are not installed 
	// since we could already have downloaded/installed them without refreshing the browser (leading to a crash)
	if ((flags & MissingCodecs) && !Media::IsMSCodecsInstalled ()) {
		// FIXME: PAL THIS
#if defined(__APPLE__)
		g_warning ("I'm running on our only pal and I dont know what to do.  Help me and I've fallen and I can't get up");
#else
		CodecDownloader::ShowUI (GetDeployment ()->GetSurface (), false);
#endif
	}

	entry->PopulateMediaAttributes ();
	SetProperties (media);
	
	if (!(flags & MediaOpenedEmitted)) {
		flags |= MediaOpenedEmitted;
		
		PlayOrStop ();
	
		// This is a workaround for MS DRT #78: it tests that download progress has changed
		// from the latest DownloadProgressChanged event to the MediaOpened event (unless
		// DownloadProgress is 0.0 or 1.0).
		double progress = media->GetDownloadProgress ();
		progress = MAX (progress, GetDownloadProgress ());
		progress = MIN (progress + 0.00000001, 1.0);
		SetDownloadProgress (progress);
		Emit (MediaOpenedEvent, MoonUnmanagedFactory::CreateRoutedEventArgs ());
		Emit (DownloadProgressChangedEvent);
	}
}

void
MediaElement::SetProperties (Media *media)
{
	IMediaDemuxer *demuxer = NULL;
	PlaylistEntry *entry;
	Duration *natural_duration;
	bool can_seek = true;
	bool can_pause = true;
	
	LOG_MEDIAELEMENT ("MediaElement::SetProperties (%p)\n", media);
	
	g_return_if_fail (media != NULL);
	g_return_if_fail (playlist != NULL);
	
	seeked_to_position = 0;
	
	demuxer = media->GetDemuxerReffed ();
	entry = playlist->GetCurrentPlaylistEntry ();
	
	if (demuxer == NULL || entry == NULL) {
#if SANITY
		g_warning ("MediaElement::SetProperties (%p): demuxer is null or entry is null (demuxer: %p entry: %p)\n", media, demuxer, entry);
#endif
		goto cleanup;
	}
	
	ReadMarkers (media, demuxer);
	
	if (entry->GetIsLive ()) {
		can_seek = false;
		can_pause = false;
	} else {
		can_seek = entry->GetClientSkip () && demuxer->GetCanSeek ();
		can_pause = true;
	}
	
	natural_duration = new Duration (TimeSpan_FromPts (mplayer->GetDuration ()));
	SetCanPause (can_pause);
	SetCanSeek (can_seek);
	SetNaturalDuration (natural_duration);
	SetNaturalVideoHeight ((double) mplayer->GetVideoHeight ());
	SetNaturalVideoWidth ((double) mplayer->GetVideoWidth ());
	SetAudioStreamCount (mplayer->GetAudioStreamCount ());

	mplayer->SetMuted (GetIsMuted ());
	mplayer->SetVolume (GetVolume ());

	UpdateBounds ();
	InvalidateMeasure ();
	InvalidateArrange ();

cleanup:
	if (demuxer)
		demuxer->unref ();
}

void
MediaElement::SeekingHandler (PlaylistRoot *playlist, EventArgs *args)
{
	LOG_MEDIAELEMENT ("MediaElement::SeekingHandler () state: %s\n", GetStateName (state));
	VERIFY_MAIN_THREAD;
	
	SetMarkerTimeout (false);

	if (GetBufferingProgress () != 0.0) {
		SetBufferingProgress (0.0);
		Emit (BufferingProgressChangedEvent);
	}
}

void
MediaElement::SeekCompletedHandler (PlaylistRoot *playlist, EventArgs *args)
{
	LOG_MEDIAELEMENT ("MediaElement::SeekCompletedHandler () state: %s\n", GetStateName (state));
	VERIFY_MAIN_THREAD;
	
	seek_to_position = -1;
	SetMarkerTimeout (true);
}

void
MediaElement::PlayHandler (PlaylistRoot *playlist, EventArgs *args)
{
	LOG_MEDIAELEMENT ("MediaElement::PlayHandler ()\n");
	VERIFY_MAIN_THREAD;
	
	SetMarkerTimeout (true);
	
	SetState (MediaElementStatePlaying);
}

void
MediaElement::PauseHandler (PlaylistRoot *playlist, EventArgs *args)
{
	LOG_MEDIAELEMENT ("MediaElement::PauseHandler ()\n");
	VERIFY_MAIN_THREAD;
	
	SetMarkerTimeout (false);
	
	SetState (MediaElementStatePaused);
}

void
MediaElement::StopHandler (PlaylistRoot *playlist, EventArgs *args)
{
	PlaylistEntry *entry;
	
	LOG_MEDIAELEMENT ("MediaElement::StopHandler ()\n");
	VERIFY_MAIN_THREAD;
	
	g_return_if_fail (playlist != NULL);
	
	entry = playlist->GetCurrentPlaylistEntry ();
	
	g_return_if_fail (entry != NULL);
	seeked_to_position = 0;
		
	SetProperties (entry->GetMedia ());
	
	SetMarkerTimeout (false);
	CheckMarkers (); // check one last time.
	
	SetState (MediaElementStateStopped);
}

void
MediaElement::CurrentStateChangedHandler (PlaylistRoot *playlist, EventArgs *args)
{
	LOG_MEDIAELEMENT ("MediaElement::CurrentStateChangedHandler ()\n");
	VERIFY_MAIN_THREAD;
}

void
MediaElement::MediaErrorHandler (PlaylistRoot *playlist, ErrorEventArgs *args)
{
	LOG_MEDIAELEMENT ("MediaElement::MediaErrorHandler (). State: %s Message: %s Initializing: %i Uri: %s\n", GetStateName (state), args ? args->GetErrorMessage() : NULL, flags & Initializing, GetSource ()->ToString ());
	VERIFY_MAIN_THREAD;
	
	if (state == MediaElementStateClosed && !(flags & Initializing) && !(flags & ErrorPending))
		return;

	if (flags & Initializing) {
		flags |= ErrorPending;
		ReportErrorOccurred (args, true);
		return;
	}
	
	// TODO: Should ClearValue be called on these instead?
	SetAudioStreamCount (0);
	SetNaturalVideoHeight (0);
	SetNaturalVideoWidth (0);
	SetNaturalDuration (0);
	SetCanPause (false);
	SetCanSeek (false);
	SetDownloadProgress (0);
	SetDownloadProgressOffset (0);
	SetRenderedFramesPerSecond (0);
	SetDroppedFramesPerSecond (0);

	UpdateBounds ();
	InvalidateMeasure ();
	InvalidateArrange ();

	SetState (MediaElementStateClosed);

	args = new ErrorEventArgs (this, args);
	if (HasHandlers (MediaFailedEvent)) {
		Emit (MediaFailedEvent, args);
	} else {
		GetDeployment ()->GetSurface ()->EmitError (args);
	}
	flags &= ~ErrorPending;
}

void
MediaElement::MediaEndedHandler (PlaylistRoot *playlist, EventArgs *args)
{
	LOG_MEDIAELEMENT ("MediaElement::MediaEndedHandler () state: %s position: %" G_GUINT64_FORMAT "\n", GetStateName (state), MilliSeconds_FromPts (GetPosition ()));
	VERIFY_MAIN_THREAD;
	
	CheckMarkers ();
	paused_position = GetNaturalDuration ()->GetTimeSpan ();
	SetState (MediaElementStatePaused);
	Emit (MediaEndedEvent);
}

void
MediaElement::DownloadProgressChangedHandler (PlaylistRoot *playlist, EventArgs *args)
{
	ProgressEventArgs *pea = (ProgressEventArgs *) args;
	
	LOG_MEDIAELEMENT ("MediaElement::DownloadProgressChangedHandler (): %f\n", pea ? pea->progress : -1.0);
	VERIFY_MAIN_THREAD;
	
	g_return_if_fail (pea != NULL);
	
	SetDownloadProgress (pea->progress);
	Emit (DownloadProgressChangedEvent);
}

void
MediaElement::BufferingProgressChangedHandler (PlaylistRoot *playlist, EventArgs *args)
{
	ProgressEventArgs *pea = (ProgressEventArgs *) args;
	
	LOG_MEDIAELEMENT ("MediaElement::BufferingProgressChangedHandler (): %f state: %s\n", pea ? pea->progress : -1.0, GetStateName (state));
	VERIFY_MAIN_THREAD;
	
	g_return_if_fail (pea != NULL);

	if (GetBufferingProgress () < pea->progress) {
		if (state == MediaElementStatePlaying || state == MediaElementStatePaused) {
			if (state == MediaElementStatePlaying)
				flags |= PlayRequested;
			/* this is wrong when the user calls Play while we're still buffering because we'd jump back to the buffering state later (but we'd continue playing) */
			/* if we set this earlier though (SeekCompletedHandler) MS DRT #115 fails */
			if (!(flags & BufferingDisabled)) {
				SetState (MediaElementStateBuffering);
			}
		}
		SetBufferingProgress (pea->progress);
		if (IsAttached ())
			Emit (BufferingProgressChangedEvent);
	}
	
	if (pea->progress >= 1.0) {
		if (GetState () == MediaElementStateBuffering) {
			LOG_MEDIAELEMENT ("MediaElement::BufferingProgressChangedHandler (): buffer full, playing...\n");
			PlayOrStop ();
		} else if (flags & PlayRequested) {
			LOG_MEDIAELEMENT ("MediaElement::BufferingProgressChangedHandler (): buffer full, state: %s PlayRequested: 1\n", GetStateName (state));
			Play ();
		}
	}
}

void
MediaElement::MediaInvalidate ()
{
	Emit (MediaInvalidatedEvent);
	Invalidate ();
}

void
MediaElement::SetUriSource (const Uri *uri)
{
	LOG_MEDIAELEMENT ("MediaElement::SetUriSource ('%s')\n", uri ? uri->ToString () : NULL);
	VERIFY_MAIN_THREAD;
	
	Reinitialize (false);
	
	g_return_if_fail (playlist == NULL);
	
	flags |= Initializing;
	
	if (!Uri::IsNullOrEmpty (uri)) {
		CreatePlaylist ();
		playlist->GetCurrentEntry ()->InitializeWithUri (GetResourceBase (), uri);
	} else {
		UpdateBounds ();
		InvalidateMeasure ();
		InvalidateArrange ();
	}
	
	flags &= ~Initializing;
}

void
MediaElement::SetSource (Downloader *downloader, const char *PartName)
{
	LOG_MEDIAELEMENT ("MediaElement::SetSource (%p, '%s')\n", downloader, PartName);
	VERIFY_MAIN_THREAD;

	Reinitialize (false);
	
	g_return_if_fail (downloader != NULL);
	g_return_if_fail (playlist == NULL);
	
	flags |= Initializing;
	
	CreatePlaylist ();
	playlist->GetCurrentEntry ()->InitializeWithDownloader (downloader, PartName);
	
	flags &= ~Initializing;
}

void
MediaElement::SetStreamSource (ManagedStreamCallbacks *callbacks)
{
	LOG_MEDIAELEMENT ("MediaElement::SetStreamSource (%p)\n", callbacks);
	VERIFY_MAIN_THREAD;

	Reinitialize (false);
	
	g_return_if_fail (callbacks != NULL);
	g_return_if_fail (playlist == NULL);
	
	flags |= Initializing;
	
	CreatePlaylist ();
	playlist->GetCurrentEntry ()->InitializeWithStream (callbacks);
	
	SetDownloadProgress (1.0);
	
	flags &= ~Initializing;
}

IMediaDemuxer *
MediaElement::SetDemuxerSource (void *context, CloseDemuxerCallback close_demuxer, GetDiagnosticAsyncCallback get_diagnostic, GetFrameAsyncCallback get_sample,
	OpenDemuxerAsyncCallback open_demuxer, SeekAsyncCallback seek, SwitchMediaStreamAsyncCallback switch_media_stream)
{
	ExternalDemuxer *demuxer;
	Media *media;
	
	LOG_MEDIAELEMENT ("MediaElement::SetDemuxerSource ()\n");
	VERIFY_MAIN_THREAD;

	Reinitialize (false);
	
	g_return_val_if_fail (context != NULL, NULL);
	g_return_val_if_fail (close_demuxer != NULL && get_diagnostic != NULL && get_sample != NULL && open_demuxer != NULL && seek != NULL && switch_media_stream != NULL, NULL);
	g_return_val_if_fail (playlist == NULL, NULL);
	
	flags |= Initializing;
	flags |= BufferingDisabled;
	
	CreatePlaylist ();
	media = new Media (playlist);
	demuxer = new ExternalDemuxer (media, context, close_demuxer, get_diagnostic, get_sample, open_demuxer, seek, switch_media_stream);
	playlist->GetCurrentEntry ()->InitializeWithDemuxer (demuxer);
	media->unref ();
	
	SetDownloadProgress (1.0);
	
	flags &= ~Initializing;
	
	return demuxer;
}

void
MediaElement::SetPlayRequested ()
{
	LOG_MEDIAELEMENT ("MediaElement::SetPlayRequested ()\n");
	VERIFY_MAIN_THREAD;
	
	flags |= PlayRequested;
}

void
MediaElement::PlayOrStop ()
{
	LOG_MEDIAELEMENT ("MediaElement::PlayOrPause (): GetCanPause (): %s, PlayRequested: %s, GetAutoPlay: %s, AutoPlayed: %s IsAttached: %s\n",
			  GetCanPause () ? "true" : "false", (flags & PlayRequested) ? "true" : "false",
			  GetAutoPlay () ? "true" : "false", (flags & AutoPlayed) ? "true" : "false", IsAttached () ? "true" : "false");
	VERIFY_MAIN_THREAD;

	if (!IsAttached ())
		return;
	
	if (!GetCanPause ()) {
		// If we can't pause, we play
		SetState (MediaElementStatePlaying);
		playlist->PlayAsync ();
	} else if (flags & PlayRequested) {
		// A Play has already been requested.
		SetState (MediaElementStatePlaying);
		playlist->PlayAsync ();
	} else if (GetAutoPlay () && !(flags & AutoPlayed)) {
		// Autoplay us.
		flags |= AutoPlayed;
		SetState (MediaElementStatePlaying);
		playlist->PlayAsync ();
	} else {
		SetState (MediaElementStatePaused);
	}
}

void
MediaElement::Pause ()
{
	LOG_MEDIAELEMENT ("MediaElement::Pause (): current state: %s\n", GetStateName (state));
	VERIFY_MAIN_THREAD;
	
	if (playlist == NULL)
		return;
	
	switch (state) {
	case MediaElementStateOpening:// docs: No specified behaviour
		flags &= ~PlayRequested;
		return;
	case MediaElementStateClosed: // docs: No specified behaviour
		return;
	case MediaElementStatePaused:// docs: no-op
	case MediaElementStateBuffering:
	case MediaElementStatePlaying:
	case MediaElementStateStopped: // docs: pause
		flags &= ~PlayRequested;
		paused_position = GetPosition ();
		SetState (MediaElementStatePaused);
		playlist->PauseAsync ();
		break;
	case MediaElementStateIndividualizing:
	case MediaElementStateAcquiringLicense:
		g_warning ("MediaElement: Invalid state.");
		return;
	}
}

void
MediaElement::Play ()
{
	LOG_MEDIAELEMENT ("MediaElement::Play (): current state: %s\n", GetStateName (state));
	VERIFY_MAIN_THREAD;
	
	if (!IsAttached ())
		return;

	if (playlist == NULL) {
		LOG_MEDIAELEMENT ("MediaElement::Play (): no source set yet.\n");
		return;
	}
	
	switch (state) {
	case MediaElementStateClosed: // docs: No specified behaviour
	case MediaElementStateOpening:// docs: No specified behaviour
		flags |= PlayRequested;
		break;
	case MediaElementStatePlaying:// docs: no-op
	case MediaElementStateBuffering:
	case MediaElementStatePaused:
	case MediaElementStateStopped:
		SetState (MediaElementStatePlaying);
		playlist->PlayAsync ();
		break;
	case MediaElementStateIndividualizing:
	case MediaElementStateAcquiringLicense:
		g_warning ("MediaElement: Invalid state.");
		return;
	}
}

void
MediaElement::Stop ()
{
	LOG_MEDIAELEMENT ("MediaElement::Stop (): current state: %s\n", GetStateName (state));
	VERIFY_MAIN_THREAD;
	
	if (!IsAttached ())
		return;

	switch (state) {
	case MediaElementStateOpening:// docs: No specified behaviour
		flags &= ~PlayRequested;
		return;
	case MediaElementStateClosed: // docs: No specified behaviour
	case MediaElementStateStopped:// docs: no-op
		return;
	case MediaElementStateBuffering:
	case MediaElementStatePlaying:
	case MediaElementStatePaused: // docs: stop
		flags &= ~PlayRequested;

		LOG_MEDIAELEMENT ("MediaElement::Stop (): state: %s, IsSingleFile: %i\n", GetStateName (state), playlist->IsSingleFile ());
		
		if (!playlist->IsSingleFile ())
			flags &= ~MediaOpenedEmitted; // for playlists we must re-emit MediaOpened when the first entry/media has been re-loaded (even though we stop the first entry).
		
		SetState (MediaElementStateStopped);
		playlist->StopAsync ();
		break;
	case MediaElementStateIndividualizing:
	case MediaElementStateAcquiringLicense:
		g_warning ("MediaElement: Invalid state.");
		return;
	}
}

void
MediaElement::Seek (TimeSpan to, bool force)
{
	LOG_MEDIAELEMENT ("MediaElement::Seek (%" G_GUINT64_FORMAT " = %" G_GUINT64_FORMAT " ms) state: %s\n", to, MilliSeconds_FromPts (to), GetStateName (state));
	VERIFY_MAIN_THREAD;

	if (!force && !IsAttached ()) {
		LOG_MEDIAELEMENT ("MediaElement::Seek (): not attached.\n");
		return;
	}
		
	if (!force && !GetCanSeek ()) {
		LOG_MEDIAELEMENT ("MediaElement::Seek (): CanSeek is false, not seeking\n");
		return;
	}
	
	switch (state) {
	case MediaElementStateIndividualizing:
	case MediaElementStateAcquiringLicense:
		g_warning ("MediaElement:Seek (): Invalid state %s\n", GetStateName (state));
		// Fall through
	case MediaElementStateOpening:
	case MediaElementStateClosed:
		if (!force)
			return;
		/* fall through */
	case MediaElementStateBuffering:
	case MediaElementStatePlaying:
	case MediaElementStatePaused:
	case MediaElementStateStopped:
		Duration *duration = GetNaturalDuration ();
		
		if (duration->HasTimeSpan () && to > duration->GetTimeSpan ())
			to = duration->GetTimeSpan ();
		else if (to < 0)
			to = 0;
		
		// The following breaks playback of the video at www.nbcolympics.com - 
		// it seems like the smooth streaming code is expecting us to request a 
		// seek in this case, even though I have not been able to create a test
		// case. The main issue is that if we request frames from the smooth
		// streaming code when the smooth streaming code things we're waiting
		// for a seek, it'll return frames with pts = 0. This messes up our
		// buffering (we'll just buffer everything we can get), eventually
		// leading to strange out of memory conditions (failed to allocate memory
		// because you've reached the max number of mmaped areas, not because you
		// really ran out of memory).
		//if (!force && to == TimeSpan_FromPts (mplayer->GetPosition ()))
		//	return;
		
		previous_position = to;
		seek_to_position = to;
		seeked_to_position = to;
		paused_position = to;

		if (state == MediaElementStatePlaying)
			flags |= PlayRequested;

		mplayer->NotifySeek (TimeSpan_ToPts (to));
		playlist->SeekAsync (to);
		Emit (MediaInvalidatedEvent);
		Invalidate ();
		
		LOG_MEDIAELEMENT ("MediaElement::Seek (%" G_GUINT64_FORMAT " = %" G_GUINT64_FORMAT " ms) previous position: %" G_GUINT64_FORMAT "\n", to, MilliSeconds_FromPts (to), previous_position);
		
		break;
	}
}

void
MediaElement::OnPropertyChanged (PropertyChangedEventArgs *args, MoonError *error)
{
	if (args->GetId () == MediaElement::SourceProperty) {
		DownloaderAccessPolicy policy = MediaPolicy;
		const Uri *uri = GetSource ();
		const Uri *location;
		
		if (uri != NULL) {
			if (!(location = GetDeployment ()->GetXapLocation ()))
				location = GetDeployment ()->GetSurface ()->GetSourceLocation ();
			
			if (uri->IsAbsolute () && uri->GetScheme () && (uri->IsScheme ("mms") || uri->IsScheme ("rtsp") || uri->IsScheme ("rtsps")))
				policy = StreamingPolicy;
		}
		
		SetUriSource (uri);
	} else if (args->GetId () == MediaElement::AudioStreamIndexProperty) {
		if (mplayer)
			mplayer->SetAudioStreamIndex (args->GetNewValue()->AsInt32 ());
	} else if (args->GetId () == MediaElement::AutoPlayProperty) {
		// no state to change
	} else if (args->GetId () == MediaElement::BalanceProperty) {
		if (mplayer)
			mplayer->SetBalance (args->GetNewValue()->AsDouble ());
	} else if (args->GetId () == MediaElement::BufferingProgressProperty) {
		// read-only property
	} else if (args->GetId () == MediaElement::BufferingTimeProperty) {
		// TODO
		// Not quite sure what to do here, we could:
		// a) store the buffering time on the mediaplayer and let the media request it whenever it needs it 
		// b) let the media request the buffering time from mediaelement
		// c) always keep the current media up to date (this is harder to do right when we get multiple media downloading at the same time)
		// note that thread-safety is easier with a) (media will do the request from another thread) or c)
		/*
		if (media)
			media->SetBufferingTime (TimeSpan_ToPts (GetBufferingTime ()));
		*/
	} else if (args->GetId () == MediaElement::CurrentStateProperty) {
		// read-only property
		// This should really not happen, we use a property provider for this property.
	} else if (args->GetId () == MediaElement::IsMutedProperty) {
		if (mplayer)
			mplayer->SetMuted (args->GetNewValue()->AsBool ());
	} else if (args->GetId () == MediaElement::MarkersProperty) {
		// 
	} else if (args->GetId () == MediaElement::NaturalVideoHeightProperty) {
		// read-only property
	} else if (args->GetId () == MediaElement::NaturalVideoWidthProperty) {
		// read-only property
	} else if (args->GetId () == MediaElement::PositionProperty) {
		Seek (args->GetNewValue()->AsTimeSpan (), false);
		ClearValue (MediaElement::PositionProperty, false); // We need this, otherwise our property system will return the seeked-to position forever (MediaElementPropertyValueProvider isn't called).
	} else if (args->GetId () == MediaElement::VolumeProperty) {
		if (mplayer)
			mplayer->SetVolume (args->GetNewValue()->AsDouble ());
	}
	
	if (args->GetProperty ()->GetOwnerType() != Type::MEDIAELEMENT) {
		// propagate to parent class
		FrameworkElement::OnPropertyChanged (args, error);
		return;
	}
	
	NotifyListenersOfPropertyChange (args, error);
}

bool 
MediaElement::EnableAntiAlias (void)
{
	return !(absolute_xform.xx == absolute_xform.yy &&		/* no rotation */
		 (absolute_xform.yx == 0 && absolute_xform.xy == 0));	/* no skew */

}

void
MediaElement::ReportErrorOccurredCallback (EventObject *obj)
{
	MediaElement *me = (MediaElement *) obj;
	ErrorEventArgs *args;
	
	me->mutex.Lock ();
	args = me->error_args;
	me->error_args = NULL;
	me->mutex.Unlock ();
	
	me->ReportErrorOccurred (args);
	if (args)
		args->unref ();
}

void
MediaElement::ReportErrorOccurred (ErrorEventArgs *args)
{
	ReportErrorOccurred (args, false);
}

void
MediaElement::ReportErrorOccurred (ErrorEventArgs *args, bool force_async)
{
	LOG_MEDIAELEMENT ("MediaElement::ReportErrorOccurred (%p)\n", args);

	if (force_async || !Surface::InMainThread ()) {
		mutex.Lock ();
		if (error_args)
			error_args->unref ();
		error_args = args;
		if (error_args)
			error_args->ref ();
		mutex.Unlock ();
		AddTickCall (ReportErrorOccurredCallback);
		return;
	}
	
	VERIFY_MAIN_THREAD;
	MediaErrorHandler (NULL, args);
}

void
MediaElement::ReportErrorOccurred (const char *args)
{
	LOG_MEDIAELEMENT ("MediaElement::ReportErrorOccurred ('%s')\n", args);

	ErrorEventArgs *eea = new ErrorEventArgs (this, MediaError, MoonError (MoonError::EXCEPTION, 3001, g_strdup (args)));
	ReportErrorOccurred (eea);
	eea->unref ();
}

/*
 * MediaElementPropertyValueProvider
 */

MediaElementPropertyValueProvider::MediaElementPropertyValueProvider (MediaElement *element, PropertyPrecedence precedence)
	: FrameworkElementProvider (element, precedence)
{
	position = NULL;
	current_state = NULL;
	rendered_frames_per_second = NULL;
	dropped_frames_per_second = NULL;
}

MediaElementPropertyValueProvider::~MediaElementPropertyValueProvider ()
{
	delete position;
	delete current_state;
	delete rendered_frames_per_second;
	delete dropped_frames_per_second;
}

Value *
MediaElementPropertyValueProvider::GetPropertyValue (DependencyProperty *property)
{
	// We verify main thread here too in case some object in the pipeline happens to want a property on the media element
	VERIFY_MAIN_THREAD;
	
	if (property->GetId () == MediaElement::PositionProperty)
		return GetPosition ();
		
	if (property->GetId () == MediaElement::CurrentStateProperty)
		return GetCurrentState ();
	
	if (property->GetId () == MediaElement::DroppedFramesPerSecondProperty)
		return GetDroppedFramesPerSecond ();
	
	if (property->GetId () == MediaElement::RenderedFramesPerSecondProperty)
		return GetRenderedFramesPerSecond ();

	return FrameworkElementProvider::GetPropertyValue (property);
}

Value *
MediaElementPropertyValueProvider::GetDroppedFramesPerSecond ()
{
	MediaElement *element = (MediaElement *) obj;
	MediaPlayer *mplayer = element->GetMediaPlayer ();
	
	delete dropped_frames_per_second;
	
	if (mplayer) {
		dropped_frames_per_second = new Value (mplayer->GetDroppedFramesPerSecond ());
	} else {
		dropped_frames_per_second = NULL;
	}
	
	return dropped_frames_per_second;
}

Value *
MediaElementPropertyValueProvider::GetRenderedFramesPerSecond ()
{
	MediaElement *element = (MediaElement *) obj;
	MediaPlayer *mplayer = element->GetMediaPlayer ();
	
	delete rendered_frames_per_second;
	
	if (mplayer) {
		rendered_frames_per_second = new Value (mplayer->GetRenderedFramesPerSecond ());
	} else {
		rendered_frames_per_second = NULL;
	}
	
	return rendered_frames_per_second;
}

Value *
MediaElementPropertyValueProvider::GetCurrentState ()
{
	MediaElement *element = (MediaElement *) obj;

	delete current_state;
	current_state = new Value (element->IsAttached () ? element->state : element->detached_state, Type::MEDIAELEMENTSTATE);
	
	return current_state;
}

Value *
MediaElementPropertyValueProvider::GetPosition ()
{
	bool use_mplayer = false;;
	bool use_pause = false;
	MediaElement *element = (MediaElement *) obj;
	guint64 position = TimeSpan_ToPts (element->seek_to_position);
	
	delete this->position;
	this->position = NULL;
	
	switch (element->state) {
	case MediaElementStateIndividualizing:
	case MediaElementStateAcquiringLicense:
		g_warning ("MediaElementPropertyValueProvider::GetPosition (): Invalid state.\n");
		// Fall through
	case MediaElementStateOpening:
	case MediaElementStateClosed:
		use_mplayer = false;
		break;
	case MediaElementStateStopped:
		position = 0;
		break;
	case MediaElementStateBuffering:
	case MediaElementStatePlaying:
		use_mplayer = true;
		break;
	case MediaElementStatePaused:
		use_pause = true;
		break;
	}
	
	// If a seek is pending, we need to return that position.
	if (use_mplayer) {
		if (TimeSpan_FromPts (position) == -1)
			position = element->mplayer->GetPosition ();
	} else if (use_pause) {
		position = element->paused_position;
	}
	
	if (position < element->seeked_to_position)
		position = element->seeked_to_position;
	
	if (TimeSpan_FromPts (position) == -1) {
		position = 0;
	} else {
		position = MIN (position, element->mplayer->GetDuration ());
	}
		
	this->position = new Value (TimeSpan_FromPts (position), Type::TIMESPAN);
	return this->position;
}

};
