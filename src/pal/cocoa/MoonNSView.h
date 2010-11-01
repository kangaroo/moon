#import "window-cocoa.h"
#import <AppKit/AppKit.h>

@interface MoonNSView : NSView {
	Moonlight::MoonWindowCocoa *moonwindow;
}

@property Moonlight::MoonWindowCocoa *moonwindow;
@end

