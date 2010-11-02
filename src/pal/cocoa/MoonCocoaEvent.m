#include "runtime.h"
#import "MoonCocoaEvent.h"

@implementation MoonCocoaEvent

@synthesize view;
@synthesize event;

- (id) initWithEvent: (NSEvent *) event view: (MoonNSView *) view
{
	self.view = view;
	self.event = event;

	return self;
}

@end
