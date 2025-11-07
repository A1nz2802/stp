#include "stp.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
/**
 * @brief Calculates the 64-bit Bridge ID (BID) from its components.
 * * @details The BID is a 64-bit value used to uniquely identify a switch and
 * determine the Root Bridge. It is a composite of the 16-bit
 * bridge priority and the 48-bit MAC address.
 *
 * @param sw A pointer to the switch whose BID is to be calculated.
 * @return The 64-bit unsigned Bridge ID.
 */
uint64_t get_switch_bid(struct Switch *sw) {
    uint64_t high_part = (uint64_t)sw->bridge_priority;
    uint64_t priority_part = high_part << 48;

    uint64_t mac_part = sw->mac_address & 0x0000FFFFFFFFFFFF;
    uint64_t final_bid = priority_part | mac_part;

    return final_bid;
}

struct Topology init_topology() {
    struct Topology topology;

    // --- Switch 0 (Future Root Switch) ---
    topology.switches[0] = (struct Switch){
        // A low priority ensures this switch wins the Root election
        .bridge_priority = 4096,
        .mac_address = 0x001BA13C8EF1,
        // -1 = No Root Port (bacause this switch *is* the Root)
        .root_port_index = -1,
        .ports = {
            // port_cost 19 = 100Mbps (Fast Ethernet)
            // port_priority 128 = Standard default
            [0] = {.port_number = 1, .port_priority = 128, .port_cost = 19, .state = BLOCKING},
            [1] = {.port_number = 2, .port_priority = 128, .port_cost = 19, .state = BLOCKING},
        }};

    // --- Switch 1 ---
    topology.switches[1] = (struct Switch){
        // 32768 = The standard default priority
        .bridge_priority = 32768,
        .mac_address = 0x001BA13CBBBB,
        // -1 = No Root Port (it doesn't know who the Root is yet)
        .root_port_index = -1,
        .ports = {
            [0] = {.port_number = 1, .port_priority = 128, .port_cost = 19, .state = BLOCKING},
            [1] = {.port_number = 2, .port_priority = 128, .port_cost = 19, .state = BLOCKING}}};

    // --- Switch 2 ---
    topology.switches[2] = (struct Switch){
        .bridge_priority = 32768,
        .mac_address = 0x00000000CCCC,
        .root_port_index = -1,
        .ports = {
            [0] = {.port_number = 1, .port_priority = 128, .port_cost = 19, .state = BLOCKING},
            [1] = {.port_number = 2, .port_priority = 128, .port_cost = 19, .state = BLOCKING}}};

    // STP Logic Initialization for each Switch.
    // At startup, every switch assumes it's the Root Bridge.
    // This loop populates their "my_bpdu" template with their own info.
    for (int i = 0; i < NUM_SWITCHES; i++) {
        struct Switch *sw = &topology.switches[i];
        uint64_t my_bid = get_switch_bid(sw);

        sw->my_bpdu.root_id = my_bid;   // "I'm the Root"
        sw->my_bpdu.root_path_cost = 0; // "Cost to reach me is 0"
        sw->my_bpdu.bridge_id = my_bid; // "I'm the sender"
    }

    return topology;
}

void connect_topology(struct Topology *topology) {

    // Connection 1: Switch 0, Port 0 <-----> Switch 1, Port 0
    topology->switches[0].ports[0].connected_switch = &topology->switches[1];
    topology->switches[1].ports[0].connected_switch = &topology->switches[0];

    // Connection 2: Switch 1, Port 1 <-----> Switch 2, Port 0
    topology->switches[1].ports[1].connected_switch = &topology->switches[2];
    topology->switches[2].ports[0].connected_switch = &topology->switches[1];

    // Connection 3: Switch 2, Port 1 <-----> Switch 0, Port 1
    topology->switches[2].ports[1].connected_switch = &topology->switches[0];
    topology->switches[0].ports[1].connected_switch = &topology->switches[2];
}

void run_simulation_tick(struct Topology topology) {
}

void deliver_bpdu_to_port(struct Port *port, struct Bpdu *bpdu) {
    // Safety check: Is the inbox full?
    if (port->inbox_count >= MAX_BPDU_QUEUE) {
        // This shouldn't happen in our simple simulation, but it's good defense.
        printf("⚠️ Port %d (Switch ?) inbox is full. BPDU dropped.\n", port->port_number);
        return;
    }

    // Copy the BPDU into the next available slot in the inbox
    port->bpdu_inbox[port->inbox_count] = *bpdu;

    // Increment the count of BPDUs in the inbox
    port->inbox_count++;
}

void clear_port_inbox(struct Port *port) {
    port->inbox_count = 0;
}

void send_bpdu_on_port(struct Port *sending_port, struct Bpdu *bpdu_to_send) {
    // Check if this port is actually "plugged in"
    if (sending_port->connected_port == NULL) {
        return;
    }

    // --- User-requested Debug Message ---
    // We use the BPDU's 'bridge_id' to see who the sender is.
    // We can't easily get the neighbor's switch ID, but we can get its port number.
    printf("[TX] BPDU sent from S-%" PRIx64 " Port-%d -> delivered to Port-%d\n",
           bpdu_to_send->bridge_id,
           sending_port->port_number,
           sending_port->connected_port->port_number);

    deliver_bpdu_to_port(sending_port->connected_port, bpdu_to_send);
}

void send_all_bpdus(struct Topology *topology) {
    printf("\n--- Send BPDU Phase ---\n");

    // Loop through every switch in the network
    for (int i = 0; i < NUM_SWITCHES; i++) {
        struct Switch *sw = &topology->switches[i];

        // LOGIC: When does a switch send a BPDU?
        // 1. If it's the Root Bridge (or thinks it is)
        // 2. If it's a non-Root switch, it only sends BPDUs out of its DESIGNATED ports.

        // For our startup simulation, *every* switch thinks it's the Root.
        // So, this logic is correct for the initial "election" phase.
        if (sw->my_bpdu.root_id == get_switch_bid(sw)) {

            // This switch believes it is the Root.
            // It should send its BPDU out of all its ports.
            // (Later, we'll refine this to "all DESIGNATED ports")
            for (int p_idx = 0; p_idx < MAX_PORTS; p_idx++) {
                struct Port *port = &sw->ports[p_idx];

                // Only send if the port is connected to something
                if (port->connected_port != NULL) {

                    // Send this switch's self-generated BPDU
                    send_bpdu_on_port(port, &sw->my_bpdu);
                }
            }
        }
    }
}
