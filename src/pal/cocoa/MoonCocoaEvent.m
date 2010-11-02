#include "runtime.h"
#import "MoonCocoaEvent.h"

@implementation MoonCocoaEvent

@synthesize view;
@synthesize event;

- (id) initWithEvent: (NSEvent *) evt view: (MoonNSView *) v
{
	self.view = v;
	self.event = evt;

	return self;
}

@end
