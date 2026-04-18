#import <Cocoa/Cocoa.h>
#include "engine.hpp"

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (strong, nonatomic) NSStatusItem *statusItem;
@property (assign, nonatomic) Engine *engine;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    self.engine = new Engine();
    
    self.statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
    self.statusItem.button.title = @"🥁";
    
    NSMenu *menu = [[NSMenu alloc] init];
    [menu addItemWithTitle:@"Start Engine" action:@selector(startEngine:) keyEquivalent:@""];
    [menu addItemWithTitle:@"Stop Engine" action:@selector(stopEngine:) keyEquivalent:@""];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItemWithTitle:@"Quit" action:@selector(quitApp:) keyEquivalent:@"q"];
    // Disable the Stop button on launch since the engine hasn't started yet
    [[menu itemAtIndex:1] setEnabled:NO];
    
    self.statusItem.menu = menu;

}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
    if (self.engine) {
        self.engine->stop();
        delete self.engine;
        self.engine = nullptr;
    }
}

- (void)startEngine:(id)sender {
    if (self.engine) {
        self.engine->start();

        // Disable "Start" (Index 0), Enable "Stop" (Index 1)
        [[self.statusItem.menu itemAtIndex:0] setEnabled:NO];
        [[self.statusItem.menu itemAtIndex:1] setEnabled:YES];

        // Optional: Change the icon to show it's "Live"
        self.statusItem.button.title = @"🟢 🥁";
    }
}

- (void)stopEngine:(id)sender {
    if (self.engine) {
        self.engine->stop();

        // Enable "Start" (Index 0), Disable "Stop" (Index 1)
        [[self.statusItem.menu itemAtIndex:0] setEnabled:YES];
        [[self.statusItem.menu itemAtIndex:1] setEnabled:NO];

        // Change icon back to standby
        self.statusItem.button.title = @"🥁";
    }
}

- (void)quitApp:(id)sender {
    [NSApp terminate:nil];
}

@end

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        AppDelegate *delegate = [[AppDelegate alloc] init];
        app.delegate = delegate;
        [app run];
    }
    return 0;
}
