/**
 * @file stp.h
 * @brief Defines the data structures and function prototypes for a Spanning Tree Protocol (STP) simulator.
 */

#include <inttypes.h>

// --- Constants ---
#define MAX_PORTS 16     ///< Maximum number of ports a switch can have.
#define NUM_SWITCHES 3   ///< Number of switches to create in the topology.
#define MAX_BPDU_QUEUE 8 ///< Max BPDUs a port can hold in its inbox per tick.

#define BPDU_IS_BETTER -1 ///< The new BPDU is better
#define BPDU_IS_EQUAL 0   ///< The BPDUs are identical
#define BPDU_IS_WORST 1   ///< The new BPDU is worst

#define FORWARD_DELAY_SECONDS 15 ///< Time to wait in LISTENING and LEARNING

// ---------- Enums ----------

/** @brief Defines the operational states of a port according to STP. */
enum PortState {
    BLOCKING,   ///< Port discards frames, does not learn MACs, but listens to BPDUs.
    LISTENING,  ///< Port discards frames, does not learn MACs, but processes BPDUs.
    LEARNING,   ///< Port discards frames, but learns MAC addresses.
    FORWARDING, ///< Port forwards frames, learns MACs, and is fully operational.
    DISABLED    ///< Port is administratively shut down.
};

/** @brief Defines the calculated role of a port in the STP topology. */
enum PortRole {
    ROOT,          ///< The port with the best (lowest cost) path to the Root Bridge.
    DESIGNATED,    ///< The port responsible for sending BPDUs and traffic onto a network segment.
    NON_DESIGNATED ///< A blocked port that is neither Root nor Designated (redundant link).
};

// ---------- Structs ----------

/**
 * @brief Represents a Bridge Protocol Data Unit (BPDU).
 * This is the message packet switches use to communicate STP information.
 */
struct Bpdu {
    uint64_t root_id;        ///< The BID of the switch believed to be the Root Bridge.
    uint32_t root_path_cost; ///< The total accumulated cost to reach the Root Bridge.
    uint64_t bridge_id;      ///< The BID of the switch sending this BPDU.
    uint16_t port_id;        ///< The Port ID of the port sending this BPDU.

    // temporized fields
    uint16_t message_age;   ///< Time elapsed since the Root originated this info.
    uint16_t max_age;       ///< Time this BPDU should be stored before being discarded.
    uint16_t hello_time;    ///< The interval between BPDUs (dictated by the Root).
    uint16_t forward_delay; ///< The time to wait in LISTENING and LEARNING states.
};

/**
 * @brief Represents a physical port on a switch.
 *
 * ///  Link Speed   | Cost   ///
 * /// --------------|------- ///
 * ///   10  Gbps    |  2     ///
 * ///   1   Gbps    |  4     ///
 * ///   100 Mbps    |  19    ///
 * ///   10  Mbps    |  100   ///
 */
struct Port {
    uint16_t port_priority;                 ///< Port priority (default 128), used for tie-breaking.
    int port_number;                        ///< Physical identifier of the port (e.g., 1, 2, 3...).
    uint16_t port_id;                       ///< 16-bit combination of port priority and number.
    int port_cost;                          ///< Link cost (e.g., 19 for 100Mbps, 4 for 1Gbps).
    enum PortState state;                   ///< Current operational state (BLOCKING, FORWARDING, etc.).
    enum PortRole role;                     ///< Calculated role in the tree (ROOT, DESIGNATED, etc.).
    struct Bpdu stored_bpdu;                ///< The "best" BPDU this port has ever received.
    struct Switch *connected_switch;        ///< Pointer to the neighbor switch (for simulation only).
    struct Bpdu bpdu_inbox[MAX_BPDU_QUEUE]; ///< The inbox for BPDUs received *during* the current simulation tick.
    int inbox_count;                        ///< The number of BPDUs currently waiting in the bpdu_inbox.
    struct Port *connected_port;            ///< Direct pointer to the neighbor's port.
    int state_timer;                        ///< A countdown timer (in seconds) for LISTENING and LEARNING states.
};

/**
 * @brief Represents a network Switch (or Bridge) running STP.
 */
struct Switch {
    uint16_t bridge_priority;     ///< Configurable priority (default 32768) for Root election.
    uint64_t mac_address;         ///< Unique 6-byte hardware (MAC) address of the switch.
    struct Bpdu my_bpdu;          ///< The BPDU template this switch originates if it's Root.
    struct Port ports[MAX_PORTS]; ///< Array of all ports on this switch.
    int root_port_index;          ///< Index in the 'ports' array for the Root Port (-1 = none).
};

/**
 * @brief Holds the entire simulated network topology.
 */
struct Topology {
    struct Switch switches[NUM_SWITCHES]; ///< Array of all switches in the simulation.
};

/**
 * @brief Connects the ports of the switches in the topology.
 * @details This function creates a bi-directional link that the simulation
 * loop will use to pass BPDUs between switches and creates a 3-switch triangle topology.
 *
 * @param topology A pointer to the Topology struct to be modified.
 */
void connect_topology(struct Topology *topology);

/**
 * @brief Creates and initializes the simulation's network topology.
 * @details This function represents the "power-on" state of the network,
 * *before* any STP convergence has occurred. It sets up the default
 * values for each switch based on real-world hardware.
 *
 * @return A `struct Topology` (by value) containing the initialized switches.
 */
struct Topology init_topology(void);

/**
 * @brief The BID is a 64-bit value used to uniquely identify a switch and determine the
 * Root Bridge. It's a composite of the 16-bit bridge priority and the 48-bit MAC address.
 *
 * @param sw Pointer to the switch.
 * @return The 64-bit calculated BID.
 */
uint64_t get_switch_bid(struct Switch *sw);

/**
 * @brief Delivers a new BPDU to a port's inbox.
 * This simulates the physical reception of a BPDU frame.
 * @param port The port that is receiving the BPDU.
 * @param bpdu The BPDU being delivered.
 */
void deliver_bpdu_to_port(struct Port *port, struct Bpdu *bpdu);

/**
 * @brief Clears all BPDUs from a port's inbox.
 * Called after all BPDUs for a tick have been processed.
 * @param port The port whose inbox will be cleared.
 */
void clear_port_inbox(struct Port *port);

/**
 * @brief Sends a BPDU out of a specific port.
 * This function finds the connected neighbor port and
 * calls deliver_bpdu_to_port() on it.
 * @param port The *sending* port.
 * @param bpdu The BPDU to send.
 */
void send_bpdu_on_port(struct Port *port, struct Bpdu *bpdu);

/**
 * @brief Orchestrator function for the "send" phase of a simulation tick.
 * @param topology A pointer to the entire network topology.
 */
void send_all_bpdus(struct Topology *topology);

/**
 * @brief Compares two BPDUs to determine which is superior (better).
 * Follows the 4-step STP comparison logic.
 * @param new_bpdu The new, incoming BPDU.
 * @param stored_bpdu The port's currently stored "best" BPDU.
 * @return BPDU_IS_BETTER (-1), BPDU_IS_EQUAL (0), or BPDU_IS_WORST (1).
 */
int compare_bpdus(struct Bpdu *new_bpdu, struct Bpdu *stored_bpdu);

/**
 * @brief Orchestrator for the "process" phase of a simulation tick.
 * Loops through all ports, reads their inboxes, compares incoming BPDUs
 * to the stored BPDU, and updates the stored BPDU if a better one is found.
 * @param topology A pointer to the entire network topology.
 */
void process_all_bpdus(struct Topology *topology);

/**
 * @brief Orchestrator for the "election" phase of a simulation tick.
 * This function is the "brain" of STP. It loops through all switches
 * and makes them re-evaluate their role (Root or Non-Root),
 * elect their Root Port, and elect their Designated/Non-Designated ports.
 * @param topology A pointer to the entire network topology.
 */
void elect_all_port_roles(struct Topology *topology);

/**
 * @brief Orchestrator for the "state transition" phase of a simulation tick.
 * This function checks the 'role' of each port (decided by elect_all_port_roles)
 * and updates its 'state' (BLOCKING, LISTENING, etc.) accordingly,
 * managing the 15-second Forward Delay timers.
 * @param topology A pointer to the entire network topology.
 */
void update_all_port_states(struct Topology *topology);
