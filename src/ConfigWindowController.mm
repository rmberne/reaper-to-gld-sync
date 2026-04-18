#import "ConfigWindowController.h"

@implementation ConfigWindowController

- (void)windowDidLoad {
    [super windowDidLoad];
    
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    
    self.mixerIpField.stringValue = [defaults stringForKey:@"mixerIp"] ?: @"192.168.1.10";
    self.mixerPortField.intValue = (int)[defaults integerForKey:@"mixerPort"] ?: 51325;
    self.midiChannelField.intValue = (int)[defaults integerForKey:@"midiChannel"];
    self.nrpnParamField.intValue = (int)[defaults integerForKey:@"nrpnParam"] ?: 8449;
    
    self.arduinoEnabledCheckbox.state = [defaults boolForKey:@"arduinoEnabled"] ? NSControlStateValueOn : NSControlStateValueOff;
    self.pulseEnabledCheckbox.state = [defaults boolForKey:@"pulseEnabled"] ? NSControlStateValueOn : NSControlStateValueOff;
    self.mixerEnabledCheckbox.state = [defaults boolForKey:@"mixerEnabled"] ? NSControlStateValueOn : NSControlStateValueOff;
}

- (IBAction)saveButtonClicked:(id)sender {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    
    [defaults setObject:self.mixerIpField.stringValue forKey:@"mixerIp"];
    [defaults setInteger:self.mixerPortField.intValue forKey:@"mixerPort"];
    [defaults setInteger:self.midiChannelField.intValue forKey:@"midiChannel"];
    [defaults setInteger:self.nrpnParamField.intValue forKey:@"nrpnParam"];
    [defaults setBool:(self.arduinoEnabledCheckbox.state == NSControlStateValueOn) forKey:@"arduinoEnabled"];
    [defaults setBool:(self.pulseEnabledCheckbox.state == NSControlStateValueOn) forKey:@"pulseEnabled"];
    [defaults setBool:(self.mixerEnabledCheckbox.state == NSControlStateValueOn) forKey:@"mixerEnabled"];
    
    [defaults synchronize];
    [self.window close];
}

@end
