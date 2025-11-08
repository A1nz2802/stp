#include "stp.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

// Helper function to convert state enum to string
const char *get_state_string(enum PortState state) {
    switch (state) {
    case BLOCKING:
        return "BLOCKING";
    case LISTENING:
        return "LISTENING";
    case LEARNING:
        return "LEARNING";
    case FORWARDING:
        return "FORWARDING";
    case DISABLED:
        return "DISABLED";
    default:
        return "UNKNOWN";
    }
}

// Helper function to convert role enum to string
const char *get_role_string(enum PortRole role) {
    switch (role) {
    case ROOT:
        return "Root";
    case DESIGNATED:
        return "Designated";
    case NON_DESIGNATED:
        return "Blocking";
    default:
        return "Unknown";
    }
}

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

// void run_simulation_tick(struct Topology topology) {
// }

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

int compare_bpdus(struct Bpdu *new_bpdu, struct Bpdu *stored_bpdu) {
    // 1. Lowest Root ID wins
    if (new_bpdu->root_id < stored_bpdu->root_id)
        return BPDU_IS_BETTER;
    if (new_bpdu->root_id > stored_bpdu->root_id)
        return BPDU_IS_WORST;

    // 2. (Root IDs are equal) Lowest Root Path Cost wins
    if (new_bpdu->root_path_cost < stored_bpdu->root_path_cost)
        return BPDU_IS_BETTER;
    if (new_bpdu->root_path_cost > stored_bpdu->root_path_cost)
        return BPDU_IS_WORST;

    // 3. (Costs are equal) Lowest Sender Bridge ID wins
    if (new_bpdu->bridge_id < stored_bpdu->bridge_id)
        return BPDU_IS_BETTER;
    if (new_bpdu->bridge_id > stored_bpdu->bridge_id)
        return BPDU_IS_WORST;

    // 4. (Sender BIDs are equal) Lowest Sender Port ID wins
    if (new_bpdu->port_id < stored_bpdu->port_id)
        return BPDU_IS_BETTER;
    if (new_bpdu->port_id > stored_bpdu->port_id)
        return BPDU_IS_WORST;

    // Everything is identical
    return BPDU_IS_EQUAL;
}

void process_all_bpdus(struct Topology *topology) {
    printf("\n--- Process BPDUs Phase ---\n");

    // Loop through every switch
    for (int i = 0; i < NUM_SWITCHES; i++) {
        struct Switch *sw = &topology->switches[i];

        // Loop through every port on that switch
        for (int p_idx = 0; p_idx < MAX_PORTS; p_idx++) {
            struct Port *port = &sw->ports[p_idx];

            // Skip if this port's inbox is empty
            if (port->inbox_count == 0) {
                continue;
            }

            // Loop through all BPDUs in this port's inbox
            for (int b_idx = 0; b_idx < port->inbox_count; b_idx++) {
                struct Bpdu *received_bpdu = &port->bpdu_inbox[b_idx];

                // Compare the new BPDU with the "best" one this port has ever seen
                int comparison = compare_bpdus(received_bpdu, &port->stored_bpdu);

                if (comparison == BPDU_IS_BETTER) {
                    // --- User-requested Debug Message ---
                    printf("[RX] Switch S-%" PRIx64 " Port-%d received a SUPERIOR BPDU!\n",
                           get_switch_bid(sw),
                           port->port_number);

                    // This new BPDU is better! Save it.
                    port->stored_bpdu = *received_bpdu;
                }
            }

            // After processing all BPDUs in the inbox, clear it for the next tick.
            clear_port_inbox(port);
        }
    }
}

void elect_all_port_roles(struct Topology *topo) {
    printf("\n--- Elect Port Roles Phase ---\n");

    for (int i = 0; i < NUM_SWITCHES; i++) {
        struct Switch *sw = &topo->switches[i];
        uint64_t my_bid = get_switch_bid(sw);

        // --- Step 1: Find the best BPDU this switch knows about ---
        // We start by assuming our *own* BPDU is the best.
        struct Bpdu *best_bpdu = &sw->my_bpdu;
        int root_port_index = -1; // -1 means "no Root Port" (I am the Root)

        // Now, loop through all ports and compare their stored BPDUs
        for (int p_idx = 0; p_idx < MAX_PORTS; p_idx++) {
            struct Port *port = &sw->ports[p_idx];
            if (port->connected_port == NULL) {
                continue; // Skip ports that aren't plugged in
            }

            // Is the BPDU stored on this port better than the best one we've seen so far?
            if (compare_bpdus(&port->stored_bpdu, best_bpdu) == BPDU_IS_BETTER) {
                // Yes! This BPDU is our new best.
                best_bpdu = &port->stored_bpdu;
                root_port_index = p_idx; // This port is our new candidate for Root Port
            }
        }

        // --- Step 2: Make decisions based on the best BPDU found ---

        if (root_port_index == -1) {
            // --- DECISION: I AM THE ROOT BRIDGE ---
            // My own BPDU was better than any I received.
            // This is the "Elected Root" state.

            if (sw->root_port_index != -1) {
                // This is the first tick we've *become* the Root
                printf("  [ELECTION] S-%" PRIx64 " has decided IT IS THE ROOT.\n", my_bid);
            }
            sw->root_port_index = -1; // I don't have a Root Port

            // All my ports must be DESIGNATED ports
            for (int p_idx = 0; p_idx < MAX_PORTS; p_idx++) {
                sw->ports[p_idx].role = DESIGNATED;
            }

        } else {
            // --- DECISION: I AM A NON-ROOT BRIDGE ---
            // A BPDU from port `root_port_index` was superior to mine.

            struct Port *rp = &sw->ports[root_port_index];
            rp->role = ROOT; // Set this port's role to ROOT

            if (sw->root_port_index != root_port_index) {
                // This is the first tick we've chosen this port
                printf("  [ELECTION] S-%" PRIx64 " has elected Port-%d as its ROOT PORT.\n",
                       my_bid, rp->port_number);
            }
            sw->root_port_index = root_port_index;

            // --- Step 3: "SURRENDER" - Update my own BPDU template ---
            // This is the CRUCIAL step you identified.
            // We stop advertising ourselves as the Root.
            sw->my_bpdu.root_id = rp->stored_bpdu.root_id;
            // Our *new* cost is the Root's cost PLUS our port's cost
            sw->my_bpdu.root_path_cost = rp->stored_bpdu.root_path_cost + rp->port_cost;
            sw->my_bpdu.bridge_id = my_bid; // We are still the sender

            // --- Step 4: Decide roles for all *other* ports ---
            for (int p_idx = 0; p_idx < MAX_PORTS; p_idx++) {
                if (p_idx == sw->root_port_index) {
                    continue; // Skip the Root Port
                }

                struct Port *port = &sw->ports[p_idx];
                if (port->connected_port == NULL) {
                    continue; // Skip disconnected ports
                }

                // Is my (new, updated) BPDU superior to what this port is receiving?
                int result = compare_bpdus(&sw->my_bpdu, &port->stored_bpdu);

                if (result == BPDU_IS_BETTER) {
                    // Yes. I win this segment. My port is DESIGNATED.
                    port->role = DESIGNATED;
                } else {
                    // No. I lose this segment. My port must be NON-DESIGNATED (Blocking).
                    if (port->role != NON_DESIGNATED) {
                        printf("  [ELECTION] S-%" PRIx64 " setting Port-%d to BLOCKING (Non-Designated)\n",
                               my_bid, port->port_number);
                        port->role = NON_DESIGNATED;
                    }
                }
            }
        }
    }
}

void update_all_port_states(struct Topology *topology) {
    printf("\n--- Update Port States Phase ---\n");

    for (int i = 0; i < NUM_SWITCHES; i++) {
        struct Switch *sw = &topology->switches[i];

        for (int p_idx = 0; p_idx < MAX_PORTS; p_idx++) {
            struct Port *port = &sw->ports[p_idx];

            // If this port isn't "plugged in", skip all logic for it.
            if (port->connected_port == NULL) {
                continue; // Skip to the next port
            }

            // --- Logic for ROOT and DESIGNATED ports ---
            // These ports want to move *towards* FORWARDING
            if (port->role == ROOT || port->role == DESIGNATED) {

                if (port->state == BLOCKING) {
                    // It's a new Root/Designated port. Start the transition.
                    port->state = LISTENING;
                    port->state_timer = FORWARD_DELAY_SECONDS;
                    printf("[STATE] S-%" PRIx64 " Port-%d -> LISTENING (15s)\n",
                           get_switch_bid(sw), port->port_number);
                } else if (port->state == LISTENING) {
                    // It's in the Listening state. Decrement timer.
                    port->state_timer--;
                    if (port->state_timer <= 0) {
                        port->state = LEARNING;
                        port->state_timer = FORWARD_DELAY_SECONDS;
                        printf("[STATE] S-%" PRIx64 " Port-%d -> LEARNING (15s)\n",
                               get_switch_bid(sw), port->port_number);
                    }
                } else if (port->state == LEARNING) {
                    // It's in the Learning state. Decrement timer.
                    port->state_timer--;
                    if (port->state_timer <= 0) {
                        port->state = FORWARDING;
                        printf("[STATE] S-%" PRIx64 " Port-%d -> FORWARDING (OPEN!)\n",
                               get_switch_bid(sw), port->port_number);
                    }
                }
                // If state is FORWARDING, do nothing. It's stable.

            }

            // --- Logic for NON_DESIGNATED ports ---
            // These ports must be *blocked*
            else if (port->role == NON_DESIGNATED) {

                if (port->state != BLOCKING) {
                    // This port was previously open, but its role is now Blocking.
                    // Shut it down immediately. No timers needed.
                    printf("  [STATE] S-%" PRIx64 " Port-%d -> BLOCKING (Link down)\n",
                           get_switch_bid(sw), port->port_number);
                    port->state = BLOCKING;
                    port->state_timer = 0; // Reset timer
                }
            }
        }
    }
}

void print_network_status(struct Topology *topology) {
    printf("\n--- NETWORK STATUS ---\n");

    for (int i = 0; i < NUM_SWITCHES; i++) {
        struct Switch *sw = &topology->switches[i];
        uint64_t my_bid = get_switch_bid(sw);

        printf("Switch S-%" PRIx64 " (Prio: %u)\n", my_bid, sw->bridge_priority);

        // Check if this switch is the Root
        if (sw->my_bpdu.root_id == my_bid) {
            printf("  ROLE: **ROOT BRIDGE**\n");
        } else {
            printf("  ROLE: Non-Root Bridge\n");
            printf("  -> Believes Root is S-%" PRIx64 "\n", sw->my_bpdu.root_id);
        }

        // Print status of all connected ports
        for (int p_idx = 0; p_idx < MAX_PORTS; p_idx++) {
            struct Port *port = &sw->ports[p_idx];
            if (port->connected_port == NULL) {
                continue; // Skip unused/unplugged ports
            }

            printf("    Port %d: [Role: %-10s] [State: %-10s]",
                   port->port_number,
                   get_role_string(port->role),
                   get_state_string(port->state));

            // Print timer if it's active
            if (port->state == LISTENING || port->state == LEARNING) {
                printf(" (Timer: %2ds left)", port->state_timer);
            }
            printf("\n");
        }
    }

    // THIS IS KEY: Force printf to empty its buffer and print to console NOW
    fflush(stdout);
}
