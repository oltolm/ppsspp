//
//  DisplayManager.m
//  native
//
//  Created by xieyi on 2019/6/9.
//

#import "DisplayManager.h"
#import "ViewController.h"
#import "AppDelegate.h"
#include "base/display.h"
#include "base/NativeApp.h"
#include "Core/System.h"

#define IS_IPAD() ([UIDevice currentDevice].userInterfaceIdiom == UIUserInterfaceIdiomPad)

@interface DisplayManager ()

@property BOOL listenerActive;
@property (atomic, retain) NSMutableArray<UIScreen *> *extDisplays;

- (void)updateScreen:(UIScreen *)screen;

@end

@implementation DisplayManager

- (instancetype)init
{
	self = [super init];
	if (self) {
		[self setListenerActive:NO];
		[self setExtDisplays:[[NSMutableArray<UIScreen *> alloc] init]];
	}
	return self;
}

+ (DisplayManager *)shared {
	static DisplayManager *sharedInstance = nil;
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		sharedInstance = [[DisplayManager alloc] init];
	});
	return sharedInstance;
}

- (void)setupDisplayListener {
	if ([self listenerActive]) {
		NSLog(@"setupDisplayListener already called");
		return;
	}
	NSLog(@"Setting up display manager");
	[self setMainScreen:[UIScreen mainScreen]];
	// Display connected
	[[NSNotificationCenter defaultCenter] addObserverForName:UIScreenDidConnectNotification object:nil queue:nil usingBlock:^(NSNotification * _Nonnull notification) {
		UIScreen *screen = (UIScreen *) notification.object;
		NSLog(@"New display connected: %@", [screen debugDescription]);
		[[self extDisplays] addObject:screen];
		// Do not switch to second connected display
		if ([self mainScreen] != [UIScreen mainScreen]) {
			return;
		}
		[self updateScreen:screen];
	}];
	// Display disconnected
	[[NSNotificationCenter defaultCenter] addObserverForName:UIScreenDidDisconnectNotification object:nil queue:nil usingBlock:^(NSNotification * _Nonnull notification) {
		UIScreen *screen = (UIScreen *) notification.object;
		NSLog(@"Display disconnected: %@", [screen debugDescription]);
		if ([[self extDisplays] containsObject:screen]) {
			[[self extDisplays] removeObject:screen];
		}
		if ([[self extDisplays] count] > 0) {
			UIScreen *newScreen = [[self extDisplays] lastObject];
			[self updateScreen:newScreen];
		} else {
			[self updateScreen:[UIScreen mainScreen]];
		}
	}];
	[self setListenerActive:YES];
}

- (void)updateScreen:(UIScreen *)screen {
	[self setMainScreen:screen];
	UIWindow *gameWindow = [(AppDelegate *)[[UIApplication sharedApplication] delegate] window];
	// Hide before moving window to external display, otherwise iPhone won't switch to it
	[gameWindow setHidden:YES];
	[gameWindow setScreen:screen];
	// Set optimal resolution
	// Dispatch later to prevent "no window is preset" error
	dispatch_async(dispatch_get_main_queue(), ^{
		if (screen != [UIScreen mainScreen]) {
			NSUInteger count = [[screen availableModes] count];
			UIScreenMode* mode = [screen availableModes][count - 1];
			[screen setCurrentMode:mode];
			[screen setOverscanCompensation:UIScreenOverscanCompensationNone];
		}
		[self updateResolution:screen];
		[gameWindow setHidden:NO];
	});
}

- (void)updateResolution:(UIScreen *)screen {
	float scale = screen.scale;
	
	if ([screen respondsToSelector:@selector(nativeScale)]) {
		scale = screen.nativeScale;
	}
	
	CGSize size = screen.applicationFrame.size;
	
	if (size.height > size.width) {
		float h = size.height;
		size.height = size.width;
		size.width = h;
	}
	
	g_dpi = (IS_IPAD() ? 200.0f : 150.0f) * scale;
	g_dpi_scale_x = 240.0f / g_dpi;
	g_dpi_scale_y = 240.0f / g_dpi;
	g_dpi_scale_real_x = g_dpi_scale_x;
	g_dpi_scale_real_y = g_dpi_scale_y;
	pixel_xres = size.width * scale;
	pixel_yres = size.height * scale;
	
	dp_xres = pixel_xres * g_dpi_scale_x;
	dp_yres = pixel_yres * g_dpi_scale_y;
	
	pixel_in_dps_x = (float)pixel_xres / (float)dp_xres;
	pixel_in_dps_y = (float)pixel_yres / (float)dp_yres;
	
	[[sharedViewController view] setContentScaleFactor:scale];
	
	// PSP native resize
	PSP_CoreParameter().pixelWidth = pixel_xres;
	PSP_CoreParameter().pixelHeight = pixel_yres;
	
	NativeResized();
	
	NSLog(@"Updated display resolution: (%d, %d) @%.1fx", pixel_xres, pixel_yres, scale);
}

@end
