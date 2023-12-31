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

#ifndef __INET_GPSRMODIFIED_H
#define __INET_GPSRMODIFIED_H

#include "inet/common/INETDefs.h"
#include "inet/common/geometry/common/Coord.h"
#include "inet/common/packet/Packet.h"
#include "inet/mobility/contract/IMobility.h"
#include "inet/networklayer/contract/IL3AddressType.h"
#include "inet/networklayer/contract/INetfilter.h"
#include "inet/networklayer/contract/IRoutingTable.h"
#include "inet/routing/base/RoutingProtocolBase.h"
#include "GpsrModified_m.h"
#include "MultiLinkPacketTag_m.h"
#include "inet/transportlayer/udp/UdpHeader_m.h"
#include "PositionTableModified.h"
//////////////////////////////////////////////////////////////////////////
// Cross-layer routing (Musab)
//////////////////////////////////////////////////////////////////////////
#include "PositionTableCongestionLevelModified.h"

using namespace inet;

/**
 * This class implements the Greedy Perimeter Stateless Routing for Wireless Networks.
 * The implementation supports both GG and RNG planarization algorithms.
 *
 * For more information on the routing algorithm, see the GPSR paper
 * http://www.eecs.harvard.edu/~htk/publication/2000-mobi-karp-kung.pdf
 */
// TODO: optimize internal data structures for performance to use less lookups and be more prepared for routing a packet
// TODO: implement position piggybacking that is all packets should carry the position of the sender, all packets act as a beacon and reset beacon timer
// TODO: implement promiscuous mode, all receivers should process all packets with respect to neighbor positions
// KLUDGE: implement position registry protocol instead of using a global variable
class GpsrModified : public RoutingProtocolBase, public cListener, public NetfilterBase::HookBase
{
  private:
    // Multi-Link Parameters
    bool useMultiLink = false;
    bool useIntelligentMultiLinkMode = false;
    m multiLinkCutoffDistance;
    double pSatcom = 0.0;

    // GPSR parameters
    GpsrPlanarizationMode planarizationMode = static_cast<GpsrPlanarizationMode>(-1);
    const char *interfaces = nullptr;
    simtime_t beaconInterval;
    simtime_t maxJitter;
    simtime_t neighborValidityInterval;
    bool displayBubbles;
    //////////////////////////////////////////////////////////////////////////
    // Set the destination position (The ground station) in the GPSR packet (Musab)
    //////////////////////////////////////////////////////////////////////////
    double GSx = 0.0;
    double GSy = 0.0;
    double GSz = 0.0;
    //Name of the Trace file where list of ground stations are given
    const char* groundstationsTraceFile ;
    virtual void parseGroundstationTraceFile2Vector(const char* file_name);
    virtual int findClosestGroundStation();
    //Declaring Vector to store the ground station coordinates and ethernet
    std::vector<std::vector<double>> ground_stations_coordinates_array;
    std::vector<std::string> ethernet_vector;

    //////////////////////////////////////////////////////////////////////////
    // Cross-layer routing (Musab)
    //////////////////////////////////////////////////////////////////////////
    double weightingFactor; 
    int congestionLevel; 
    int congestionLevelByteLength = -1;  
    //////////////////////////////////////////////////////////////////////////
    // Enable/Disable creation of beacons (Musab)
    //////////////////////////////////////////////////////////////////////////
    bool beaconForwardedFromGpsr;
    // context
    cModule *host = nullptr;
    IMobility *mobility = nullptr;
    IL3AddressType *addressType = nullptr;
    IInterfaceTable *interfaceTable = nullptr;
    const char *outputInterface = nullptr;
    //////////////////////////////////////////////////////////////////////////
    // Set interface to the interface used in A2G if next hop is GS (Musab)
    //////////////////////////////////////////////////////////////////////////
    const char *a2gOutputInterface = nullptr;
    IRoutingTable *routingTable = nullptr;    // TODO: delete when necessary functions are moved to interface table
    INetfilter *networkProtocol = nullptr;
    static PositionTableModified globalPositionTable;    // KLUDGE: implement position registry protocol
    //////////////////////////////////////////////////////////////////////////
    // Cross-layer routing (Musab)
    //////////////////////////////////////////////////////////////////////////
    static PositionTableCongestionLevelModified globalPositionCongestionLevelTable;    // KLUDGE: implement position registry protocol

    // packet size
    int positionByteLength = -1;

    // internal
    cMessage *beaconTimer = nullptr;
    cMessage *purgeNeighborsTimer = nullptr;
    PositionTableModified neighborPositionTable;
    //////////////////////////////////////////////////////////////////////////
    // Cross-layer routing (Musab)
    //////////////////////////////////////////////////////////////////////////
    PositionTableCongestionLevelModified neighborPositionCongestionLevelTable;

  public:
    GpsrModified();
    virtual ~GpsrModified();
    //////////////////////////////////////////////////////////////////////////
    // Process MCSOTDMA beacons (Musab)
    //////////////////////////////////////////////////////////////////////////
    // process beacons that are received from MCSOTDMA and pass information to GpsrModified
    void processBeaconMCSOTDMA(const L3Address& address, const Coord& coord);
    void processBeaconCongestionLevelMCSOTDMA(const L3Address& address, const Coord& coord, const int& congestionLevel);
    //////////////////////////////////////////////////////////////////////////
    // Cross-layer routing (Musab)
    //////////////////////////////////////////////////////////////////////////
    bool enableCrossLayerRouting;
  protected:
    // module interface
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    void initialize(int stage) override;
    //////////////////////////////////////////////////////////////////////////
    // Overwrite the base class (Musab)
    //////////////////////////////////////////////////////////////////////////
    virtual void handleMessageWhenDown(cMessage *message) override;
    void handleMessageWhenUp(cMessage *message) override;

  private:
    // handling messages
    void processSelfMessage(cMessage *message);
    void processMessage(cMessage *message);

    // handling beacon timers
    void scheduleBeaconTimer();
    void processBeaconTimer();

    // handling purge neighbors timers
    void schedulePurgeNeighborsTimer();
    void processPurgeNeighborsTimer();

    // handling UDP packets
    void sendUdpPacket(Packet *packet);
    void processUdpPacket(Packet *packet);

    // handling beacons
    const Ptr<GpsrBeaconModified> createBeacon();
    //////////////////////////////////////////////////////////////////////////
    // Cross-layer routing (Musab)
    //////////////////////////////////////////////////////////////////////////
    const Ptr<GpsrBeaconCongestionLevelModified> createBeaconCongestionLevel();
    void sendBeacon(const Ptr<GpsrBeaconModified>& beacon);
    //////////////////////////////////////////////////////////////////////////
    // Cross-layer routing (Musab)
    //////////////////////////////////////////////////////////////////////////
    void sendBeaconCongestionLevel(const Ptr<GpsrBeaconCongestionLevelModified>& beacon);
    void processBeacon(Packet *packet);
    //////////////////////////////////////////////////////////////////////////
    // Cross-layer routing (Musab)
    //////////////////////////////////////////////////////////////////////////
    void processBeaconCongestionLevel(Packet *packet);
    
    // handling packets
    GpsrOption *createGpsrOption(L3Address destination);
    int computeOptionLength(GpsrOption *gpsrOption);
    void setGpsrOptionOnNetworkDatagram(Packet *packet, const Ptr<const NetworkHeaderBase>& networkHeader, GpsrOption *gpsrOption);

    // returns nullptr if not found
    GpsrOption *findGpsrOptionInNetworkDatagramForUpdate(const Ptr<NetworkHeaderBase>& networkHeader);
    const GpsrOption *findGpsrOptionInNetworkDatagram(const Ptr<const NetworkHeaderBase>& networkHeader) const;

    // throws an error when not found
    GpsrOption *getGpsrOptionFromNetworkDatagramForUpdate(const Ptr<NetworkHeaderBase>& networkHeader);
    const GpsrOption *getGpsrOptionFromNetworkDatagram(const Ptr<const NetworkHeaderBase>& networkHeader) const;

    // configuration
    void configureInterfaces();

    // position
    Coord lookupPositionInGlobalRegistry(const L3Address& address) const;
    void storePositionInGlobalRegistry(const L3Address& address, const Coord& position) const;
    //////////////////////////////////////////////////////////////////////////
    // Cross-layer routing (Musab)
    //////////////////////////////////////////////////////////////////////////
    void storePositionCongestionLevelInGlobalRegistry(const L3Address& address, const Coord& position,const int& congestionLevel) const; 
    void storeSelfPositionInGlobalRegistry() const;
    //////////////////////////////////////////////////////////////////////////
    // Cross-layer routing (Musab)
    //////////////////////////////////////////////////////////////////////////
    void storeSelfPositionCongestionLevelInGlobalRegistry() const;
    Coord computeIntersectionInsideLineSegments(Coord& begin1, Coord& end1, Coord& begin2, Coord& end2) const;
    Coord getNeighborPosition(const L3Address& address) const;
    //////////////////////////////////////////////////////////////////////////
    // Cross-layer routing (Musab)
    //////////////////////////////////////////////////////////////////////////
    Coord getNeighborPositionCongestionLevel(const L3Address& address) const;
    //////////////////////////////////////////////////////////////////////////
    // The ground station communication range (Musab)
    //////////////////////////////////////////////////////////////////////////
    m groundStationRange;

    // angle
    double getVectorAngle(Coord vector) const;
    double getNeighborAngle(const L3Address& address) const;

    // address
    std::string getHostName() const;
    L3Address getSelfAddress() const;
    L3Address getSenderNeighborAddress(const Ptr<const NetworkHeaderBase>& networkHeader) const;

    // neighbor
    simtime_t getNextNeighborExpiration();
    void purgeNeighbors();
    std::vector<L3Address> getPlanarNeighbors() const;
    std::vector<L3Address> getPlanarNeighborsCounterClockwise(double startAngle) const;

    // next hop
    L3Address findNextHop(const L3Address& destination, GpsrOption *gpsrOption);
    L3Address findGreedyRoutingNextHop(const L3Address& destination, GpsrOption *gpsrOption);
    L3Address findPerimeterRoutingNextHop(const L3Address& destination, GpsrOption *gpsrOption);

    // routing
    Result routeDatagram(Packet *datagram, GpsrOption *gpsrOption);

    // netfilter
    virtual Result datagramPreRoutingHook(Packet *datagram) override;
    virtual Result datagramForwardHook(Packet *datagram) override { return ACCEPT; }
    virtual Result datagramPostRoutingHook(Packet *datagram) override { return ACCEPT; }
    //////////////////////////////////////////////////////////////////////////
    // Emit Hop Count Signal (Musab)
    //////////////////////////////////////////////////////////////////////////
    virtual Result datagramLocalInHook(Packet *datagram) override;
//    virtual Result datagramLocalInHook(Packet *datagram) override { return ACCEPT; }
    virtual Result datagramLocalOutHook(Packet *datagram) override;

    // lifecycle
    virtual void handleStartOperation(LifecycleOperation *operation) override;
    virtual void handleStopOperation(LifecycleOperation *operation) override;
    virtual void handleCrashOperation(LifecycleOperation *operation) override;

    // notification
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details) override;

    //////////////////////////////////////////////////////////////////////////
    // Emit Hop Count Signal (Musab)
    //////////////////////////////////////////////////////////////////////////
    simsignal_t hopCountSignal;
    //////////////////////////////////////////////////////////////////////////
    // Route not found (Musab)
    //////////////////////////////////////////////////////////////////////////
    simsignal_t routingFailedSignal;
    //////////////////////////////////////////////////////////////////////////
    // Switch to Perimeter Routing (Musab)
    //////////////////////////////////////////////////////////////////////////
    simsignal_t greedyForwardingFailedSignal;

};


#endif // ifndef __INET_GPSRMODIFIED_H

