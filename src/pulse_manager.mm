#include "pulse_manager.hpp"
#include <CoreBluetooth/CoreBluetooth.h>
#include <dispatch/dispatch.h>
#include <iostream>
static NSString *const kPulseServiceUUID = @"ccb2986e-1b2d-4c29-9cf8-25cdf8fe44fc";
static NSString *const kPulseWriteCharUUID = @"fbaf40a5-ccd9-4e41-88f6-ef56c0ba299d";
static NSString *const kPulseNotifyCharUUID = @"15a08173-ac6b-4804-853a-7af4d795a8ad";





@interface PulseBleDelegate
    : NSObject <CBCentralManagerDelegate, CBPeripheralDelegate>
@property(strong, nonatomic) CBCentralManager *centralManager;
@property(strong, nonatomic) CBPeripheral *pulsePeripheral;
@property(strong, nonatomic) CBCharacteristic *writeCharacteristic;
@property(strong, nonatomic) CBCharacteristic *notifyCharacteristic;
@property(assign, nonatomic) BOOL isReady;
@property(assign, nonatomic) int lastKnownBpm;

- (void)sendCommand:(uint8_t)cmd value:(uint8_t)value;
- (void)setBpm:(int)bpm;
- (void)sendClock;
- (void)sendStart;
- (void)sendStop;
@end

@implementation PulseBleDelegate

- (instancetype)init {
  self = [super init];
  if (self) {
    dispatch_queue_t centralQueue =
        dispatch_queue_create("com.reaper.pulse.ble", DISPATCH_QUEUE_SERIAL);
    _centralManager = [[CBCentralManager alloc] initWithDelegate:self
                                                           queue:centralQueue];
    _isReady = NO;
    _lastKnownBpm = 120; // Default BPM
  }
  return self;
}

#pragma mark - Central Manager

- (void)centralManagerDidUpdateState:(CBCentralManager *)central {
  if (central.state == CBManagerStatePoweredOn) {
    std::cout << "[Pulse BLE] Bluetooth Powered On. Scanning for Soundbrenner..." << std::endl;

    // Scan for our specific service
    NSArray *services = @[[CBUUID UUIDWithString:kPulseServiceUUID]];
    [self.centralManager scanForPeripheralsWithServices:services options:nil];

  } else {
    std::cerr << "[Pulse BLE] Bluetooth state updated to: " << central.state
              << std::endl;
  }
}

- (void)centralManager:(CBCentralManager *)central
    didDiscoverPeripheral:(CBPeripheral *)peripheral
        advertisementData:(NSDictionary<NSString *, id> *)advertisementData
                     RSSI:(NSNumber *)RSSI {

  std::cout << "[Pulse BLE] Discovered: " << ([peripheral.name UTF8String] ? [peripheral.name UTF8String] : "Unknown") << std::endl;

  // Try to match device by name if no services are advertised or as a fallback
  if ([peripheral.name containsString:@"Soundbrenner"] || [peripheral.name containsString:@"Pulse"]) {
      std::cout << "[Pulse BLE] Found Pulse device! Connecting..." << std::endl;

      self.pulsePeripheral = peripheral;
      self.pulsePeripheral.delegate = self;

      [self.centralManager stopScan];
      [self.centralManager connectPeripheral:self.pulsePeripheral options:nil];
  } else {
    // If scanning with service filter, this else might not be strictly needed,
    // but good for robustness if scanning without filters in the future.
    std::cout << "[Pulse BLE] Not a Pulse device. Ignoring." << std::endl;
  }
}

- (void)centralManager:(CBCentralManager *)central
    didConnectPeripheral:(CBPeripheral *)peripheral {

  std::cout << "[Pulse BLE] Connected. Discovering services..." << std::endl;
  [peripheral discoverServices:@[[CBUUID UUIDWithString:kPulseServiceUUID]]];
}

- (void)centralManager:(CBCentralManager *)central
    didDisconnectPeripheral:(CBPeripheral *)peripheral
                      error:(NSError *)error {

  std::cout << "[Pulse BLE] Disconnected. Re-scanning..." << std::endl;

  self.isReady = NO;
  self.pulsePeripheral = nil;
  self.writeCharacteristic = nil;
  self.notifyCharacteristic = nil; // Clear notify characteristic on disconnect

  NSArray *services = @[[CBUUID UUIDWithString:kPulseServiceUUID]];
  [self.centralManager scanForPeripheralsWithServices:services options:nil];
}

#pragma mark - Peripheral

- (void)peripheral:(CBPeripheral *)peripheral
    didDiscoverServices:(NSError *)error {

  if (error) {
    std::cerr << "[Pulse BLE] Service discovery error: " <<
        [[error localizedDescription] UTF8String] << std::endl;
    return;
  }
  for (CBService *service in peripheral.services) {
    if ([[service.UUID UUIDString].lowercaseString isEqualToString:kPulseServiceUUID]) {
      std::cout << "[Pulse BLE] Service found: " <<
          [[service.UUID UUIDString] UTF8String] << std::endl;
      // Discover only the characteristics we care about for this service
      NSArray *characteristics = @[[CBUUID UUIDWithString:kPulseWriteCharUUID],
                                   [CBUUID UUIDWithString:kPulseNotifyCharUUID]];
      [peripheral discoverCharacteristics:characteristics forService:service];
    }
  }
}

- (void)peripheral:(CBPeripheral *)peripheral
    didDiscoverCharacteristicsForService:(CBService *)service
                                   error:(NSError *)error {

  if (error) {
    std::cerr << "[Pulse BLE] Characteristic discovery error: " <<
        [[error localizedDescription] UTF8String] << std::endl;
    return;
  }

  for (CBCharacteristic *c in service.characteristics) {
    NSString *uuid = [c.UUID UUIDString].lowercaseString;
    std::cout << "[Pulse BLE] Characteristic: " << [uuid UTF8String] << " properties: " << (int)c.properties << std::endl;

    if ([uuid isEqualToString:kPulseWriteCharUUID]) {
      std::cout << "[Pulse BLE] Found write characteristic! (" << [uuid UTF8String] << ")" << std::endl;
      self.writeCharacteristic = c;
    } else if ([uuid isEqualToString:kPulseNotifyCharUUID]) {
      std::cout << "[Pulse BLE] Found notify characteristic! (" << [uuid UTF8String] << ") Enabling notifications." << std::endl;
      self.notifyCharacteristic = c;
      [peripheral setNotifyValue:YES forCharacteristic:c];
    }
  }

  // Check if both characteristics are found and notify is active
  if (self.writeCharacteristic && self.notifyCharacteristic && self.notifyCharacteristic.isNotifying) {
      self.isReady = YES;
  } else {
      self.isReady = NO;
  }
}

- (void)peripheral:(CBPeripheral *)peripheral
    didUpdateNotificationStateForCharacteristic:(CBCharacteristic *)characteristic
                                          error:(NSError *)error {
  if (error) {
    std::cerr << "[Pulse BLE] Error changing notification state: " <<
        [[error localizedDescription] UTF8String] << std::endl;
    return;
  }

  if ([characteristic.UUID.UUIDString.lowercaseString isEqualToString:kPulseNotifyCharUUID]) {
    if (characteristic.isNotifying) {
      std::cout << "[Pulse BLE] Notification enabled for " << [characteristic.UUID.UUIDString UTF8String] << std::endl;
      // Set ready if write characteristic is also available
      if (self.writeCharacteristic && self.notifyCharacteristic && self.notifyCharacteristic.isNotifying) {
          self.isReady = YES;
      }
    } else {
      std::cout << "[Pulse BLE] Notification disabled for " << [characteristic.UUID.UUIDString UTF8String] << std::endl;
      self.isReady = NO;
    }
  }
}

#pragma mark - Send Data

- (void)sendCommand:(uint8_t)cmd value:(uint8_t)value {
  if (!self.isReady || !self.pulsePeripheral || !self.writeCharacteristic)
    return;

  static uint16_t seq = 0;

  uint8_t packet[4];
  packet[0] = seq & 0xFF; // seq_lo
  packet[1] = (seq >> 8) & 0xFF; // seq_hi
  packet[2] = cmd;
  packet[3] = value;

  seq++;

  NSData *data = [NSData dataWithBytes:packet length:4];

  std::cout << "[Pulse BLE] Sending: ";
  for (int i = 0; i < 4; i++) {
    std::cout << std::hex << (int)packet[i] << " ";
  }
  std::cout << std::dec << std::endl;

  [self.pulsePeripheral writeValue:data
                 forCharacteristic:self.writeCharacteristic
                              type:CBCharacteristicWriteWithoutResponse];
}

- (void)setBpm:(int)bpm {
  // Command 0x01 is BPM
  self.lastKnownBpm = bpm; // Store for start command
  [self sendCommand:0x01 value:(uint8_t)bpm];
}

- (void)sendClock() {
    if (!self.isReady || !self.pulsePeripheral || !self.writeCharacteristic)
        return;

    uint8_t midiClockByte = 0xF8;
    NSData *data = [NSData dataWithBytes:&midiClockByte length:1];

    [self.pulsePeripheral writeValue:data
                   forCharacteristic:self.writeCharacteristic
                                type:CBCharacteristicWriteWithoutResponse];
}

- (void)sendStart {
    // Start by sending the last known BPM
    [self setBpm:self.lastKnownBpm];
}

- (void)sendStop {
    // Stop by sending BPM 0
    [self setBpm:0];
}

@end

namespace rt {
namespace midi {

struct PulseManager::Impl {
  PulseBleDelegate *delegate;
};

PulseManager::PulseManager() : impl_(new Impl()) {
  impl_->delegate = [[PulseBleDelegate alloc] init];
}

PulseManager::~PulseManager() {
  if (impl_) {
    if (impl_->delegate) {
      if (impl_->delegate.pulsePeripheral) {
        [impl_->delegate.centralManager
            cancelPeripheralConnection:impl_->delegate.pulsePeripheral];
      }
      impl_->delegate = nil;
    }
    delete impl_;
  }
}

bool PulseManager::openPort() { return impl_->delegate.centralManager != nil; }

void PulseManager::setBpm(int bpm) { [impl_->delegate setBpm:bpm]; }

void PulseManager::sendClock() { [impl_->delegate sendClock]; }

void PulseManager::sendStart() { [impl_->delegate sendStart]; }

void PulseManager::sendStop() { [impl_->delegate sendStop]; }

} // namespace midi
} // namespace rt
