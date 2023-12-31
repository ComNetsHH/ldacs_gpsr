//
// Copyright (C) 2013 Opensim Ltd
// Author: Levente Meszaros
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//

#include <algorithm>

#include "inet/common/INETUtils.h"
#include "inet/common/IProtocolRegistrationListener.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/networklayer/common/HopLimitTag_m.h"
#include "inet/networklayer/common/IpProtocolId_m.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/networklayer/common/L3Tools.h"
#include "inet/networklayer/common/NextHopAddressTag_m.h"
#include "inet/networklayer/contract/IInterfaceTable.h"
#include "GpsrModified.h"
#include <string>
#include<fstream>



#ifdef WITH_IPv4
#include "inet/networklayer/ipv4/Ipv4Header_m.h"
#endif

#ifdef WITH_IPv6
#include "inet/networklayer/ipv6/Ipv6ExtensionHeaders_m.h"
#include "inet/networklayer/ipv6/Ipv6InterfaceData.h"
#endif

#ifdef WITH_NEXTHOP
#include "inet/networklayer/nexthop/NextHopForwardingHeader_m.h"
#endif

using namespace inet;

Define_Module(GpsrModified);

static inline double determinant(double a1, double a2, double b1, double b2)
{
    return a1 * b2 - a2 * b1;
}

GpsrModified::GpsrModified()
{
}

GpsrModified::~GpsrModified()
{
    cancelAndDelete(beaconTimer);
    cancelAndDelete(purgeNeighborsTimer);
}

//
// module interface
//

void GpsrModified::initialize(int stage)
{
    if (stage == INITSTAGE_ROUTING_PROTOCOLS)
        addressType = getSelfAddress().getAddressType();

    RoutingProtocolBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        // Gpsr parameters
        const char *planarizationModeString = par("planarizationMode");
        if (!strcmp(planarizationModeString, ""))
            planarizationMode = GPSR_NO_PLANARIZATION;
        else if (!strcmp(planarizationModeString, "GG"))
            planarizationMode = GPSR_GG_PLANARIZATION;
        else if (!strcmp(planarizationModeString, "RNG"))
            planarizationMode = GPSR_RNG_PLANARIZATION;
        else
            throw cRuntimeError("Unknown planarization mode");
        interfaces = par("interfaces");
        beaconInterval = par("beaconInterval");
        maxJitter = par("maxJitter");
        neighborValidityInterval = par("neighborValidityInterval");
        displayBubbles = par("displayBubbles");
        // MULTI-LINK
        useMultiLink = par("useMultiLink");
        useIntelligentMultiLinkMode = par("useIntelligentMultiLinkMode");
        multiLinkCutoffDistance = m(par("multiLinkCutoffDistance"));
        pSatcom = par("pSatcom");
        // context
        host = getContainingNode(this);
        interfaceTable = getModuleFromPar<IInterfaceTable>(par("interfaceTableModule"), this);
        outputInterface = par("outputInterface");
        mobility = check_and_cast<IMobility *>(host->getSubmodule("mobility"));
        routingTable = getModuleFromPar<IRoutingTable>(par("routingTableModule"), this);
        networkProtocol = getModuleFromPar<INetfilter>(par("networkProtocolModule"), this);
        // internal
        beaconTimer = new cMessage("BeaconTimer");
        purgeNeighborsTimer = new cMessage("PurgeNeighborsTimer");
        // packet size
        positionByteLength = par("positionByteLength");
        // KLUDGE: implement position registry protocol
        globalPositionTable.clear();
        //////////////////////////////////////////////////////////////////////////
        // Cross-layer routing (Musab)
        //////////////////////////////////////////////////////////////////////////
        globalPositionCongestionLevelTable.clear();
        //////////////////////////////////////////////////////////////////////////
        // Register Hop Count Signal (Musab)
        //////////////////////////////////////////////////////////////////////////
        hopCountSignal = registerSignal("hopCount");
        //////////////////////////////////////////////////////////////////////////
        // Route not found Signal (Musab)
        //////////////////////////////////////////////////////////////////////////
        routingFailedSignal = registerSignal("routingFailed");
        //////////////////////////////////////////////////////////////////////////
        // Switch to Perimeter Routing Signal (Musab)
        //////////////////////////////////////////////////////////////////////////
        greedyForwardingFailedSignal = registerSignal("greedyForwardingFailed");
        //////////////////////////////////////////////////////////////////////////
        // The ground station communication range + a2gOutputInterface (Musab)
        //////////////////////////////////////////////////////////////////////////
        groundStationRange = m(par("groundStationRange"));
        GSx = par("GSx");
        GSy = par("GSy");
        GSz = par("GSz");
        const char *file_name = par("groundstationsTraceFile");
        parseGroundstationTraceFile2Vector(file_name);
        a2gOutputInterface = par("a2gOutputInterface");
        //new code added here
        int destination_index = findClosestGroundStation();
        //////////////////////////////////////////////////////////////////////////
        // Cross-layer routing (Musab)
        //////////////////////////////////////////////////////////////////////////
        weightingFactor = par("weightingFactor"); 
        congestionLevel = par("congestionLevel"); 
        enableCrossLayerRouting = par("enableCrossLayerRouting");
        // packet size
        congestionLevelByteLength = par("congestionLevelByteLength");
        //////////////////////////////////////////////////////////////////////////
        // Enable/Disable creation of beacons (Musab)
        //////////////////////////////////////////////////////////////////////////
        beaconForwardedFromGpsr = par("beaconForwardedFromGpsr");
    }
    else if (stage == INITSTAGE_ROUTING_PROTOCOLS) {
        registerService(Protocol::manet, nullptr, gate("ipIn"));
        registerProtocol(Protocol::manet, gate("ipOut"), nullptr);
        host->subscribe(linkBrokenSignal, this);
        networkProtocol->registerHook(0, this);
        if (enableCrossLayerRouting)
            WATCH(neighborPositionCongestionLevelTable);
        else
            WATCH(neighborPositionTable);
    }
}

void GpsrModified::handleMessageWhenUp(cMessage *message)
{
    if (message->isSelfMessage())
        processSelfMessage(message);
    else
        processMessage(message);
}

//
// handling messages
//

void GpsrModified::processSelfMessage(cMessage *message)
{
    if (message == beaconTimer)
        processBeaconTimer();
    else if (message == purgeNeighborsTimer)
        processPurgeNeighborsTimer();
    else
        throw cRuntimeError("Unknown self message");
}

void GpsrModified::processMessage(cMessage *message)
{
    if (auto pk = dynamic_cast<Packet *>(message))
        processUdpPacket(pk);
    else
        throw cRuntimeError("Unknown message");
}

//////////////////////////////////////////////////////////////////////////
// Allow multuple groundstations (Musab) 
//////////////////////////////////////////////////////////////////////////
//The following function reads a trace file to store the coordinates of the ground stations
void GpsrModified::parseGroundstationTraceFile2Vector(const char* file_name)
{
    // std::vector<double> groundstationCoordinate;
    std::ifstream in(file_name, std::ios::in);
    // Check if the file is opened (we modified the error message here to just  return in order to enable scripting the application)
    if (in.fail()){
         throw std::invalid_argument( "file does not exist" );
    }
    std::string lineStr;
    std::string ethernetInterface;
    double x;
    double y;
    double z;
    std::istringstream iss;
    // Read the file line by line until the end.
    while (std::getline(in, lineStr))
    {
        std::istringstream iss(lineStr);
        iss >> x >> y >> z >> ethernetInterface;
        //std::cout << "x=" << x << ",y=" << y << ",z=" << z << ",interface=" << ethernetInterface << std::endl;
        // insert the x, y, z coordinates of one groundstation into groundstationCoordinate vector
        // groundstationCoordinate.insert(groundstationCoordinate.end(), { x,y,z });
        // ground_stations_coordinates_array.insert(ground_stations_coordinates_array.end(), { groundstationCoordinate });
        ground_stations_coordinates_array.push_back({x,y,z });
        // clear the vector that contains a row of the grounstation trace file
        // groundstationCoordinate.clear();
        // insert the groundstationCoordinate vector into the vector of vectors groundstationCoordinates
        ethernet_vector.insert(ethernet_vector.end(), { ethernetInterface });
    }
    int num_of_GS = ground_stations_coordinates_array.size();
    for (int i = 0; i < num_of_GS; i++){
        EV_INFO << "Ground station " << i << " location: " << ground_stations_coordinates_array.at(i).at(0) << ", " << ground_stations_coordinates_array.at(i).at(1) << ", " << ground_stations_coordinates_array.at(i).at(2) << endl;
    }
    // auto array_test = ground_stations_coordinates_array;
    // auto array_test1 = ground_stations_coordinates_array[0];
    // auto array_test2= ground_stations_coordinates_array[1];
    // Close The File
    in.close();
}

//////////////////////////////////////////////////////////////////////////
// The ground station communication range + a2gOutputInterface (Musab)
//////////////////////////////////////////////////////////////////////////
////The following function calculates distance to all ground stations and finds the closest one
int GpsrModified::findClosestGroundStation()
{
    int closest_ground_station=0;
    //getting the position of the current aircraft
    Coord aircraft_position = check_and_cast<IMobility *>(getContainingNode(this)->getSubmodule("mobility"))->getCurrentPosition();
    EV << " aircraft_position: " << aircraft_position << " \n";
    
    std::vector<double> distance_vector;
    //applying the formula sqrt((x_2-x_1)^2+(y_2-y_1)^2+(z_2-z1)^2) to get the distance between aircraft and all ground stations
    double min_distance = sqrt(pow((aircraft_position.x-ground_stations_coordinates_array.at(0).at(0)),2) + pow((aircraft_position.y-ground_stations_coordinates_array.at(0).at(1)),2)+ pow((aircraft_position.z-ground_stations_coordinates_array.at(0).at(2)),2));
    int num_of_GS = ground_stations_coordinates_array.size();
    for(int i = 0; i < num_of_GS; i++){
        double distance = sqrt(pow((aircraft_position.x-ground_stations_coordinates_array.at(i).at(0)),2) + pow((aircraft_position.y-ground_stations_coordinates_array.at(i).at(1)),2)+ pow((aircraft_position.z-ground_stations_coordinates_array.at(i).at(2)),2));
        distance_vector.push_back(distance);

        //find the closest ground station
        if (distance < min_distance){
            min_distance = distance;
            closest_ground_station=i;
        }
    }
    EV << " closest_ground_station is: " << closest_ground_station << " \n";
    EV << " Corresponding Ethernet is: " << ethernet_vector[closest_ground_station] << " \n";
    //new code upto here
    return closest_ground_station;
}


//
// beacon timers
//

void GpsrModified::scheduleBeaconTimer()
{
    EV_DEBUG << "Scheduling beacon timer" << endl;
    scheduleAt(simTime() + beaconInterval + uniform(-1, 1) * maxJitter, beaconTimer);
}

void GpsrModified::processBeaconTimer()
{
    EV_DEBUG << "Processing beacon timer" << endl;
    const L3Address selfAddress = getSelfAddress();
    if (!selfAddress.isUnspecified()) {
        //////////////////////////////////////////////////////////////////////////
        // Omit the sending of beacons from gpsr (Musab)
        //////////////////////////////////////////////////////////////////////////
        if(beaconForwardedFromGpsr) {
            //////////////////////////////////////////////////////////////////////////
            // Cross-layer routing (Musab)
            //////////////////////////////////////////////////////////////////////////
            if (enableCrossLayerRouting)
                sendBeaconCongestionLevel(createBeaconCongestionLevel());
            else
                sendBeacon(createBeacon());
            // sendBeacon(createBeacon());
        }
        //////////////////////////////////////////////////////////////////////////
        // Cross-layer routing (Musab)
        //////////////////////////////////////////////////////////////////////////
        if (enableCrossLayerRouting)
            storeSelfPositionCongestionLevelInGlobalRegistry();
        else
            storeSelfPositionInGlobalRegistry();
        // sendBeacon(createBeacon());
        // storeSelfPositionInGlobalRegistry();
    }
    scheduleBeaconTimer();
    schedulePurgeNeighborsTimer();
}

//
// handling purge neighbors timers
//

void GpsrModified::schedulePurgeNeighborsTimer()
{
    EV_DEBUG << "Scheduling purge neighbors timer" << endl;
    simtime_t nextExpiration = getNextNeighborExpiration();
    if (nextExpiration == SimTime::getMaxTime()) {
        if (purgeNeighborsTimer->isScheduled())
            cancelEvent(purgeNeighborsTimer);
    }
    else {
        if (!purgeNeighborsTimer->isScheduled())
            scheduleAt(nextExpiration, purgeNeighborsTimer);
        else {
            if (purgeNeighborsTimer->getArrivalTime() != nextExpiration) {
                cancelEvent(purgeNeighborsTimer);
                scheduleAt(nextExpiration, purgeNeighborsTimer);
            }
        }
    }
}

void GpsrModified::processPurgeNeighborsTimer()
{
    EV_DEBUG << "Processing purge neighbors timer" << endl;
    purgeNeighbors();
    schedulePurgeNeighborsTimer();
}

//
// handling UDP packets
//

void GpsrModified::sendUdpPacket(Packet *packet)
{
    send(packet, "ipOut");
}

void GpsrModified::processUdpPacket(Packet *packet)
{
    packet->popAtFront<UdpHeader>();
    //////////////////////////////////////////////////////////////////////////
    // Cross-layer routing (Musab)
    //////////////////////////////////////////////////////////////////////////
    if (enableCrossLayerRouting)
        processBeaconCongestionLevel(packet);
    else
        processBeacon(packet);
    // processBeacon(packet);
    schedulePurgeNeighborsTimer();
}

//
// handling beacons
//

const Ptr<GpsrBeaconModified> GpsrModified::createBeacon()
{
    const auto& beacon = makeShared<GpsrBeaconModified>();
    beacon->setAddress(getSelfAddress());
    beacon->setPosition(mobility->getCurrentPosition());
    beacon->setChunkLength(B(getSelfAddress().getAddressType()->getAddressByteLength() + positionByteLength));
    return beacon;
}

//////////////////////////////////////////////////////////////////////////
// Cross-layer routing (Musab)
//////////////////////////////////////////////////////////////////////////
const Ptr<GpsrBeaconCongestionLevelModified> GpsrModified::createBeaconCongestionLevel()
{
    const auto& beacon = makeShared<GpsrBeaconCongestionLevelModified>();
    auto selfCongestionLevel = congestionLevel;
    beacon->setAddress(getSelfAddress());
    beacon->setPosition(mobility->getCurrentPosition());
    beacon->setCongestionLevel(selfCongestionLevel);
    beacon->setChunkLength(B(getSelfAddress().getAddressType()->getAddressByteLength() + positionByteLength + congestionLevelByteLength));
    return beacon;
}

void GpsrModified::sendBeacon(const Ptr<GpsrBeaconModified>& beacon)
{
    EV_INFO << "Sending beacon: address = " << beacon->getAddress() << ", position = " << beacon->getPosition() << endl;
    Packet *udpPacket = new Packet("GpsrBeaconModified");
    udpPacket->insertAtBack(beacon);
    auto udpHeader = makeShared<UdpHeader>();
    udpHeader->setSourcePort(GPSR_UDP_PORT);
    udpHeader->setDestinationPort(GPSR_UDP_PORT);
    udpHeader->setCrcMode(CRC_DISABLED);
    udpPacket->insertAtFront(udpHeader);
    auto addresses = udpPacket->addTag<L3AddressReq>();
    addresses->setSrcAddress(getSelfAddress());
    addresses->setDestAddress(addressType->getLinkLocalManetRoutersMulticastAddress());
    udpPacket->addTag<HopLimitReq>()->setHopLimit(255);
    udpPacket->addTag<PacketProtocolTag>()->setProtocol(&Protocol::manet);
    udpPacket->addTag<DispatchProtocolReq>()->setProtocol(addressType->getNetworkProtocol());
    sendUdpPacket(udpPacket);
}

//////////////////////////////////////////////////////////////////////////
// Cross-layer routing (Musab)
//////////////////////////////////////////////////////////////////////////
void GpsrModified::sendBeaconCongestionLevel(const Ptr<GpsrBeaconCongestionLevelModified>& beacon)
{
    EV_INFO << "Sending beacon: address = " << beacon->getAddress() << ", position = " << beacon->getPosition() << ", congestion level = " << beacon->getCongestionLevel() << endl;
    Packet *udpPacket = new Packet("GpsrBeaconCongestionLevelModified");
    udpPacket->insertAtBack(beacon);
    auto udpHeader = makeShared<UdpHeader>();
    udpHeader->setSourcePort(GPSR_UDP_PORT);
    udpHeader->setDestinationPort(GPSR_UDP_PORT);
    udpHeader->setCrcMode(CRC_DISABLED);
    udpPacket->insertAtFront(udpHeader);
    auto addresses = udpPacket->addTag<L3AddressReq>();
    addresses->setSrcAddress(getSelfAddress());
    addresses->setDestAddress(addressType->getLinkLocalManetRoutersMulticastAddress());
    udpPacket->addTag<HopLimitReq>()->setHopLimit(255);
    udpPacket->addTag<PacketProtocolTag>()->setProtocol(&Protocol::manet);
    udpPacket->addTag<DispatchProtocolReq>()->setProtocol(addressType->getNetworkProtocol());
    sendUdpPacket(udpPacket);
}

void GpsrModified::processBeacon(Packet *packet)
{
    const auto& beacon = packet->peekAtFront<GpsrBeaconModified>();
    EV_INFO << "Processing beacon: address = " << beacon->getAddress() << ", position = " << beacon->getPosition() << endl;
    neighborPositionTable.setPosition(beacon->getAddress(), beacon->getPosition());
    delete packet;
}

//////////////////////////////////////////////////////////////////////////
// Cross-layer routing (Musab)
//////////////////////////////////////////////////////////////////////////
void GpsrModified::processBeaconCongestionLevel(Packet *packet)
{
    const auto& beacon = packet->peekAtFront<GpsrBeaconCongestionLevelModified>();
    EV_INFO << "Processing beacon: address = " << beacon->getAddress() << ", position = " << beacon->getPosition() << ", congestion level = " << beacon->getCongestionLevel() << endl;
    neighborPositionCongestionLevelTable.setPositionCongestionLevel(beacon->getAddress(), beacon->getPosition(), beacon->getCongestionLevel());
    delete packet;
}

void GpsrModified::processBeaconMCSOTDMA(const L3Address& address, const Coord& coord)
{
    EV_INFO << "Processing beacon: address = " << address << ", position = " << coord << endl;
    neighborPositionTable.setPosition(address, coord);
}

//////////////////////////////////////////////////////////////////////////
// Cross-layer routing (Musab)
//////////////////////////////////////////////////////////////////////////
void GpsrModified::processBeaconCongestionLevelMCSOTDMA(const L3Address& address, const Coord& coord, const int& congestionLevel)
{
    EV_INFO << "Processing beacon: address = " << address << ", position = " << coord << ", congestion level = " << congestionLevel << endl;
    neighborPositionCongestionLevelTable.setPositionCongestionLevel(address, coord, congestionLevel);
}

//
// handling packets
//

GpsrOption *GpsrModified::createGpsrOption(L3Address destination)
{
    GpsrOption *gpsrOption = new GpsrOption();
    gpsrOption->setRoutingMode(GPSR_GREEDY_ROUTING);
    //////////////////////////////////////////////////////////////////////////
    // Set the destination position (The ground station) in the GPSR packet (Musab)
    //////////////////////////////////////////////////////////////////////////
    int destination_index = findClosestGroundStation();
    // const Coord GroundStationLocation = Coord(GSx, GSy, GSz);
    const Coord GroundStationLocation = Coord(ground_stations_coordinates_array[destination_index][0], ground_stations_coordinates_array[destination_index][1], ground_stations_coordinates_array[destination_index][2]);
    EV_INFO << "Ground station (Destination) position = " << GroundStationLocation << endl;
    gpsrOption->setDestinationPosition(GroundStationLocation);
    // gpsrOption->setDestinationIndex(destination_index);
//    gpsrOption->setDestinationPosition(lookupPositionInGlobalRegistry(destination));
    gpsrOption->setLength(computeOptionLength(gpsrOption));
//    //////////////////////////////////////////////////////////////////////////
//    // Emit Hop Count Signal (Musab)
//    //////////////////////////////////////////////////////////////////////////
//    // Initially create a packet with a hop count value of 0 (Musab)
//    gpsrOption->setHopCount(0);
    return gpsrOption;
}

int GpsrModified::computeOptionLength(GpsrOption *option)
{
    // routingMode
    int routingModeBytes = 1;
    // destinationPosition, perimeterRoutingStartPosition, perimeterRoutingForwardPosition
    int positionsBytes = 3 * positionByteLength;
    // currentFaceFirstSenderAddress, currentFaceFirstReceiverAddress, senderAddress
    int addressesBytes = 3 * getSelfAddress().getAddressType()->getAddressByteLength();
    // type and length
    int tlBytes = 1 + 1;

    return tlBytes + routingModeBytes + positionsBytes + addressesBytes;
}

//
// configuration
//

void GpsrModified::configureInterfaces()
{
    // join multicast groups
    cPatternMatcher interfaceMatcher(interfaces, false, true, false);
    for (int i = 0; i < interfaceTable->getNumInterfaces(); i++) {
        InterfaceEntry *interfaceEntry = interfaceTable->getInterface(i);
        if (interfaceEntry->isMulticast() && interfaceMatcher.matches(interfaceEntry->getInterfaceName()))
            interfaceEntry->joinMulticastGroup(addressType->getLinkLocalManetRoutersMulticastAddress());
    }
}

//
// position
//

// KLUDGE: implement position registry protocol
PositionTableModified GpsrModified::globalPositionTable;

// KLUDGE: implement position registry protocol
//////////////////////////////////////////////////////////////////////////
// Cross-layer routing (Musab)
//////////////////////////////////////////////////////////////////////////
PositionTableCongestionLevelModified GpsrModified::globalPositionCongestionLevelTable;

Coord GpsrModified::lookupPositionInGlobalRegistry(const L3Address& address) const
{
    //////////////////////////////////////////////////////////////////////////
    // Cross-layer routing (Musab)
    //////////////////////////////////////////////////////////////////////////
    if (enableCrossLayerRouting)
        return globalPositionCongestionLevelTable.getPosition(address);
    else
        return globalPositionTable.getPosition(address);
    // // KLUDGE: implement position registry protocol
    // return globalPositionTable.getPosition(address);
}

void GpsrModified::storePositionInGlobalRegistry(const L3Address& address, const Coord& position) const
{
    // KLUDGE: implement position registry protocol
    globalPositionTable.setPosition(address, position);
}

//////////////////////////////////////////////////////////////////////////
// Cross-layer routing (Musab)
//////////////////////////////////////////////////////////////////////////
void GpsrModified::storePositionCongestionLevelInGlobalRegistry(const L3Address& address, const Coord& position, const int& congestionLevel) const
{
    // KLUDGE: implement position registry protocol
    globalPositionCongestionLevelTable.setPositionCongestionLevel(address, position, congestionLevel);
}

void GpsrModified::storeSelfPositionInGlobalRegistry() const
{
    auto selfAddress = getSelfAddress();
    if (!selfAddress.isUnspecified())
        storePositionInGlobalRegistry(selfAddress, mobility->getCurrentPosition());
}

//////////////////////////////////////////////////////////////////////////
// Cross-layer routing (Musab)
//////////////////////////////////////////////////////////////////////////
void GpsrModified::storeSelfPositionCongestionLevelInGlobalRegistry() const
{
    auto selfAddress = getSelfAddress();
    auto selfCongestionLevel = congestionLevel;
    if (!selfAddress.isUnspecified())
        storePositionCongestionLevelInGlobalRegistry(selfAddress, mobility->getCurrentPosition(), selfCongestionLevel);
}

Coord GpsrModified::computeIntersectionInsideLineSegments(Coord& begin1, Coord& end1, Coord& begin2, Coord& end2) const
{
    // NOTE: we must explicitly avoid computing the intersection points inside due to double instability
    if (begin1 == begin2 || begin1 == end2 || end1 == begin2 || end1 == end2)
        return Coord::NIL;
    else {
        double x1 = begin1.x;
        double y1 = begin1.y;
        double x2 = end1.x;
        double y2 = end1.y;
        double x3 = begin2.x;
        double y3 = begin2.y;
        double x4 = end2.x;
        double y4 = end2.y;
        double a = determinant(x1, y1, x2, y2);
        double b = determinant(x3, y3, x4, y4);
        double c = determinant(x1 - x2, y1 - y2, x3 - x4, y3 - y4);
        double x = determinant(a, x1 - x2, b, x3 - x4) / c;
        double y = determinant(a, y1 - y2, b, y3 - y4) / c;
        if ((x <= x1 && x <= x2) || (x >= x1 && x >= x2) || (x <= x3 && x <= x4) || (x >= x3 && x >= x4) ||
            (y <= y1 && y <= y2) || (y >= y1 && y >= y2) || (y <= y3 && y <= y4) || (y >= y3 && y >= y4))
            return Coord::NIL;
        else
            return Coord(x, y, 0);
    }
}

Coord GpsrModified::getNeighborPosition(const L3Address& address) const
{
    return neighborPositionTable.getPosition(address);
}

//////////////////////////////////////////////////////////////////////////
// Cross-layer routing (Musab)
//////////////////////////////////////////////////////////////////////////
Coord GpsrModified::getNeighborPositionCongestionLevel(const L3Address& address) const
{
    return neighborPositionCongestionLevelTable.getPosition(address);
}

//
// angle
//

double GpsrModified::getVectorAngle(Coord vector) const
{
    ASSERT(vector != Coord::ZERO);
    double angle = atan2(-vector.y, vector.x);
    if (angle < 0)
        angle += 2 * M_PI;
    return angle;
}

double GpsrModified::getNeighborAngle(const L3Address& address) const
{
    //////////////////////////////////////////////////////////////////////////
    // Cross-layer routing (Musab)
    //////////////////////////////////////////////////////////////////////////
    if (enableCrossLayerRouting)
        return getVectorAngle(getNeighborPositionCongestionLevel(address) - mobility->getCurrentPosition());
    else
        return getVectorAngle(getNeighborPosition(address) - mobility->getCurrentPosition());
    // return getVectorAngle(getNeighborPosition(address) - mobility->getCurrentPosition());
}

//
// address
//

std::string GpsrModified::getHostName() const
{
    return host->getFullName();
}

L3Address GpsrModified::getSelfAddress() const
{
    //TODO choose self address based on a new 'interfaces' parameter
    L3Address ret = routingTable->getRouterIdAsGeneric();
#ifdef WITH_IPv6
    if (ret.getType() == L3Address::IPv6) {
        for (int i = 0; i < interfaceTable->getNumInterfaces(); i++) {
            InterfaceEntry *ie = interfaceTable->getInterface(i);
            if ((!ie->isLoopback())) {
                if (auto ipv6Data = ie->findProtocolData<Ipv6InterfaceData>()) {
                    ret = ipv6Data->getPreferredAddress();
                    break;
                }
            }
        }
    }
#endif
    return ret;
}

L3Address GpsrModified::getSenderNeighborAddress(const Ptr<const NetworkHeaderBase>& networkHeader) const
{
    const GpsrOption *gpsrOption = getGpsrOptionFromNetworkDatagram(networkHeader);
    return gpsrOption->getSenderAddress();
}

//
// neighbor
//

simtime_t GpsrModified::getNextNeighborExpiration()
{
    simtime_t oldestPosition = neighborPositionTable.getOldestPosition();
    if (oldestPosition == SimTime::getMaxTime())
        return oldestPosition;
    else
        return oldestPosition + neighborValidityInterval;
}

void GpsrModified::purgeNeighbors()
{
    //////////////////////////////////////////////////////////////////////////
    // Cross-layer routing (Musab)
    //////////////////////////////////////////////////////////////////////////
    if (enableCrossLayerRouting)
        neighborPositionTable.removeOldPositions(simTime() - neighborValidityInterval);
    else
        neighborPositionCongestionLevelTable.removeOldPositions(simTime() - neighborValidityInterval);
    // neighborPositionTable.removeOldPositions(simTime() - neighborValidityInterval);
}

std::vector<L3Address> GpsrModified::getPlanarNeighbors() const
{
    std::vector<L3Address> planarNeighbors;
    // std::vector<L3Address> neighborAddresses = neighborPositionTable.getAddresses();
    //////////////////////////////////////////////////////////////////////////
    // Cross-layer routing (Musab)
    //////////////////////////////////////////////////////////////////////////
    std::vector<L3Address> neighborAddresses;
    if (enableCrossLayerRouting)
        neighborAddresses = neighborPositionCongestionLevelTable.getAddresses();
    else
        neighborAddresses = neighborPositionTable.getAddresses();
    Coord selfPosition = mobility->getCurrentPosition();
    for (auto it = neighborAddresses.begin(); it != neighborAddresses.end(); it++) {
        auto neighborAddress = *it;
        // Coord neighborPosition = neighborPositionTable.getPosition(neighborAddress);
        //////////////////////////////////////////////////////////////////////////
        // Cross-layer routing (Musab)
        //////////////////////////////////////////////////////////////////////////
        Coord neighborPosition;
        if (enableCrossLayerRouting)
            neighborPosition = neighborPositionCongestionLevelTable.getPosition(neighborAddress);
        else
            neighborPosition = neighborPositionTable.getPosition(neighborAddress);
        if (planarizationMode == GPSR_NO_PLANARIZATION)
            return neighborAddresses;
        else if (planarizationMode == GPSR_RNG_PLANARIZATION) {
            double neighborDistance = (neighborPosition - selfPosition).length();
            for (auto & witnessAddress : neighborAddresses) {
                // Coord witnessPosition = neighborPositionTable.getPosition(witnessAddress);
                //////////////////////////////////////////////////////////////////////////
                // Cross-layer routing (Musab)
                //////////////////////////////////////////////////////////////////////////
                Coord witnessPosition;
                if (enableCrossLayerRouting)
                    witnessPosition = neighborPositionCongestionLevelTable.getPosition(witnessAddress);
                else
                    witnessPosition = neighborPositionTable.getPosition(witnessAddress);
                double witnessDistance = (witnessPosition - selfPosition).length();
                double neighborWitnessDistance = (witnessPosition - neighborPosition).length();
                if (neighborAddress == witnessAddress)
                    continue;
                else if (neighborDistance > std::max(witnessDistance, neighborWitnessDistance))
                    goto eliminate;
            }
        }
        else if (planarizationMode == GPSR_GG_PLANARIZATION) {
            Coord middlePosition = (selfPosition + neighborPosition) / 2;
            double neighborDistance = (neighborPosition - middlePosition).length();
            for (auto & witnessAddress : neighborAddresses) {
                // Coord witnessPosition = neighborPositionTable.getPosition(witnessAddress);
                //////////////////////////////////////////////////////////////////////////
                // Cross-layer routing (Musab)
                //////////////////////////////////////////////////////////////////////////
                Coord witnessPosition;
                if (enableCrossLayerRouting)
                    witnessPosition = neighborPositionCongestionLevelTable.getPosition(witnessAddress);
                else
                    witnessPosition = neighborPositionTable.getPosition(witnessAddress);
                double witnessDistance = (witnessPosition - middlePosition).length();
                if (neighborAddress == witnessAddress)
                    continue;
                else if (witnessDistance < neighborDistance)
                    goto eliminate;
            }
        }
        else
            throw cRuntimeError("Unknown planarization mode");
        planarNeighbors.push_back(*it);
      eliminate:;
    }
    return planarNeighbors;
}

std::vector<L3Address> GpsrModified::getPlanarNeighborsCounterClockwise(double startAngle) const
{
    std::vector<L3Address> neighborAddresses = getPlanarNeighbors();
    std::sort(neighborAddresses.begin(), neighborAddresses.end(), [&](const L3Address& address1, const L3Address& address2) {
        // NOTE: make sure the neighbor at startAngle goes to the end
        auto angle1 = getNeighborAngle(address1) - startAngle;
        auto angle2 = getNeighborAngle(address2) - startAngle;
        if (angle1 <= 0)
            angle1 += 2 * M_PI;
        if (angle2 <= 0)
            angle2 += 2 * M_PI;
        return angle1 < angle2;
    });
    return neighborAddresses;
}

//
// next hop
//

L3Address GpsrModified::findNextHop(const L3Address& destination, GpsrOption *gpsrOption)
{
    //////////////////////////////////////////////////////////////////////////
    // Check whether the GS is in communication range (Musab)
    //////////////////////////////////////////////////////////////////////////
    m distanceToGroundStation = m(mobility->getCurrentPosition().distance(gpsrOption->getDestinationPosition()));
    EV_INFO << "The distance to the ground station = " << km(distanceToGroundStation) << endl;
    if (distanceToGroundStation <= groundStationRange) {
        EV_INFO << "The ground station is within communication range" << endl;
        return destination; // The next hop is the destination (the ground station)
    }
    switch (gpsrOption->getRoutingMode()) {
        case GPSR_GREEDY_ROUTING: return findGreedyRoutingNextHop(destination, gpsrOption);
        case GPSR_PERIMETER_ROUTING: return findPerimeterRoutingNextHop(destination, gpsrOption);
        default: throw cRuntimeError("Unknown routing mode");
    }
}

L3Address GpsrModified::findGreedyRoutingNextHop(const L3Address& destination, GpsrOption *gpsrOption)
{
    EV_DEBUG << "Finding next hop using greedy routing: destination = " << destination << endl;
    L3Address selfAddress = getSelfAddress();
    Coord selfPosition = mobility->getCurrentPosition();
    Coord destinationPosition = gpsrOption->getDestinationPosition();
    double bestDistance = (destinationPosition - selfPosition).length(); // distance of the closest neighbor to the destination
    double destinationDistance = (destinationPosition - selfPosition).length(); // distance of the current node to the distination
    int maxCongestionLevel = 4; // 1:uncongested, 2:slightly_congested, 3:moderately_congested, 4:congested
    float bestDistanceCongestionLevelRatio = 1; // neighbor that is closest to the GS than the current node and give the best distance/congestion level ratio
    L3Address bestNeighbor;
    // std::vector<L3Address> neighborAddresses = neighborPositionTable.getAddresses();
    //////////////////////////////////////////////////////////////////////////
    // Cross-layer routing (Musab)
    //////////////////////////////////////////////////////////////////////////
    std::vector<L3Address> neighborAddresses;
    if (enableCrossLayerRouting)
        neighborAddresses = neighborPositionCongestionLevelTable.getAddresses();
    else
        neighborAddresses = neighborPositionTable.getAddresses();
    for (auto& neighborAddress: neighborAddresses) {
        // Coord neighborPosition = neighborPositionTable.getPosition(neighborAddress);
        //////////////////////////////////////////////////////////////////////////
        // Cross-layer routing (Musab)
        //////////////////////////////////////////////////////////////////////////
        Coord neighborPosition;
        int neighborCongestionLevel;
        if (enableCrossLayerRouting){
            // If cross-layer routing is used we get additionally the congestion level
            neighborPosition = neighborPositionCongestionLevelTable.getPosition(neighborAddress);
            neighborCongestionLevel = neighborPositionCongestionLevelTable.getCongestionLevel(neighborAddress);
        }
        else
            neighborPosition = neighborPositionTable.getPosition(neighborAddress);
        double neighborDistance = (destinationPosition - neighborPosition).length();
        if (enableCrossLayerRouting){
            // If cross-layer routing is enabled best neighbor is selected based on the equation
            // best_ratio = alpha * neighborDistance / destinationDistance + (1 - alpha) * neighborCongestionLevel / maxCongestionLevel
            // This is applied only if neighborDistance < destinationDistance: the node is closer to the destination than the current node
            if (neighborDistance < destinationDistance){
                float neighborDistanceCongestionLevelRatio = (float)(weightingFactor * neighborDistance / destinationDistance) +(float) ((1 - weightingFactor) * neighborCongestionLevel / maxCongestionLevel);
                if (neighborDistanceCongestionLevelRatio < bestDistanceCongestionLevelRatio){    
                    bestDistanceCongestionLevelRatio = neighborDistanceCongestionLevelRatio;
                    bestNeighbor = neighborAddress;
                }
            }
        }
        else{
            // If the cross-layer routing is not enabled do it as conventional GPSR
            if (neighborDistance < bestDistance) {
                bestDistance = neighborDistance;
                bestNeighbor = neighborAddress;
            }
        }
        // if (neighborDistance < bestDistance) {
        //     bestDistance = neighborDistance;
        //     bestNeighbor = neighborAddress;
        // }
    }
    if (bestNeighbor.isUnspecified()) {
        EV_DEBUG << "Switching to perimeter routing: destination = " << destination << endl;
        //////////////////////////////////////////////////////////////////////////
        // Switch to Perimeter Routing Signal (Musab)
        //////////////////////////////////////////////////////////////////////////
        emit(greedyForwardingFailedSignal, simTime());
        if (displayBubbles && hasGUI())
            getContainingNode(host)->bubble("Switching to perimeter routing");
        gpsrOption->setRoutingMode(GPSR_PERIMETER_ROUTING);
        gpsrOption->setPerimeterRoutingStartPosition(selfPosition);
        gpsrOption->setPerimeterRoutingForwardPosition(selfPosition);
        gpsrOption->setCurrentFaceFirstSenderAddress(selfAddress);
        gpsrOption->setCurrentFaceFirstReceiverAddress(L3Address());
        return findPerimeterRoutingNextHop(destination, gpsrOption);
    }
    else
        return bestNeighbor;
}

L3Address GpsrModified::findPerimeterRoutingNextHop(const L3Address& destination, GpsrOption *gpsrOption)
{
    EV_DEBUG << "Finding next hop using perimeter routing: destination = " << destination << endl;
    L3Address selfAddress = getSelfAddress();
    Coord selfPosition = mobility->getCurrentPosition();
    Coord perimeterRoutingStartPosition = gpsrOption->getPerimeterRoutingStartPosition();
    Coord destinationPosition = gpsrOption->getDestinationPosition();
    double selfDistance = (destinationPosition - selfPosition).length();
    double perimeterRoutingStartDistance = (destinationPosition - perimeterRoutingStartPosition).length();
    if (selfDistance < perimeterRoutingStartDistance) {
        EV_DEBUG << "Switching to greedy routing: destination = " << destination << endl;
        if (displayBubbles && hasGUI())
            getContainingNode(host)->bubble("Switching to greedy routing");
        gpsrOption->setRoutingMode(GPSR_GREEDY_ROUTING);
        gpsrOption->setPerimeterRoutingStartPosition(Coord());
        gpsrOption->setPerimeterRoutingForwardPosition(Coord());
        gpsrOption->setCurrentFaceFirstSenderAddress(L3Address());
        gpsrOption->setCurrentFaceFirstReceiverAddress(L3Address());
        return findGreedyRoutingNextHop(destination, gpsrOption);
    }
    else {
        const L3Address& firstSenderAddress = gpsrOption->getCurrentFaceFirstSenderAddress();
        const L3Address& firstReceiverAddress = gpsrOption->getCurrentFaceFirstReceiverAddress();
        auto senderNeighborAddress = gpsrOption->getSenderAddress();
        auto neighborAngle = senderNeighborAddress.isUnspecified() ? getVectorAngle(destinationPosition - mobility->getCurrentPosition()) : getNeighborAngle(senderNeighborAddress);
        L3Address selectedNeighborAddress;
        std::vector<L3Address> neighborAddresses = getPlanarNeighborsCounterClockwise(neighborAngle);
        for (auto& neighborAddress : neighborAddresses) {
            //////////////////////////////////////////////////////////////////////////
            // Cross-layer routing (Musab)
            //////////////////////////////////////////////////////////////////////////
            Coord neighborPosition;
            if (enableCrossLayerRouting)
                neighborPosition = getNeighborPositionCongestionLevel(neighborAddress);
            else
                neighborPosition = getNeighborPosition(neighborAddress);
            // Coord neighborPosition = getNeighborPosition(neighborAddress);
            Coord intersection = computeIntersectionInsideLineSegments(perimeterRoutingStartPosition, destinationPosition, selfPosition, neighborPosition);
            if (std::isnan(intersection.x)) {
                selectedNeighborAddress = neighborAddress;
                break;
            }
            else {
                EV_DEBUG << "Edge to next hop intersects: intersection = " << intersection << ", nextNeighbor = " << selectedNeighborAddress << ", firstSender = " << firstSenderAddress << ", firstReceiver = " << firstReceiverAddress << ", destination = " << destination << endl;
                gpsrOption->setCurrentFaceFirstSenderAddress(selfAddress);
                gpsrOption->setCurrentFaceFirstReceiverAddress(L3Address());
                gpsrOption->setPerimeterRoutingForwardPosition(intersection);
            }
        }
        if (selectedNeighborAddress.isUnspecified()) {
            EV_DEBUG << "No suitable planar graph neighbor found in perimeter routing: firstSender = " << firstSenderAddress << ", firstReceiver = " << firstReceiverAddress << ", destination = " << destination << endl;
            return L3Address();
        }
        else if (firstSenderAddress == selfAddress && firstReceiverAddress == selectedNeighborAddress) {
            EV_DEBUG << "End of perimeter reached: firstSender = " << firstSenderAddress << ", firstReceiver = " << firstReceiverAddress << ", destination = " << destination << endl;
            if (displayBubbles && hasGUI())
                getContainingNode(host)->bubble("End of perimeter reached");
            return L3Address();
        }
        else {
            if (gpsrOption->getCurrentFaceFirstReceiverAddress().isUnspecified())
                gpsrOption->setCurrentFaceFirstReceiverAddress(selectedNeighborAddress);
            return selectedNeighborAddress;
        }
    }
}

//
// routing
//

INetfilter::IHook::Result GpsrModified::routeDatagram(Packet *datagram, GpsrOption *gpsrOption)
{
    const auto& networkHeader = getNetworkProtocolHeader(datagram);
    const L3Address& source = networkHeader->getSourceAddress();
    const L3Address& destination = networkHeader->getDestinationAddress();

    //// START MULTI_LINK 
    if(useMultiLink) {
        const auto& multiLinkPacketTag = datagram->findTag<MultiLinkPacketTag>();
        bool hasTag = multiLinkPacketTag != nullptr;

        if(hasTag && multiLinkPacketTag->isSatcom()) {
            auto satcomInterfaceEntry = CHK(interfaceTable->findInterfaceByName("ppp0"));
            datagram->addTagIfAbsent<InterfaceReq>()->setInterfaceId(satcomInterfaceEntry->getInterfaceId());
            return ACCEPT;

        }
    }
    //// END MULTI_LINK 



    EV_INFO << "Finding next hop: source = " << source << ", destination = " << destination << endl;
    auto nextHop = findNextHop(destination, gpsrOption);
    datagram->addTagIfAbsent<NextHopAddressReq>()->setNextHopAddress(nextHop);
    if (nextHop.isUnspecified()) {
        EV_WARN << "No next hop found, dropping packet: source = " << source << ", destination = " << destination << endl;
        //////////////////////////////////////////////////////////////////////////
        // Route not found Signal (Musab)
        //////////////////////////////////////////////////////////////////////////
        emit(routingFailedSignal, simTime());
        if (displayBubbles && hasGUI())
            getContainingNode(host)->bubble("No next hop found, dropping packet");
        return DROP;
    }
    else {
        EV_INFO << "Next hop found: source = " << source << ", destination = " << destination << ", nextHop: " << nextHop << endl;
        // EV_INFO << "getSelfAddress() = " << getSelfAddress() << endl;
        // EV_INFO << "gpsrOption->getDestinationIndex() = " << gpsrOption->getDestinationIndex() << endl;
        gpsrOption->setSenderAddress(getSelfAddress());
        // auto destination_index = gpsrOption->getDestinationIndex();
        auto interfaceEntry = CHK(interfaceTable->findInterfaceByName(outputInterface));
        //////////////////////////////////////////////////////////////////////////
        // Set interface to the interface used in A2G if next hop is GS (Musab)
        //////////////////////////////////////////////////////////////////////////
        // need to covert string to character array
        // auto a2gInterfaceEntry = CHK(interfaceTable->findInterfaceByName(ethernet_vector[destination_index].c_str()));
        // auto a2gInterfaceEntry = CHK(interfaceTable->findInterfaceByName(a2gOutputInterface));
        // EV_INFO << "A2G output interface = " << ethernet_vector[0] << endl;
        m distanceToGroundStation = m(mobility->getCurrentPosition().distance(gpsrOption->getDestinationPosition()));
        if (distanceToGroundStation <= groundStationRange) {
            int destination_index = findClosestGroundStation();
            auto a2gInterfaceEntry = CHK(interfaceTable->findInterfaceByName(ethernet_vector[destination_index].c_str()));
            EV_INFO << "OutputInterface = " << ethernet_vector[destination_index].c_str() << " Interface ID = " << a2gInterfaceEntry << endl;
            datagram->addTagIfAbsent<InterfaceReq>()->setInterfaceId(a2gInterfaceEntry->getInterfaceId());
            return ACCEPT;
        }
        datagram->addTagIfAbsent<InterfaceReq>()->setInterfaceId(interfaceEntry->getInterfaceId());
//        //////////////////////////////////////////////////////////////////////////
//        // Emit Hop Count Signal (Musab)
//        //////////////////////////////////////////////////////////////////////////
//        // Increment the hop count value by 1 (Musab)
//        gpsrOption->setHopCount(gpsrOption->getHopCount() + 1);
//        EV_INFO << "Current hop count = " << gpsrOption->getHopCount() << endl;
        return ACCEPT;
    }
}

void GpsrModified::setGpsrOptionOnNetworkDatagram(Packet *packet, const Ptr<const NetworkHeaderBase>& networkHeader, GpsrOption *gpsrOption)
{
    packet->trimFront();
#ifdef WITH_IPv4
    if (dynamicPtrCast<const Ipv4Header>(networkHeader)) {
        auto ipv4Header = removeNetworkProtocolHeader<Ipv4Header>(packet);
        gpsrOption->setType(IPOPTION_TLV_GPSR);
        B oldHlen = ipv4Header->calculateHeaderByteLength();
        ASSERT(ipv4Header->getHeaderLength() == oldHlen);
        ipv4Header->addOption(gpsrOption);
        B newHlen = ipv4Header->calculateHeaderByteLength();
        ipv4Header->setHeaderLength(newHlen);
        ipv4Header->addChunkLength(newHlen - oldHlen);
        ipv4Header->setTotalLengthField(ipv4Header->getTotalLengthField() + newHlen - oldHlen);
        insertNetworkProtocolHeader(packet, Protocol::ipv4, ipv4Header);
    }
    else
#endif
#ifdef WITH_IPv6
    if (dynamicPtrCast<const Ipv6Header>(networkHeader)) {
        auto ipv6Header = removeNetworkProtocolHeader<Ipv6Header>(packet);
        gpsrOption->setType(IPv6TLVOPTION_TLV_GPSR);
        B oldHlen = ipv6Header->calculateHeaderByteLength();
        Ipv6HopByHopOptionsHeader *hdr = check_and_cast_nullable<Ipv6HopByHopOptionsHeader *>(ipv6Header->findExtensionHeaderByTypeForUpdate(IP_PROT_IPv6EXT_HOP));
        if (hdr == nullptr) {
            hdr = new Ipv6HopByHopOptionsHeader();
            hdr->setByteLength(B(8));
            ipv6Header->addExtensionHeader(hdr);
        }
        hdr->getTlvOptionsForUpdate().insertTlvOption(gpsrOption);
        hdr->setByteLength(B(utils::roundUp(2 + B(hdr->getTlvOptions().getLength()).get(), 8)));
        B newHlen = ipv6Header->calculateHeaderByteLength();
        ipv6Header->addChunkLength(newHlen - oldHlen);
        insertNetworkProtocolHeader(packet, Protocol::ipv6, ipv6Header);
    }
    else
#endif
#ifdef WITH_NEXTHOP
    if (dynamicPtrCast<const NextHopForwardingHeader>(networkHeader)) {
        auto nextHopHeader = removeNetworkProtocolHeader<NextHopForwardingHeader>(packet);
        gpsrOption->setType(NEXTHOP_TLVOPTION_TLV_GPSR);
        int oldHlen = nextHopHeader->getTlvOptions().getLength();
        nextHopHeader->getTlvOptionsForUpdate().insertTlvOption(gpsrOption);
        int newHlen = nextHopHeader->getTlvOptions().getLength();
        nextHopHeader->addChunkLength(B(newHlen - oldHlen));
        insertNetworkProtocolHeader(packet, Protocol::nextHopForwarding, nextHopHeader);
    }
    else
#endif
    {
    }
}

const GpsrOption *GpsrModified::findGpsrOptionInNetworkDatagram(const Ptr<const NetworkHeaderBase>& networkHeader) const
{
    const GpsrOption *gpsrOption = nullptr;

#ifdef WITH_IPv4
    if (auto ipv4Header = dynamicPtrCast<const Ipv4Header>(networkHeader)) {
        gpsrOption = check_and_cast_nullable<const GpsrOption *>(ipv4Header->findOptionByType(IPOPTION_TLV_GPSR));
    }
    else
#endif
#ifdef WITH_IPv6
    if (auto ipv6Header = dynamicPtrCast<const Ipv6Header>(networkHeader)) {
        const Ipv6HopByHopOptionsHeader *hdr = check_and_cast_nullable<const Ipv6HopByHopOptionsHeader *>(ipv6Header->findExtensionHeaderByType(IP_PROT_IPv6EXT_HOP));
        if (hdr != nullptr) {
            int i = (hdr->getTlvOptions().findByType(IPv6TLVOPTION_TLV_GPSR));
            if (i >= 0)
                gpsrOption = check_and_cast<const GpsrOption *>(hdr->getTlvOptions().getTlvOption(i));
        }
    }
    else
#endif
#ifdef WITH_NEXTHOP
    if (auto nextHopHeader = dynamicPtrCast<const NextHopForwardingHeader>(networkHeader)) {
        int i = (nextHopHeader->getTlvOptions().findByType(NEXTHOP_TLVOPTION_TLV_GPSR));
        if (i >= 0)
            gpsrOption = check_and_cast<const GpsrOption *>(nextHopHeader->getTlvOptions().getTlvOption(i));
    }
    else
#endif
    {
    }
    return gpsrOption;
}

GpsrOption *GpsrModified::findGpsrOptionInNetworkDatagramForUpdate(const Ptr<NetworkHeaderBase>& networkHeader)
{
    GpsrOption *gpsrOption = nullptr;

#ifdef WITH_IPv4
    if (auto ipv4Header = dynamicPtrCast<Ipv4Header>(networkHeader)) {
        gpsrOption = check_and_cast_nullable<GpsrOption *>(ipv4Header->findMutableOptionByType(IPOPTION_TLV_GPSR));
    }
    else
#endif
#ifdef WITH_IPv6
    if (auto ipv6Header = dynamicPtrCast<Ipv6Header>(networkHeader)) {
        Ipv6HopByHopOptionsHeader *hdr = check_and_cast_nullable<Ipv6HopByHopOptionsHeader *>(ipv6Header->findExtensionHeaderByTypeForUpdate(IP_PROT_IPv6EXT_HOP));
        if (hdr != nullptr) {
            int i = (hdr->getTlvOptions().findByType(IPv6TLVOPTION_TLV_GPSR));
            if (i >= 0)
                gpsrOption = check_and_cast<GpsrOption *>(hdr->getTlvOptionsForUpdate().getTlvOptionForUpdate(i));
        }
    }
    else
#endif
#ifdef WITH_NEXTHOP
    if (auto nextHopHeader = dynamicPtrCast<NextHopForwardingHeader>(networkHeader)) {
        int i = (nextHopHeader->getTlvOptions().findByType(NEXTHOP_TLVOPTION_TLV_GPSR));
        if (i >= 0)
            gpsrOption = check_and_cast<GpsrOption *>(nextHopHeader->getTlvOptionsForUpdate().getTlvOptionForUpdate(i));
    }
    else
#endif
    {
    }
    return gpsrOption;
}

const GpsrOption *GpsrModified::getGpsrOptionFromNetworkDatagram(const Ptr<const NetworkHeaderBase>& networkHeader) const
{
    const GpsrOption *gpsrOption = findGpsrOptionInNetworkDatagram(networkHeader);
    if (gpsrOption == nullptr)
        throw cRuntimeError("Gpsr option not found in datagram!");
    return gpsrOption;
}

GpsrOption *GpsrModified::getGpsrOptionFromNetworkDatagramForUpdate(const Ptr<NetworkHeaderBase>& networkHeader)
{
    GpsrOption *gpsrOption = findGpsrOptionInNetworkDatagramForUpdate(networkHeader);
    if (gpsrOption == nullptr)
        throw cRuntimeError("Gpsr option not found in datagram!");
    return gpsrOption;
}

//
// netfilter
//

INetfilter::IHook::Result GpsrModified::datagramPreRoutingHook(Packet *datagram)
{
    Enter_Method("datagramPreRoutingHook");
    const auto& networkHeader = getNetworkProtocolHeader(datagram);
    const L3Address& destination = networkHeader->getDestinationAddress();
    if (destination.isMulticast() || destination.isBroadcast() || routingTable->isLocalAddress(destination))
        return ACCEPT;
    else {
        // KLUDGE: this allows overwriting the GPSR option inside
        auto gpsrOption = const_cast<GpsrOption *>(getGpsrOptionFromNetworkDatagram(networkHeader));
        return routeDatagram(datagram, gpsrOption);
    }
}

INetfilter::IHook::Result GpsrModified::datagramLocalOutHook(Packet *packet)
{
    Enter_Method("datagramLocalOutHook");

    if(useMultiLink) {
        if(useIntelligentMultiLinkMode) {
            const Coord groundStationLocation = Coord(GSx, GSy, GSz);
            m distanceToGroundStation = m(mobility->getCurrentPosition().distance(groundStationLocation));
            packet->addTagIfAbsent<MultiLinkPacketTag>()->setIsSatcom(distanceToGroundStation >= multiLinkCutoffDistance);
        } else {
            packet->addTagIfAbsent<MultiLinkPacketTag>()->setIsSatcom(uniform(0, 1.0) <= pSatcom);
        }
    }

    const auto& networkHeader = getNetworkProtocolHeader(packet);
    const L3Address& destination = networkHeader->getDestinationAddress();
    if (destination.isMulticast() || destination.isBroadcast() || routingTable->isLocalAddress(destination))
        return ACCEPT;
    else {
        GpsrOption *gpsrOption = createGpsrOption(networkHeader->getDestinationAddress());
        setGpsrOptionOnNetworkDatagram(packet, networkHeader, gpsrOption);
        return routeDatagram(packet, gpsrOption);
    }
}

//
// This allows emitting the hop count signal for application packets only (Musab)
//

INetfilter::IHook::Result GpsrModified::datagramLocalInHook(Packet *packet)
{
    Enter_Method("datagramLocalInHook");
    const auto& ipv4Header = packet->peekAtFront<Ipv4Header>();
    const auto& networkHeader = getNetworkProtocolHeader(packet);
    const L3Address& destination = networkHeader->getDestinationAddress();

    //////////////////////////////////////////////////////////////////////////
    // Emit Hop Count Signal (Musab)
    //////////////////////////////////////////////////////////////////////////
    const GpsrOption *gpsrOption = findGpsrOptionInNetworkDatagram(networkHeader);
    // Emit signal only in the case if gpsrOption exist (Musab)
    if (gpsrOption != nullptr)
//        emit(hopCountSignal, gpsrOption->getHopCount());
        emit(hopCountSignal, 96 - (ipv4Header->getTimeToLive()) + 1);
        EV_INFO << "Hop count for application packet = " << 32 - (ipv4Header->getTimeToLive()) + 1 << endl;
    return ACCEPT;
}

//
// lifecycle
//

void GpsrModified::handleStartOperation(LifecycleOperation *operation)
{
    configureInterfaces();
    if (enableCrossLayerRouting)
        return storeSelfPositionCongestionLevelInGlobalRegistry();
    else
        return storeSelfPositionInGlobalRegistry();
    scheduleBeaconTimer();
}

void GpsrModified::handleStopOperation(LifecycleOperation *operation)
{
    // TODO: send a beacon to remove ourself from peers neighbor position table
    // neighborPositionTable.clear();
    //////////////////////////////////////////////////////////////////////////
    // Emit Hop Count Signal (Musab)
    //////////////////////////////////////////////////////////////////////////
    if (enableCrossLayerRouting)
        neighborPositionCongestionLevelTable.clear();
    else
        neighborPositionTable.clear();
    cancelEvent(beaconTimer);
    cancelEvent(purgeNeighborsTimer);
}

void GpsrModified::handleCrashOperation(LifecycleOperation *operation)
{
    // neighborPositionTable.clear();
    //////////////////////////////////////////////////////////////////////////
    // Emit Hop Count Signal (Musab)
    //////////////////////////////////////////////////////////////////////////
    if (enableCrossLayerRouting)
        neighborPositionCongestionLevelTable.clear();
    else
        neighborPositionTable.clear();
    cancelEvent(beaconTimer);
    cancelEvent(purgeNeighborsTimer);
}

//
// notification
//

void GpsrModified::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details)
{
    Enter_Method("receiveChangeNotification");
    if (signalID == linkBrokenSignal) {
        EV_WARN << "Received link break" << endl;
        // TODO: remove the neighbor
    }
}

//////////////////////////////////////////////////////////////////////////
// Overwrite the base class (Musab)
//////////////////////////////////////////////////////////////////////////
void GpsrModified::handleMessageWhenDown(cMessage *message) {
    return;
}




