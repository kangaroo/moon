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
	NSLog (@"Mouse Down");
}

- (void)mouseMoved:(NSEvent *)event
{
	moonwindow->MotionEvent (event);
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
