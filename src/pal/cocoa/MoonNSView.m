#include "runtime.h"
#import "MoonNSView.h"

@implementation MoonNSView

@synthesize moonwindow;

-(void)drawRect:(NSRect)rect
{
	moonwindow->ExposeEvent (Moonlight::Rect (rect.origin.x, rect.origin.y, rect.size.width, rect.size.height));
}

- (void)mouseDown:(NSEvent*)event
{
	moonwindow->ButtonPressEvent (event);
}

- (void)mouseUp:(NSEvent*)event
{
	moonwindow->ButtonReleaseEvent (event);
}

- (void)mouseMoved:(NSEvent *)event
{
	moonwindow->MotionEvent (event);
}

- (void)mouseEntered:(NSEvent *)event
{
	moonwindow->MouseEnteredEvent (event);
}

- (void)mouseExited:(NSEvent *)event
{
	moonwindow->MouseExitedEvent (event);
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
	return YES;
}

- (BOOL)acceptsFirstResponder
{
	return YES;
}

@end
