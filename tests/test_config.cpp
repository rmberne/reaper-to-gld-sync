#include "../src/config_loader.hpp"
#include <fstream>
#include <iostream>
#include <cassert>

void testDefaultConfig() {
    config::Config cfg = config::ConfigLoader::load("non_existent_config.txt");
    assert(cfg.mixerIp == "192.168.1.10");
    assert(cfg.mixerEnabled == true);
    assert(cfg.pulseEnabled == false);
    std::cout << "testDefaultConfig passed" << std::endl;
}

void testCustomConfig() {
    std::ofstream tmp("test_config.txt");
    tmp << "IP=10.0.0.5\n";
    tmp << "PORT=12345\n";
    tmp << "CHANNEL=5\n";
    tmp << "PARAMETER=1000\n";
    tmp << "MIXER_ENABLED=false\n";
    tmp << "PULSE_ENABLED=true\n";
    tmp.close();

    config::Config cfg = config::ConfigLoader::load("test_config.txt");
    assert(cfg.mixerIp == "10.0.0.5");
    assert(cfg.mixerPort == "12345");
    assert(cfg.midiChannel == 5);
    assert(cfg.nrpnParam == 1000);
    assert(cfg.mixerEnabled == false);
    assert(cfg.pulseEnabled == true);
    
    std::cout << "testCustomConfig passed" << std::endl;
}

int main() {
    testDefaultConfig();
    testCustomConfig();
    std::cout << "All ConfigLoader tests passed!" << std::endl;
    return 0;
}
