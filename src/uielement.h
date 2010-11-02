/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * uielement.h
 *
 * Contact:
 *   Moonlight List (moonlight-list@lists.ximian.com)
 *
 * Copyright 2007-2009 Novell, Inc. (http://www.novell.com)
 *
 * See the LICENSE file included with the distribution for details.
 * 
 */

#ifndef __MOON_UIELEMENT_H__
#define __MOON_UIELEMENT_H__

#include <glib.h>

#include "dependencyobject.h"
#include "resources.h"
#include "point.h"
#include "rect.h"
#include "region.h"
#include "list.h"
#include "size.h"
#include "layoutinformation.h"
#include "context-cairo.h"

#define QUANTUM_ALPHA 1

#if QUANTUM_ALPHA
#define IS_TRANSLUCENT(x) (x * 255 < 254.5)
#define IS_INVISIBLE(x) (x * 255 < .5)
#else
#define IS_TRANSLUCENT(x) (x < 1.0)
#define IS_INVISIBLE(x) (x <= 0.0)
#endif

namespace Moonlight {

class Surface;

// return false to skip the subtree rooted at el
typedef bool (*VisualTreeVisitor)(UIElement *el, gpointer data);

/* @Namespace=System.Windows */
class UIElement : public DependencyObject {
public:
	/* @SkipFactories */
	UIElement ();
	virtual void Dispose ();
	
	int dirty_flags;
	List::Node *up_dirty_node;
	List::Node *down_dirty_node;

	bool force_invalidate_of_new_bounds;

	Region *dirty_region;

	int DumpHierarchy (UIElement *obj);

	enum UIElementFlags {
		NONE             = 0x00,

		// these two flags correspond to the 2 states of VisibilityProperty
		RENDER_VISIBLE   = 0x02,

		// the HitTestVisible property
		HIT_TEST_VISIBLE = 0x04,

		TOTAL_RENDER_VISIBLE   = 0x08,
		TOTAL_HIT_TEST_VISIBLE = 0x10,

		SHAPE_EMPTY      = 0x20,	// there's is nothing to draw, the cached path may be NULL
		SHAPE_NORMAL     = 0x40,	// normal drawing
		SHAPE_DEGENERATE = 0x80,	// degenerate drawing, use the Stroke brush for filling
		SHAPE_RADII      = 0x100,
		SHAPE_MASK       = (SHAPE_EMPTY | SHAPE_NORMAL | SHAPE_DEGENERATE | SHAPE_RADII),

		// These are flags which are propagated up the visual tree so that
		// the layout update pass knows which branches need processing.
		DIRTY_ARRANGE_HINT = 0x800,
		DIRTY_MEASURE_HINT = 0x1000,
		DIRTY_SIZE_HINT = 0x2000,

		// this flag is preset when projection effect should be used
		RENDER_PROJECTION = 0x4000,

		// these are flags which correspond to the intermediate buffer
		// stages necessary to render the element
		COMPOSITE_TRANSFORM = 0x8000,
		COMPOSITE_CLIP = 0x10000,
		COMPOSITE_EFFECT = 0x20000,
		COMPOSITE_OPACITY = 0x40000,
		COMPOSITE_OPACITY_MASK = 0x80000,
		COMPOSITE_CACHE = 0x100000,
		COMPOSITE_MASK = (COMPOSITE_TRANSFORM | COMPOSITE_CLIP | COMPOSITE_EFFECT | COMPOSITE_OPACITY | COMPOSITE_OPACITY_MASK | COMPOSITE_CACHE)
	};
	
	virtual TimeManager *GetTimeManager ();

	virtual bool PermitsMultipleParents () { return false; }

	virtual void SetVisualParent (UIElement *visual_parent);
	/* @GeneratePInvoke */
	UIElement *GetVisualParent () { return visual_parent; }

	int GetVisualLevel () { return visual_level; }
	void SetVisualLevel (int level) { visual_level = level; }

	/* @GeneratePInvoke,ManagedAccess=Internal */
	virtual void SetSubtreeObject (DependencyObject *value);

	/* @GeneratePInvoke,ManagedAccess=Internal */
	virtual DependencyObject *GetSubtreeObject () { return subtree_object; }

	/* @GeneratePInvoke */
	virtual void ElementAdded (UIElement *obj);
	/* @GeneratePInvoke */
	virtual void ElementRemoved (UIElement *obj);
	
	virtual bool EnableAntiAlias() { return true; }

	virtual void OnIsAttachedChanged (bool value);

	// UpdateTotalRenderVisibility:
	//   Updates the opacity and render visibility on this item based on 
	//   its parent's opacity and visibility as well as the value of its 
	//   OpacityProperty and RenderVisibilityProperty.
	//
	void UpdateTotalRenderVisibility ();
	void ComputeTotalRenderVisibility ();
	bool GetActualTotalRenderVisibility ();

	//
	// GetRenderVisible:
	//   Returns true if the Visibility property of this item is "Visible", and false otherwise
	//
	bool GetRenderVisible () { return (flags & UIElement::TOTAL_RENDER_VISIBLE) != 0; }


	// UpdateTotalHitTestVisibility:
	//   Updates the hit testability of the item based on the you know..
	//   The hit test flag.
	void UpdateTotalHitTestVisibility ();
	void ComputeTotalHitTestVisibility ();
	bool GetActualTotalHitTestVisibility ();

	//
	// GetHitTestVisible:
	//   Returns true if the IsHitTestVisible property of this item true, and false otherwise
	//
	bool GetHitTestVisible () { return (flags & UIElement::TOTAL_HIT_TEST_VISIBLE) != 0; }
	
	//
	// UpdateTransform:
	//   Updates the absolute_xform for this item
	//
	void UpdateTransform ();
	void ComputeLocalTransform ();
	void ComputeTransform ();
	virtual void TransformBounds (cairo_matrix_t *old, cairo_matrix_t *current);

	//
	// UpdateProjection:
	//   Updates the projection for this item
	//
	void UpdateProjection ();
	void ComputeLocalProjection ();

	//
	// IsLoaded:
	//   Returns true if the element has been attached to a
	//   surface and is part of the visual hierarchy.
	//
	bool IsLoaded () { return loaded; }
	void SetIsLoaded (bool value);
	virtual void OnIsLoadedChanged (bool loaded);

	void ClearFlag (UIElementFlags flag) { flags &= ~flag; }
	bool HasFlag (UIElementFlags flag) { return (flags & flag) == flag; }
	void SetFlag (UIElementFlags flag) { flags |= flag; }
	void PropagateFlagUp (UIElementFlags flag);

	//
	// Render: 
	//   Renders the given @item on the @surface.  the area that is
	//   exposed is delimited by x, y, width, height
	//
	virtual void Render (cairo_t *cr, Region *region, bool path_only = false);

	//
	// Paint:
	//   Do an optimized render pass on the this element and it's
	//   subtree.
	//
	void Paint (CairoSurface *target, Rect bounds, cairo_matrix_t *matrix);

	// a non virtual method for use when we want to wrap render
	// with debugging and/or timing info
	void FrontToBack (Region *surface_region, List *render_list);
	void DoRender (Context *ctx, Region *region);
	bool UseOcclusionCulling ();
	bool RenderToIntermediate ();

	//
	// GetSizeForBrush:
	//   Gets the size of the area to be painted by a Brush (needed for image/video scaling)
	virtual void GetSizeForBrush (cairo_t *cr, double *width, double *height);

	//
	// GetOriginPoint:
	//   Gets real origin, required e.g. for lineargradientbrushes on Path
	virtual Point GetOriginPoint ()
	{
		return Point (0.0, 0.0);
	}

	//
	// ShiftPosition:
	//
	//   This method should actually set the x/y of the bounds
	//   rectangle to the new position.  It is virtual to allow
	//   subclasses with specialized bounds (Panel, InkPresenter)
	//   the opportunity to set those bounds' positions as well.
	//
	virtual void ShiftPosition (Point p);

	//
	// ComputeComposite:
	//
	//   This method updates the composite effect and flags used
	//   for rendering to intermediate buffers.
	//
	void ComputeComposite ();

	//
	// UpdateBounds:
	//   Recomputes the bounds of this element, and if they're
	//   different chains up to its parent telling it to update
	//   its bounds.
	//
	void UpdateBounds (bool force_redraw_of_new_bounds = false);
	// 
	// ComputeBounds:
	//   Updates the bounding box for the given item, this uses the parent
	//   chain to compute the composite affine.
	//
	// Output:
	//   item->bounds is updated
	// 
	virtual void ComputeBounds ();

	// 
	// ComputeGlobalBounds:
	//   Updates the global bounding box for the given item.
	//
	// Output:
	//   item->global_bounds is updated
	//
	virtual void ComputeGlobalBounds ();

	//
	// ComputeSurfaceBounds:
	//   Updates the surface bounding box for the given item, this uses
	//   the parent chain to compute the composite projection.
	//
	// Output:
	//   item->surface_bounds is updated
	//
	virtual void ComputeSurfaceBounds ();


	//
	// GetBounds:
	//   returns the current bounding box for the given item in surface 
	//   coordinates.
	// 
	Rect GetBounds () { return surface_bounds; }

	// 
	// GetSubtreeBounds:
	//   returns the bounding box including all sub-uielements.
	//   implemented by containers in surface coordinates.
	// 
	virtual Rect GetSubtreeBounds () { return surface_bounds; }

	//
	// GetRenderBounds:
	// returns the bounding box to be rendered, which
	// unfortunately isn't necessarily the same as either our
	// bounds or subtree bounds (in the case of inkpresenter)
	virtual Rect GetRenderBounds () { return bounds; }

	//
	// GetCoverageBounds:
	// returns the bounding box in global coordinates that opaquely covered by this object
	//
	virtual Rect GetCoverageBounds () { return Rect (); }

	//
	// GetLocalBounds:
	//   returns the bounding box including all sub-uielements.
	//   implemented by containers in local projection coordinates.
	//
	virtual Rect GetLocalBounds () { return bounds; }

	//
	// GetGlobalBounds:
	//   returns the bounding box including all sub-uielements.
	//   implemented by containers in parent coordinates.
	//
	virtual Rect GetGlobalBounds () { return global_bounds; }

	//
	// GetSubtreeExtents:
	//   returns the extents including all sub-uielements.
	//   implemented by containers in local coordinates.
	//
	virtual Rect GetSubtreeExtents () { return extents; }


	//
	// IsLayoutContainer:
	//   returns true if the container has children that require a measure
	//   pass.
	virtual bool IsLayoutContainer () { return GetSubtreeObject () != NULL; }
	virtual bool IsContainer () { return IsLayoutContainer (); }

	// HitTest

	//   Accumulate a list of all elements that will contain the
	//   point (or intersect the rectangle). The first node in the
	//   list is the most deeply nested node, the last node is the
	//   root.
	virtual void HitTest (cairo_t *cr, Point p, List *uielement_list);
	virtual void HitTest (cairo_t *cr, Rect r, List *uielement_list);
	
	/* @GeneratePInvoke */
	void FindElementsInHostCoordinates_p (Point p, HitTestCollection *uielement_list);
	/* @GeneratePInvoke */
	void FindElementsInHostCoordinates_r (Rect p, HitTestCollection *uielement_list);
	
	virtual bool CanFindElement () { return false; }
	virtual void FindElementsInHostCoordinates (cairo_t *cr, Point P, List *uielement_list);
	virtual void FindElementsInHostCoordinates (cairo_t *cr, Rect r, List *uielement_list);
	
	//
	// Recomputes the bounding box, requests redraws, 
	// the parameter determines if we should also update the transformation
	//
	void FullInvalidate (bool render_xform);

	//
	// InsideObject:
	//   Returns whether the position x, y is inside the object
	//
	virtual bool InsideObject (cairo_t *cr, double x, double y);
	
	//
	// Checks if the point is inside the Clip region.
	// Returns true if no Clip region is defined
	// (which is actually an infinitely big Clip region).
	// 
	bool InsideClip (cairo_t *cr, double x, double y);

	//
	// Invalidates a subrectangle of this element
	//
	void Invalidate (Rect r);

	//
	// Invalidates a subregion of this element
	void Invalidate (Region *region);
	
	//
	// Invalidates the entire bounding rectangle of this element
	//
	void Invalidate ();

	// 
	// Invalidates the paint region of the element and its subtree
	//
	void InvalidateSubtreePaint ();
	void InvalidateMask ();
	void InvalidateClip ();
	void InvalidateVisibility ();
	void InvalidateEffect ();
	void InvalidateBitmapCache ();
	void InvalidateCacheMode ();

	//
	// GetTransformOrigin:
	//   Returns the transformation origin based on  of the item and the
	//   xform_origin
	virtual Point GetTransformOrigin () {
		return Point (0, 0);
	}

	//
	// EmitKeyDown:
	//
	bool EmitKeyDown (MoonKeyEvent *key);

	//
	// EmitKeyUp:
	//
	bool EmitKeyUp (MoonKeyEvent *key);

	//
	// EmitGotFocus:
	//   Invoked when the mouse focuses the given object
	//
	bool EmitGotFocus ();

	//
	// EmitLostFocus:
	//   Invoked when the given object loses mouse focus
	//
	bool EmitLostFocus ();
	
	//
	// EmitLostMouseCapture:
	//   Invoked when the given object loses mouse capture
	//
	bool EmitLostMouseCapture ();
	
	//
	// CaptureMouse:
	//
	//    Attempts to capture the mouse.  If successful, all mouse
	//    events will be transmitted directly to this element.
	//    Leave/Enter events will no longer be sent.
	//
	/* @GeneratePInvoke,GenerateJSBinding */
	bool CaptureMouse ();
	virtual bool CanCaptureMouse () { return true; }
	
	/* @GeneratePInvoke */
	virtual bool Focus (bool recurse = true);
	
	//
	// ReleaseMouseCapture:
	//
	//    Attempts to release the mouse.  If successful, any
	//    applicable Leave/Enter events for the current mouse
	//    position will be sent.
	//
	/* @GeneratePInvoke,GenerateJSBinding */
	void ReleaseMouseCapture ();

	virtual int AddHandler (int event_id, EventHandler handler, gpointer data, GDestroyNotify data_dtor = NULL, bool managed_data_dtor = false, bool handledEventsToo = false);
	virtual int RemoveHandler (int event_id, EventHandler handler, gpointer data);
	virtual void RemoveHandler (int event_id, int token);

	virtual void OnPropertyChanged (PropertyChangedEventArgs *args, MoonError *error);
	virtual void OnSubPropertyChanged (DependencyProperty *prop, DependencyObject *obj, PropertyChangedEventArgs *subobj_args);
	virtual void OnCollectionChanged (Collection *col, CollectionChangedEventArgs *args);

	//
	// CacheInvalidateHint:
	//   Give a hint to this UIElement that it should free any possible
	//   cached (mem-intensive) data it has. This ie. can happen when the
	//   element is removed from a collection, becomes invisible, etc.
	//
	virtual void CacheInvalidateHint ();
	
	//
	// 2.0 methods
	//

	// Layout foo

	void DoMeasureWithError (MoonError *error);
	void DoArrangeWithError (MoonError *error);

	/* @GeneratePInvoke */
	virtual void MeasureWithError (Size availableSize, MoonError *error) = 0;
	/* @GeneratePInvoke */
	virtual void ArrangeWithError (Rect finalRect, MoonError *error) = 0;
	/* @GeneratePInvoke */
	void InvalidateMeasure ();
	/* @GeneratePInvoke */
	void InvalidateArrange ();

	/* @GeneratePInvoke,GenerateJSBinding=UpdateLayout */
	virtual void UpdateLayoutWithError (MoonError *error) = 0;
	virtual void UpdateLayer (LayoutPass *pass, MoonError *error) = 0;

	/* @GeneratePInvoke */
	Size GetDesiredSize () { return desired_size; }
	
	/* @GeneratePInvoke */
	Size GetRenderSize () { return render_size; }

	/* @GeneratePInvoke,GenerateJSBinding=TransformToVisual,JSBinding_ReturnValueIsReffed */
	/* This method always returns a reffed object */
	GeneralTransform *GetTransformToUIElementWithError (UIElement *to_element, MoonError *error);

	//
	// TransformPoint:
	//
	// Maps the point to the coordinate space of this item
	//
	void TransformPoint (double *x, double *y);
	
 	/* @PropertyType=Geometry,GenerateAccessors */
	const static int ClipProperty;
 	/* @PropertyType=CacheMode,GenerateAccessors */
	const static int CacheModeProperty;
 	/* @PropertyType=Effect,GenerateAccessors */
	const static int EffectProperty;
 	/* @PropertyType=Projection,GenerateAccessors */
	const static int ProjectionProperty;
 	/* @PropertyType=bool,DefaultValue=true,GenerateAccessors */
	const static int IsHitTestVisibleProperty;
 	/* @PropertyType=Brush,GenerateAccessors */
	const static int OpacityMaskProperty;
 	/* @PropertyType=double,DefaultValue=1.0,GenerateAccessors */
	const static int OpacityProperty;
 	/* @PropertyType=Point,DefaultValue=Point (0\,0),GenerateAccessors */
	const static int RenderTransformOriginProperty;
	/* @PropertyType=Transform,GenerateAccessors,GenerateManagedAccessors=false */
	const static int RenderTransformProperty;
 	/* @PropertyType=Visibility,DefaultValue=VisibilityVisible,GenerateAccessors */
	const static int VisibilityProperty;
	/* @PropertyType=bool,DefaultValue=true,GenerateAccessors */
	const static int UseLayoutRoundingProperty;
	/* @PropertyType=bool,DefaultValue=false,GenerateAccessors */
	const static int AllowDropProperty;

	// in 2.0 these properties are actually in FrameworkElement
 	/* @PropertyType=CursorType,DefaultValue=CursorTypeDefault,ManagedDeclaringType=FrameworkElement,ManagedPropertyType=Cursor,GenerateAccessors,Validator=CursorValidator,Coercer=UIElement::CoerceCursor */
	const static int CursorProperty;
 	/* @PropertyType=ResourceDictionary,ManagedDeclaringType=FrameworkElement,AutoCreateValue,ManagedFieldAccess=Internal,GenerateAccessors */
	const static int ResourcesProperty;
 	/* @PropertyType=object,ManagedDeclaringType=FrameworkElement,ManagedPropertyType=object,IsCustom=true */
	const static int TagProperty;
 	/* @PropertyType=TriggerCollection,ManagedDeclaringType=FrameworkElement,AutoCreateValue,ManagedFieldAccess=Internal,ManagedSetterAccess=Internal,GenerateAccessors */
	const static int TriggersProperty;
	
	//
	// Property Accessors

	//
	void SetClip (Geometry *clip);
	Geometry *GetClip ();

	void SetCacheMode (CacheMode *mode);
	CacheMode *GetCacheMode ();

	CursorType GetCursor ();
	void SetCursor (CursorType value);

	Effect* GetEffect ();
	void SetEffect (Effect *value);

	Projection* GetProjection ();
	void SetProjection (Projection *value);

	void SetIsHitTestVisible (bool visible);
	bool GetIsHitTestVisible ();
	
	void SetOpacityMask (Brush *mask);
	Brush *GetOpacityMask ();
	
	void SetOpacity (double opacity);
	double GetOpacity ();
	
	void SetRenderTransform (Transform *transform);
	Transform *GetRenderTransform ();
	
	void SetRenderTransformOrigin (Point *origin);
	Point *GetRenderTransformOrigin ();

	ResourceDictionary* GetResources();
	void SetResources (ResourceDictionary *value);

	TriggerCollection *GetTriggers ();
	void SetTriggers (TriggerCollection* value);
	
	Visibility GetVisibility ();
	void SetVisibility (Visibility value);

	bool GetUseLayoutRounding ();
	void SetUseLayoutRounding (bool value);

	bool GetAllowDrop ();
	void SetAllowDrop (bool value);

	// Events you can AddHandler to

	/* @ManagedDeclaringType=FrameworkElement,DelegateType=RoutedEventHandler,GenerateManagedEventField=true */
	const static int LoadedEvent;
	/* @ManagedDeclaringType=FrameworkElement,DelegateType=RoutedEventHandler */
	const static int UnloadedEvent;
	/* @DelegateType=MouseEventHandler */
	const static int MouseMoveEvent;
	/* @DelegateType=MouseButtonEventHandler,GenerateManagedEventField=true */
	const static int MouseLeftButtonDownEvent;
	/* @DelegateType=MouseButtonEventHandler,GenerateManagedEventField=true */
	const static int MouseLeftButtonUpEvent;
	/* @DelegateType=MouseButtonEventHandler */
	const static int MouseRightButtonDownEvent;
	/* @DelegateType=MouseButtonEventHandler */
	const static int MouseRightButtonUpEvent;
	/* @DelegateType=KeyEventHandler,GenerateManagedEventField=true */
	const static int KeyDownEvent;
	/* @DelegateType=KeyEventHandler,GenerateManagedEventField=true */
	const static int KeyUpEvent;
	/* @DelegateType=MouseEventHandler */
	const static int MouseEnterEvent;
	/* @DelegateType=MouseEventHandler */
	const static int MouseLeaveEvent;
	/* @GenerateManagedEvent=false */
	const static int InvalidatedEvent;
	/* @DelegateType=RoutedEventHandler */
	const static int GotFocusEvent;
	/* @DelegateType=RoutedEventHandler */
	const static int LostFocusEvent;
	/* @DelegateType=MouseEventHandler */
	const static int LostMouseCaptureEvent;
	/* @DelegateType=MouseWheelEventHandler,GenerateManagedEventField=true */
	const static int MouseWheelEvent;
	/* @DelegateType=TextCompositionEventHandler,GenerateManagedEventField=true */
	const static int TextInputEvent;
	/* @DelegateType=TextCompositionEventHandler,GenerateManagedEventField=true */
	const static int TextInputStartEvent;
	/* @DelegateType=TextCompositionEventHandler,GenerateManagedEventField=true */
	const static int TextInputUpdateEvent;
	/* @DelegateType=DragEventHandler */
	const static int DragEnterEvent;
	/* @DelegateType=DragEventHandler */
	const static int DragLeaveEvent;
	/* @DelegateType=DragEventHandler */
	const static int DragOverEvent;
	/* @DelegateType=DragEventHandler */
	const static int DropEvent;
	/* @DelegateType=EventHandler<ManipulationStartedEventArgs>,GenerateManagedEventField=true,GenerateManagedEvent=false */
	const static int ManipulationStartedEvent;
	/* @DelegateType=EventHandler<ManipulationDeltaEventArgs>,GenerateManagedEventField=true,GenerateManagedEvent=false */
	const static int ManipulationDeltaEvent;
	/* @DelegateType=EventHandler<ManipulationCompletedEventArgs>,GenerateManagedEventField=true,GenerateManagedEvent=false */
	const static int ManipulationCompletedEvent;

	// these we turn off generation for and handle manually since
	// they're desktop-only

	/* @GenerateManagedEvent=false */
	const static int MouseLeftButtonMultiClickEvent;
	
	// Helper method which checks recursively checks this element and its visual
	// parents to see if any are loaded.
	static bool IsSubtreeLoaded (UIElement *element);

	static bool CoerceCursor (DependencyObject *obj, DependencyProperty *p, Value *value, Value **coerced, MoonError *error);

	// Helper methods for properties that should be ignored when runtime
	// flags are not present
	Effect *GetRenderEffect ();
	Projection *GetRenderProjection ();
	CacheMode *GetRenderCacheMode ();

protected:
	virtual ~UIElement ();
	/* @SkipFactories */
	UIElement (Type::Kind object_type);
	Rect IntersectBoundsWithClipPath (Rect bounds, bool transform);
	void RenderClipPath (cairo_t *cr, bool path_only = false);

	void SetDesiredSize (Size s) { desired_size = s; }
	void SetRenderSize (Size s) { render_size = s; }

	// The computed bounding box
	Size hidden_desire;
	Rect bounds;
	Rect global_bounds;
	Rect surface_bounds;
	Rect extents;

	int flags;

	// Absolute affine transform, precomputed with all of its data
	cairo_matrix_t absolute_xform;
	cairo_matrix_t layout_xform;

	virtual void PreRender (Context *ctx, Region *region, bool skip_children);
	virtual void PostRender (Context *ctx, Region *region, bool skip_children);

	static void CallPreRender (Context *ctx, UIElement *element, Region *region, bool skip_children);
	static void CallPostRender (Context *ctx, UIElement *element, Region *region, bool skip_children);

	// Local perspective transform, including local affine transforms
	double local_projection[16];

	// Absolute perspective transform, precomputed with all of its data
	double absolute_projection[16];

	Thickness effect_padding;

	MoonSurface *bitmap_cache;

private:
	bool loaded;
	int visual_level;
	WeakRef<UIElement> visual_parent;
	WeakRef<DependencyObject> subtree_object;
	double total_opacity;
	Brush *opacityMask;
	Size desired_size;
	Size render_size;

	// The local render transform including tranform origin
	cairo_matrix_t local_xform;
	cairo_matrix_t render_xform;
	cairo_matrix_t cache_xform;

	// Local perspective transform, including inverse cache transform
	double render_projection[16];

	void Init ();
};

};

#endif /* __MOON_UIELEMENT_H__ */
