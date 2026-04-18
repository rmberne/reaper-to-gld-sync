#import <Cocoa/Cocoa.h>

@interface ConfigWindowController : NSWindowController

@property (weak) IBOutlet NSTextField *mixerIpField;
@property (weak) IBOutlet NSTextField *mixerPortField;
@property (weak) IBOutlet NSTextField *midiChannelField;
@property (weak) IBOutlet NSTextField *nrpnParamField;
@property (weak) IBOutlet NSButton *arduinoEnabledCheckbox;
@property (weak) IBOutlet NSButton *pulseEnabledCheckbox;
@property (weak) IBOutlet NSButton *mixerEnabledCheckbox;

- (IBAction)saveButtonClicked:(id)sender;

@end
