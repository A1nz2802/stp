#include "stp.h"
#include <stdint.h>

uint64_t get_switch_bid(struct Switch *sw) {
    uint64_t high_part = (uint64_t)sw->bridge_priority;
    uint64_t priority_part = high_part << 48;

    uint64_t mac_part = sw->mac_address & 0x0000FFFFFFFFFFFF;
    uint64_t final_bid = priority_part | mac_part;

    return final_bid;
}

struct Topology init_topology() {
    struct Topology topology;

    // --- Switch 1 (Future Root Switch) ---
    topology.switches[0] = (struct Switch){
        .bridge_priority = 4096, // Lower priority to force it to be switch root!
        .mac_address = 0x001BA13C8EF1,
        .root_port_index = -1, // It's the root, it's doesn't have root port
        .ports = {
            [0] = {.port_number = 1, .port_priority = 128, .port_cost = 19, .state = BLOCKING},
            [1] = {.port_number = 2, .port_priority = 128, .port_cost = 19, .state = BLOCKING},
        }};

    // --- Switch 2 ---
    topology.switches[1] = (struct Switch){
        .bridge_priority = 32768, // default value
        .mac_address = 0x001BA13CBBBB,
        .root_port_index = -1, // It's still not known who the root is
        .ports = {
            [0] = {.port_number = 1, .port_priority = 128, .port_cost = 19, .state = BLOCKING},
            [1] = {.port_number = 2, .port_priority = 128, .port_cost = 19, .state = BLOCKING}}};

    // --- Switch 3 ---
    topology.switches[2] = (struct Switch){
        .bridge_priority = 32768,
        .mac_address = 0x00000000CCCC,
        .root_port_index = -1,
        .ports = {
            [0] = {.port_number = 1, .port_priority = 128, .port_cost = 19, .state = BLOCKING},
            [1] = {.port_number = 2, .port_priority = 128, .port_cost = 19, .state = BLOCKING}}};

    // --- Inicialización Lógica de STP para cada Switch ---
    // (Cada switch asume que ÉL es el Root al inicio)
    for (int i = 0; i < NUM_SWITCHES; i++) {
        struct Switch *sw = &topo.switches[i];
        uint64_t my_bid = get_switch_bid(sw);

        sw->my_bpdu.root_id = my_bid;
        sw->my_bpdu.root_path_cost = 0;
        sw->my_bpdu.bridge_id = my_bid;
    }

    return topology;
}
