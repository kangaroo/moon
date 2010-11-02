#import "MoonNSView.h"
#import <AppKit/AppKit.h>

@interface MoonCocoaEvent : NSObject {
	NSEvent *event;
	MoonNSView *view;
}

@property(assign) NSEvent *event;
@property(assign) MoonNSView *view;

- (id) initWithEvent: (NSEvent *) event view: (MoonNSView *) view;
@end

