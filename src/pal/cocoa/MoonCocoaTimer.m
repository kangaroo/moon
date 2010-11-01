#import "MoonCocoaTimer.h"

@implementation MoonCocoaTimer

@synthesize timeout;
@synthesize userInfo;

- (void) onTick: (NSTimer *) timer
{
	if (!timeout (userInfo))
		[timer invalidate];
}

@end
