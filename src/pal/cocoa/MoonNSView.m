#import "MoonNSView.h"

@implementation MoonNSView

@synthesize moonwindow;

-(void)drawRect:(NSRect)rect
{
	moonwindow->ExposeEvent (Moonlight::Rect (rect.origin.x, rect.origin.y, rect.size.width, rect.size.height));
}

@end
