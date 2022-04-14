This is the IntAirNet modified GPSR repository. The targets to build and run the simulation is available at /simulations/Makefile

# Scenarios
Each scenario is described in a respective `<scenario>.ini` file.

## omnetpp.ini
An example scenario with 2 nodes and a ground station.
It also configures all nodes to use the `TdmaInterface`.
- It contains two configurations:
    - intairnet-gpsr-without-a2g: consider all nodes to use wireless transmission for A2A/A2G.
    - intairnet-gpsr: consider all nodes to use wireless transmission for A2A links and ethernet transmission for A2G links.

## intairnet.ini
An example scenario with the full mobility data in the NAC for 30 mins.
It also configures all nodes to use the `TdmaInterface`.
- It contains two configurations:
    - original-gpsr: consider all nodes to use wireless transmission for A2A/A2G.
    - intairnet-gpsr: consider all nodes to use wireless transmission for A2A links and ethernet transmission for A2G links.
