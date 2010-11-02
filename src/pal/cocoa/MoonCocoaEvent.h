#import "MoonNSView.h"
#import <AppKit/AppKit.h>

@interface MoonCocoaEvent : NSObject {
	NSEvent *event;
	MoonNSView *view;
}

@property NSEvent *event;
@property MoonNSView *view;

- (id) initWithEvent: (NSEvent *) event view: (MoonNSView *) view;
@end

