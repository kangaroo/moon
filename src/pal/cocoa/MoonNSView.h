#import "window-cocoa.h"
#import <AppKit/AppKit.h>

@interface MoonNSView : NSView {
	Moonlight::MoonWindowCocoa *moonwindow;
	NSTrackingRectTag trackingrect;
}

@property Moonlight::MoonWindowCocoa *moonwindow;
@end

