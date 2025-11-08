#include "stp.h"
#include <stdio.h>
#include <unistd.h>

int main() {
    printf("Starting STP simulator...\n");

    // Create the switches
    struct Topology network = init_topology();

    // Connect the cables
    connect_topology(&network);

    printf("Create and connected %d switches.\n", NUM_SWITCHES);

    printf("Created and connected %d switches. Starting simulation...\n", NUM_SWITCHES);

    // 3. The Simulation Engine Loop
    while (1) {

        // 1. Send all BPDUs (Puts them in the inboxes)
        send_all_bpdus(&network);

        // 2. Process all BPDUs (Reads inboxes, compares, stores best)
        process_all_bpdus(&network); // <-- ¡ACABAMOS DE AÑADIR ESTA!

        // 3. Elect Port Roles (Run the STP algorithm)
        elect_all_port_roles(&network);

        // 4. Update Port States (Move from BLOCKING -> LISTENING, etc.)
        // update_all_port_states(&network);

        sleep(2);
    }

    return 0;
}
