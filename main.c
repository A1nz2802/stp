#include <stdint.h>

#define MAX_PORTS 16

// BPDU (Bridge Protocol Data Unit)

int select_root_switch() {
  return 0;
};

// Compare two BPDU's
int get_better_bpdu(struct Bpdu bpdu1, struct Bpdu bpdu2) {
  return 0;
}

int create_some_switches() {

}

// 08:00:27:7B:A2:04
// AC:4B:C3:9D:55:1A
int main() {
  struct Switch sw1 = {
    .bridge_priority = 32768,
    .mac_address = 0x001BA13C8EF1, // 00:1B:A1:3C:8E:F1
    // bridge_id is a calculated field
    .ports = {
      // Port 1 (index 0)
      [0] = {
        .port_number = 1,
        .port_priority = 128,
        .port_cost = 19, // 19 = 100Mbps
        .state = BLOCKING, // all ports starts blocked
      },
      [1] = {
        .port_number = 2,
        .port_priority = 128,
        .port_cost = 19, // 19 = 100Mbps
        .state = BLOCKING, // all ports starts blocked
      }
    },
    .root_port_index = -1
  };
}
