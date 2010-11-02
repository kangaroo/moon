//
// Contact:
//   Moonlight List (moonlight-list@lists.ximian.com)
//
// Copyright (c) 2008 Novell, Inc (http://www.novell.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

using System;
using System.Windows;
using System.Windows.Automation;
using System.Windows.Automation.Peers;
using System.Windows.Controls.Primitives;
using System.Windows.Input;
using System.Windows.Media;
using System.Collections.Specialized;

namespace System.Windows.Controls
{
	[TemplateVisualState (Name="FocusedDropDown", GroupName="FocusStates")]
	[TemplateVisualState (Name="Normal", GroupName="CommonStates")]
	[TemplateVisualState (Name="MouseOver", GroupName="CommonStates")]
	[TemplateVisualState (Name="Disabled", GroupName="CommonStates")]
	[TemplateVisualState (Name="Unfocused", GroupName="FocusStates")]
	[TemplateVisualState (Name="Focused", GroupName="FocusStates")]
	[TemplateVisualState (Name="InvalidFocused", GroupName="ValidationStates")]
	[TemplateVisualState (Name="InvalidUnfocused", GroupName="ValidationStates")]
	[TemplateVisualState (Name="Valid", GroupName="ValidationStates")]
	[TemplatePart (Name="ContentPresenterBorder", Type=typeof(FrameworkElement))]
	[TemplatePart (Name="ContentPresenter", Type=typeof(ContentPresenter))]
	[TemplatePart (Name="DropDownToggle", Type=typeof(ToggleButton))]
	[TemplatePart (Name="ScrollViewer", Type=typeof(ScrollViewer))]
	[TemplatePart (Name="Popup", Type=typeof(Popup))]
	public class ComboBox : Selector
	{
		public static readonly DependencyProperty IsDropDownOpenProperty = 
			DependencyProperty.RegisterCore ("IsDropDownOpen", typeof (bool), typeof (ComboBox),
						     new PropertyMetadata (null, delegate (DependencyObject sender, DependencyPropertyChangedEventArgs e) {
								     ((ComboBox) sender).IsDropDownOpenChanged (sender, e);
							     }));

		public new static readonly DependencyProperty IsSelectionActiveProperty = Selector.IsSelectionActiveProperty;

		// ComboBox really does ignore property changes here 
		public static readonly DependencyProperty ItemContainerStyleProperty =
			DependencyProperty.RegisterCore ("ItemContainerStyle", typeof (Style), typeof (ComboBox),
		                                 new PropertyMetadata (ItemContainerStyleChanged));

		public static readonly DependencyProperty MaxDropDownHeightProperty =
			DependencyProperty.RegisterCore ("MaxDropDownHeight", typeof (double), typeof (ComboBox),
						     new PropertyMetadata (double.PositiveInfinity, delegate (DependencyObject sender, DependencyPropertyChangedEventArgs e) {
								     ((ComboBox) sender).MaxDropDownHeightChanged (sender, e);
							     }));
		

		public event EventHandler DropDownClosed;
		public event EventHandler DropDownOpened;

		ComboBoxItem DisplayedItem {
			get; set;
		}
		
		object NothingSelectedFallback {
			get; set;
		}
		
		public bool IsDropDownOpen {
			get { return (bool) GetValue (IsDropDownOpenProperty); }
			set { SetValue (IsDropDownOpenProperty, value); }
		}
		
		public bool IsEditable {
			get;
			internal set;
		}
		
		public bool IsSelectionBoxHighlighted {
			get { return !IsDropDownOpen && FocusManager.GetFocusedElement () == this; }
		}
		
		public Style ItemContainerStyle {
			get { return (Style) GetValue (ItemContainerStyleProperty); }
			set { SetValue (ItemContainerStyleProperty, value); }
		}
		
		int FocusedIndex {
			get; set;
		}
		
		public double MaxDropDownHeight {
			get { return (double) GetValue (MaxDropDownHeightProperty); }
			set { SetValue (MaxDropDownHeightProperty, value); }
		}

		public object SelectionBoxItem {
			get; private set;
		}
		
		public DataTemplate SelectionBoxItemTemplate {
			get; private set;
		}
		
		public ComboBox ()
		{
			DefaultStyleKey = typeof (ComboBox);

			SelectionChanged += delegate {
				if (!IsDropDownOpen)
					UpdateDisplayedItem (SelectedItem);
			};
		}

		#region Property Changed Handlers
		
		void IsDropDownOpenChanged (DependencyObject sender, DependencyPropertyChangedEventArgs e)
		{
			bool open = (bool) e.NewValue;
			if (_popup != null)
				_popup.IsOpen = open;
			if (_dropDownToggle != null)
				_dropDownToggle.IsChecked = open;
			
			if (open) {
				ComboBoxItem t = null;
				FocusedIndex = Items.Count > 0 ? Math.Max (SelectedIndex, 0) : -1;
				if (FocusedIndex > -1)
					t = ItemContainerGenerator.ContainerFromIndex (FocusedIndex) as ComboBoxItem;

				// If the ItemsPresenter hasn't attached yet 't' will be null.
				// When the itemsPresenter attaches, focus will be set when the
				// item is loaded
				if (t != null)
					t.Focus ();

				LayoutUpdated += UpdatePopupSizeAndPosition;

				UpdateLayout ();

				OnDropDownOpened (EventArgs.Empty);

				// Raises UIA Event
				AutomationPeer peer = ((ComboBox) sender).AutomationPeer;
				if (peer != null)
					peer.RaisePropertyChangedEvent (ExpandCollapsePatternIdentifiers.ExpandCollapseStateProperty,
					                                ExpandCollapseState.Collapsed,
									ExpandCollapseState.Expanded);
			} else {
				Focus ();

				LayoutUpdated -= UpdatePopupSizeAndPosition;

				OnDropDownClosed (EventArgs.Empty);

				// Raises UIA Event
				AutomationPeer peer = ((ComboBox) sender).AutomationPeer;
				if (peer != null)
					peer.RaisePropertyChangedEvent (ExpandCollapsePatternIdentifiers.ExpandCollapseStateProperty,
					                                ExpandCollapseState.Expanded,
									ExpandCollapseState.Collapsed);
			}

			UpdateDisplayedItem (open && SelectedItem is UIElement ? null : SelectedItem);
			UpdateVisualState (true);
		}

		void MaxDropDownHeightChanged (DependencyObject sender, DependencyPropertyChangedEventArgs e)
		{
			UpdatePopupMaxHeight ((double) e.NewValue);
		}

		#endregion
		

		void UpdateVisualState (bool useTransitions)
		{
			if (!IsEnabled) {
				VisualStateManager.GoToState (this, "Disabled", useTransitions);
			}
			else if (isMouseOver) {
				VisualStateManager.GoToState (this, "MouseOver", useTransitions);
			}
			else {
				VisualStateManager.GoToState (this, "Normal", useTransitions);
			}

			if (IsDropDownOpen && IsEnabled) {
				VisualStateManager.GoToState (this, "FocusedDropDown", useTransitions);
			}
			else if (Focused && IsEnabled) {
				VisualStateManager.GoToState (this, "Focused", useTransitions);
			}
			else {
				VisualStateManager.GoToState (this, "Unfocused", useTransitions);
			}
		}

		protected virtual void OnDropDownClosed (EventArgs e)
		{
			EventHandler h = DropDownClosed;
			if (h != null)
				h (this, e);
		}
		
		protected virtual void OnDropDownOpened (EventArgs e)
		{
			EventHandler h = DropDownOpened;
			if (h != null)
				h (this, e);
		}

		protected override Size ArrangeOverride (Size arrangeBounds)
		{
			return base.ArrangeOverride (arrangeBounds);
		}

		internal override void InvokeIsEnabledPropertyChanged ()
		{
			if (!IsEnabled)
				IsDropDownOpen = false;
			base.InvokeIsEnabledPropertyChanged ();
		}

		protected override DependencyObject GetContainerForItemOverride ()
		{
			return new ComboBoxItem ();
		}
		
		protected override bool IsItemItsOwnContainerOverride (object item)
		{
			return item is ComboBoxItem;
		}

		protected override void PrepareContainerForItemOverride (DependencyObject element, object item)
		{
			base.PrepareContainerForItemOverride (element, item);
			if (element != item) {
				ListBoxItem cb = (ListBoxItem) element;
				if (cb.Style == null && ItemContainerStyle != null)
					cb.Style = ItemContainerStyle;
			}
		}

		public override void OnApplyTemplate ()
		{
			base.OnApplyTemplate ();

			UpdateVisualState (false);

			IsDropDownOpen = false;
			
			_contentPresenter = GetTemplateChild ("ContentPresenter") as ContentPresenter;
			_popup = GetTemplateChild ("Popup") as Popup;
			_dropDownToggle = GetTemplateChild ("DropDownToggle") as ToggleButton;

			if (_contentPresenter != null) {
				NothingSelectedFallback = _contentPresenter.Content;
			}

			if (_popup != null) {
				UpdatePopupMaxHeight (MaxDropDownHeight);
				_popup.CatchClickedOutside ();
				_popup.ClickedOutside += delegate { 
					IsDropDownOpen = false; 
				};

				// The popup will never receive a key press event so we need to chain the event
				// using Popup.Child
				if (_popup.Child != null) {
					_popup.Child.KeyDown += delegate(object sender, KeyEventArgs e) {
						OnKeyDown (e);
					};
					((FrameworkElement) _popup.RealChild).SizeChanged += UpdatePopupSizeAndPosition;
				}
			}
			if (_dropDownToggle != null) {
				_dropDownToggle.Checked += delegate {
						IsDropDownOpen = true;
				};
				_dropDownToggle.Unchecked += delegate {
						IsDropDownOpen = false;
				};
			}
			
			UpdateVisualState (false);
			UpdateDisplayedItem (SelectedItem);
		}
		
		protected override AutomationPeer OnCreateAutomationPeer ()
		{
			return new ComboBoxAutomationPeer (this);
		}
		
		protected override void OnGotFocus (RoutedEventArgs e)
		{
			base.OnGotFocus (e);
			UpdateVisualState (true);
		}

		internal override void OnItemContainerStyleChanged (Style oldStyle, Style newStyle)
		{
			int count = Items.Count;
			for (int i = 0; i < count; i++) {
				var item = Items [i];
				var container = (ListBoxItem) ItemContainerGenerator.ContainerFromIndex (i);
				if (container != null && item != container)
					container.Style = newStyle;
			}
		}

		protected override void OnLostFocus (RoutedEventArgs e)
		{
			base.OnLostFocus (e);
			IsSelectionActive = _popup == null ? false : _popup.IsOpen;
			UpdateVisualState (true);
		}
		
		protected override void OnMouseEnter (MouseEventArgs e)
		{
			base.OnMouseEnter(e);
			isMouseOver = true;
			UpdateVisualState (true);
		}
		
		protected override void OnMouseLeave (MouseEventArgs e)
		{
			base.OnMouseLeave(e);
			isMouseOver = false;
			UpdateVisualState (true);
		}
		
		protected override void OnMouseLeftButtonDown (MouseButtonEventArgs e)
		{
			base.OnMouseLeftButtonDown (e);
			if (!e.Handled) {
				e.Handled = true;
				IsSelectionActive = true;
				IsDropDownOpen = !IsDropDownOpen;
			}
		}

		protected override void OnMouseWheel (MouseWheelEventArgs e)
		{
			base.OnMouseWheel (e);
		}

		protected override void OnKeyDown (KeyEventArgs e)
		{
			base.OnKeyDown (e);
			if (!e.Handled) {
				var key = e.Key;
				if (FlowDirection == FlowDirection.RightToLeft) {
					if (key == Key.Left)
						key = Key.Right;
					else if (key == Key.Right)
						key = Key.Left;
				}

				e.Handled = true;
				switch (key) {
				case Key.Escape:
					IsDropDownOpen = false;
					break;
					
				case Key.Enter:
				case Key.Space:
					if (IsDropDownOpen && FocusedIndex != SelectedIndex) {
						SelectedIndex = FocusedIndex;
						IsDropDownOpen = false;
					} else {
						IsDropDownOpen = true;
					}
					break;

				case Key.Right:
				case Key.Down:
					if (IsDropDownOpen) {
						if (FocusedIndex < Items.Count - 1) {
							FocusedIndex ++;
							((Control) ItemContainerGenerator.ContainerFromIndex (FocusedIndex)).Focus ();
						}
					} else {
						SelectedIndex = Math.Min (SelectedIndex + 1, Items.Count - 1);
					}
					break;

				case Key.Left:
				case Key.Up:
					if (IsDropDownOpen) {
						if (FocusedIndex > 0) {
							FocusedIndex --;
							((Control) ItemContainerGenerator.ContainerFromIndex (FocusedIndex)).Focus ();
						}
					} else if (SelectedIndex != -1) {
						SelectedIndex = Math.Max (SelectedIndex - 1, 0);
					}
					break;
					
				default:
					e.Handled = false;
					break;
				}
			} else {
				Console.WriteLine ("Already handled");
			}
		}

		void UpdateDisplayedItem (object selectedItem)
		{
			object content;

			// Can't do anything with no content presenter
			if (_contentPresenter == null)
				return;

			// Return the currently displayed object (which is a UIElement)
			// to its original container.
			if (DisplayedItem != null) {
				content = _contentPresenter.Content;
				DisplayedItem.Content = content;
				DisplayedItem = null;
			}
			_contentPresenter.Content = null;

			if (selectedItem == null) {
				_contentPresenter.Content = NothingSelectedFallback;
				_contentPresenter.ContentTemplate = null;
				SelectionBoxItem = null;
				SelectionBoxItemTemplate = null;
				return;
			}

			// If the currently selected item is a ComboBoxItem (not ListBoxItem!), we
			// display its Content instead of the CBI itself.
			content = selectedItem;
			if (content is ComboBoxItem)
				content = ((ComboBoxItem) content).Content;

			// Only allow DisplayedItem to be non-null if we physically move
			// its content. This will only happen if DisplayedItem == SelectedItem
			DisplayedItem = ItemContainerGenerator.ContainerFromIndex (SelectedIndex) as ComboBoxItem;

			SelectionBoxItem = content;
			SelectionBoxItemTemplate = ItemTemplate;

			// If displayed item is avaiable, we can get the right template from there. Otherwise
			// we need to create a container, read the template and destroy it.
			if (DisplayedItem != null) {
				SelectionBoxItemTemplate = DisplayedItem.ContentTemplate;
				if (content is UIElement)
					DisplayedItem.Content = null;
				else
					DisplayedItem = null;
			} else {
				bool fresh;
				ComboBoxItem container = ItemContainerGenerator.ContainerFromIndex (SelectedIndex) as ComboBoxItem;
				if (container == null) {
					var index = ItemContainerGenerator.GeneratorPositionFromIndex (SelectedIndex);
					using (ItemContainerGenerator.StartAt (index, GeneratorDirection.Forward, true))
						container = ItemContainerGenerator.GenerateNext (out fresh) as ComboBoxItem;
					ItemContainerGenerator.PrepareItemContainer (container);
				}
				SelectionBoxItemTemplate = container.ContentTemplate;
			}

			_contentPresenter.Content = SelectionBoxItem;
			_contentPresenter.ContentTemplate = SelectionBoxItemTemplate;
		}
		
		void UpdatePopupSizeAndPosition (object sender, EventArgs args)
		{
			if (_popup == null)
				return;

			FrameworkElement child = _popup.RealChild as FrameworkElement;
			if (child == null)
				return;

			child.MinWidth = ActualWidth;
			
			var root = Application.Current.Host.Content;
			if (root == null)
				return;

			GeneralTransform xform;

			try {
				xform = TransformToVisual (null);
			}
			catch (ArgumentException) {
				// exception is raised if the combobox is no longer in the visual tree
				// LayoutUpdated -= UpdatePopupSizeAndPosition;
				return;
			}
			
			Point  offset = new Point (0,ActualHeight);
			Point bottom_right = new Point (offset.X + child.ActualWidth, offset.Y + child.ActualHeight);
			bottom_right = xform.Transform (bottom_right);
			Point top_left = xform.Transform (offset);

			if (FlowDirection == FlowDirection.RightToLeft) {
				double left = bottom_right.X;
				bottom_right.X = top_left.X;
				top_left.X = left;
			}
			
			Point final_offset = new Point (0,0);
			if (bottom_right.X > root.ActualWidth) {
				final_offset.X = root.ActualWidth - bottom_right.X;
			} else if (top_left.X < 0) {
				final_offset.X = offset.X -top_left.X;
			} else {
				final_offset.X = offset.X;
			}
			
			if (FlowDirection == FlowDirection.RightToLeft) {
				final_offset.X = -offset.X;
			}

			if (bottom_right.Y > root.ActualHeight) {
				final_offset.Y = -child.ActualHeight;
			} else {
				final_offset.Y = RenderSize.Height;
			}
			
			_popup.HorizontalOffset = final_offset.X;
			_popup.VerticalOffset = final_offset.Y;
			// Silverlight does not resize its dropdown properly when the available height is altered.
			// This means that if you open a dropdown while your browser window is small, you end up
			// with a dropdown that is far too small forever. Instead of this we will resize the dropdown
			// to a more usable height.
			UpdatePopupMaxHeight (MaxDropDownHeight);
		}

		void UpdatePopupMaxHeight (double height)
		{
			if (_popup != null && _popup.Child is FrameworkElement) {
				if (height == double.PositiveInfinity)
					height = Application.Current.Host.Content.ActualHeight / 2.0;
				((FrameworkElement) _popup.RealChild).MaxHeight = height;
			}
		}
		
		protected override void ClearContainerForItemOverride (DependencyObject element, object item)
		{
			base.ClearContainerForItemOverride (element, item);
		}

		
		ContentPresenter _contentPresenter;
		Popup _popup;
		ToggleButton _dropDownToggle;

		bool isMouseOver;
	}
}
