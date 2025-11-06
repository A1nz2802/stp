

struct Topology init_topology() {
    struct Topology topology;

    topology.switches[0] = (struct Switch){
        .bridge_priority = 4096, // Lower priority to force it to be switch root!
        .mac_address = 0x001BA13C8EF1,
        .root_port_index = -1, // It's the root, it's doesn't have root port
        .ports = {
            [0] = {.port_number = 1, .port_priority = 128, .port_cost = 19, .state = BLOCKING},
            [1] = {.port_number = 2, .port_priority = 128, .port_cost = 19, .state = BLOCKING},
        }};

    topology.switches[1] = (struct Switch){
        .bridge_priority = 32768, // default value
        .mac_address = 0x001BA13CBBBB,
        .root_port_index = -1, // It's still not known who the root is
        .ports = {
            [0] = {.port_number = 1, .port_priority = 128, .port_cost = 19, .state = BLOCKING},
            [1] = {.port_number = 2, .port_priority = 128, .port_cost = 19, .state = BLOCKING}}};

    topology.switches[2] = (struct Switch){
        .bridge_priority = 32768,
        .mac_address = 0x00000000CCCC,
        .root_port_index = -1,
        .ports = {
            [0] = {.port_number = 1, .port_priority = 128, .port_cost = 19, .state = BLOCKING},
            [1] = {.port_number = 2, .port_priority = 128, .port_cost = 19, .state = BLOCKING}}};
}
