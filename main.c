#include "stp.h"
#include <stdio.h>

int main() {
    printf("Starting STP simulator...\n");

    // Create the switches
    struct Topology network = init_topology();

    // Connect the cables
    connect_topology(&network);

    printf("Create and connected %d switches.\n", NUM_SWITCHES);

    return 0;
}
