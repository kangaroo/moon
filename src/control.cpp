/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * control.cpp:
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

#include "rect.h"
#include "runtime.h"
#include "control.h"
#include "canvas.h"
#include "namescope.h"
#include "application.h"
#include "geometry.h"
#include "tabnavigationwalker.h"
#include "deployment.h"
#include "validators.h"
#include "style.h"

namespace Moonlight {

Control::Control ()
{
	SetObjectType (Type::CONTROL);

	default_style_applied = false;
	template_root = NULL;

	providers.isenabled = new InheritedIsEnabledValueProvider (this, PropertyPrecedence_IsEnabled);
}

Control::~Control ()
{
}

void
Control::FindElementsInHostCoordinates (cairo_t *cr, Point p, List *uielement_list)
{
	if (GetIsEnabled ())
		FrameworkElement::FindElementsInHostCoordinates (cr, p, uielement_list);
}

void
Control::HitTest (cairo_t *cr, Point p, List *uielement_list)
{
	if (GetIsEnabled ())
		FrameworkElement::HitTest (cr, p, uielement_list);
}

bool
Control::InsideObject (cairo_t *cr, double x, double y)
{
	/* 
	 * Controls don't get hit themselves the rendered elements 
	 * do and it bubbles up
	 */
	return false;
}

void
Control::OnPropertyChanged (PropertyChangedEventArgs *args, MoonError *error)
{
	if (args->GetProperty ()->GetOwnerType() != Type::CONTROL) {
		FrameworkElement::OnPropertyChanged (args, error);
		return;
	}

	if (args->GetId () == Control::TemplateProperty) {
		if (GetSubtreeObject ())
			ElementRemoved ((UIElement *) GetSubtreeObject ());
		InvalidateMeasure ();
	}
	else if (args->GetId () == Control::PaddingProperty
		 || args->GetId () == Control::BorderThicknessProperty) {
		InvalidateMeasure ();
	} else if (args->GetId () == Control::IsEnabledProperty) {
		if (!args->GetNewValue ()->AsBool ()) {
			Surface *surface = Deployment::GetCurrent ()->GetSurface ();
			if (surface && surface->GetFocusedElement () == this) {
				// Ensure this element loses focus, then try to focus the next suitable element
				surface->FocusElement (NULL);
				TabNavigationWalker::Focus (this, true);
			}
			ReleaseMouseCapture ();
		}
		args->ref (); // to counter the unref in Emit
		EmitAsync (IsEnabledChangedEvent, args);
	} else if (args->GetId () == Control::HorizontalContentAlignmentProperty
		   || args->GetId () == Control::VerticalContentAlignmentProperty) {
		InvalidateArrange ();
	}
	NotifyListenersOfPropertyChange (args, error);
}

void
Control::OnIsAttachedChanged (bool attached)
{
	FrameworkElement::OnIsAttachedChanged (attached);
	providers.isenabled->SetDataSource (GetLogicalParent ());
}


void
Control::OnIsLoadedChanged (bool loaded)
{
	if (loaded)
		ApplyDefaultStyle ();
	FrameworkElement::OnIsLoadedChanged (loaded);
}

void
Control::SetVisualParent (UIElement *visual_parent)
{
	if (GetVisualParent () != visual_parent) {
		FrameworkElement::SetVisualParent (visual_parent);
		providers.isenabled->SetDataSource (GetLogicalParent ());
 	}
}

void
Control::OnLogicalParentChanged (DependencyObject *old_parent, DependencyObject *new_parent)
{
	FrameworkElement::OnLogicalParentChanged  (old_parent, new_parent);
	providers.isenabled->SetDataSource (new_parent);
}

void
Control::Dispose ()
{
	template_root = NULL;
	FrameworkElement::Dispose ();
}

void
Control::ApplyDefaultStyle ()
{
	if (!default_style_applied) {
		Style *style = NULL;
		default_style_applied = true;
		ManagedTypeInfo *key = GetDefaultStyleKey ();

		if (key) {
			Application *app = Application::GetCurrent ();
			if (app)
				style = app->GetDefaultStyle (key);
			else
				g_warning ("Attempting to use a null Application applying default style.");
		}

		if (style) {
			MoonError e;
			DependencyProperty *style_prop = GetDeployment ()->GetTypes ()->GetProperty (FrameworkElement::StyleProperty);
			Value val (style);
			if (Validators::StyleValidator (this, style_prop, &val, &e))
				providers.defaultstyle->UpdateStyle (style, &e);
			else
				printf ("Error in the default style\n");
		}
	}
}

bool
Control::DoApplyTemplateWithError (MoonError *error)
{
	ControlTemplate *t = GetTemplate ();
	if (!t)
		return FrameworkElement::DoApplyTemplateWithError (error);

	// If the template expands to an element which is *not* a UIElement
	// we don't apply the template.
	DependencyObject *root = t->GetVisualTreeWithError (this, error);
	if (root && !root->Is (Type::UIELEMENT)) {
		g_warning ("Control::DoApplyTemplate () Template root was not a UIElement");
		root->unref ();
		root = NULL;
	}

	if (!root)
		return FrameworkElement::DoApplyTemplateWithError (error);

	// No need to ref template_root here as ElementAdded refs it
	// and it is cleared when ElementRemoved is called.
	if (template_root != root && template_root != NULL) {
		template_root->SetParent (NULL, NULL);
		template_root->SetMentor (NULL);
		MOON_CLEAR_FIELD (template_root);
	}

	MOON_SET_FIELD_NAMED (template_root, "TemplateRoot", (UIElement *)root);

	ElementAdded (template_root);

	if (IsLoaded ())
		GetDeployment ()->EmitLoadedAsync ();

	root->unref ();
	
	return true;
}


void
Control::UpdateIsEnabledSource (Control *control)
{
	providers.isenabled->SetDataSource (control);
}

void
Control::ElementAdded (UIElement *item)
{
	MoonError e;
	item->SetParent (this, &e);
	SetSubtreeObject (item);
	FrameworkElement::ElementAdded (item);
}

void
Control::ElementRemoved (UIElement *item)
{
	MoonError e;
	if (template_root != NULL) {
		template_root->SetParent (NULL, &e);
		template_root->SetMentor (NULL);
		MOON_CLEAR_FIELD_NAMED (template_root, "TemplateRoot");
	}
	item->SetParent (NULL, &e);
	FrameworkElement::ElementRemoved (item);
}

DependencyObject *
Control::GetTemplateChild (const char *name)
{
	if (template_root)
		return template_root->FindName (name);
	
	return NULL;
}

bool
Control::Focus (bool recurse)
{
	if (!IsAttached ())
		return false;
	
	 /* according to msdn, these three things must be true for an element to be focusable:
	 *
	 * 1. the element must be visible
	 * 2. the element must have IsTabStop = true
	 * 3. the element must be part of the plugin's visual tree, and must have had its Loaded event fired.
	 */
	 
	 /*
	 * If the current control is not focusable, we walk the visual tree and stop as soon
	 * as we find the first focusable child. That then becomes focused
	 */
	Types *types = GetDeployment ()->GetTypes ();
	Surface *surface = GetDeployment ()->GetSurface ();
	DeepTreeWalker walker (this);
	while (UIElement *e = walker.Step ()) {
		if (!types->IsSubclassOf (e->GetObjectType (), Type::CONTROL))
			continue;
		
		Control *c = (Control *)e;
		if (!c->GetIsEnabled ()) {
			if (!recurse)
				return false;

			walker.SkipBranch ();
			continue;
		}

		// A control is focusable if it is attached to a visual tree whose root
		// element has been loaded
		bool loaded = false;
		for (UIElement *check = this; !loaded && check != NULL; check = check->GetVisualParent ())
			loaded |= check->IsLoaded ();

		if (loaded && c->GetRenderVisible () && c->GetIsTabStop ())
			return surface->FocusElement (c);
		
		if (!recurse)
			return false;
	}
	return false;	
}

};
