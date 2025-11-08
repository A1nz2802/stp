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

    int tick_count = 1;

    // The Simulation Engine Loop
    while (1) {
        printf("\n=================== TICK %d ===================\n", tick_count);

        send_all_bpdus(&network);
        process_all_bpdus(&network);
        elect_all_port_roles(&network);
        update_all_port_states(&network);
        print_network_status(&network);

        sleep(1);
        tick_count++;
    }

    return 0;
}
