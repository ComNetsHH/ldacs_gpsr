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

package gpsr_intairnet;

import inet.routing.contract.IManetRouting;

//
// This module provides Greedy Perimeter Stateless Routing for Wireless Networks.
// The implementation supports both GG and RNG planarization algorithms.
//
// For more information on the routing algorithm, see the GPSR paper
// http://www.eecs.harvard.edu/~~htk/publication/2000-mobi-karp-kung.pdf
//
simple GpsrModified like IManetRouting
{
    parameters:
        // properties
        @display("i=block/routing");

        // Multi-Link parameters
        bool useMultiLink = default(false);
        double pSatcom = default(0);

        // context parameters
        string interfaceTableModule;   // The path to the InterfaceTable module
        string routingTableModule = default("^.ipv4.routingTable");
        string networkProtocolModule = default("^.ipv4.ip");
        string outputInterface = default("wlan0");
        //////////////////////////////////////////////////////////////////////////
        // Set interface to the interface used in A2G if next hop is GS (Musab)
        //////////////////////////////////////////////////////////////////////////
        string a2gOutputInterface = default("eth0");
        //////////////////////////////////////////////////////////////////////////
        // Enable/Disable creation of beacons (Musab)
        //////////////////////////////////////////////////////////////////////////
        bool beaconForwardedFromGpsr = default(false);

        // GPSR parameters
        string planarizationMode @enum("", "GG", "RNG") = default("GG");
        string interfaces = default("*");
        double beaconInterval @unit(s) = default(10s);
        double maxJitter @unit(s) = default(0.5 * beaconInterval);
        double neighborValidityInterval @unit(s) = default(4.5 * beaconInterval);
        int positionByteLength @unit(B) = default(2 * 4B);
        //////////////////////////////////////////////////////////////////////////
        // The ground station communication range (Musab)
        //////////////////////////////////////////////////////////////////////////
        double groundStationRange @unit(m) = default(370400m);   // the range where A2G cimmunication is possible
		//////////////////////////////////////////////////////////////////////////
        // Set the destination position (The ground station) in the GPSR packet (Musab)
        //////////////////////////////////////////////////////////////////////////
		double GSx @unit(m) = default(4734222.285m);
        double GSy @unit(m) = default(1381949.583m);
        double GSz @unit(m) = default(662813.2938m);
        //////////////////////////////////////////////////////////////////////////
        // Cross-layer routing (Musab)
        //////////////////////////////////////////////////////////////////////////
        double weightingFactor = default(0.5); 
        int congestionLevel = default(1); 
        int congestionLevelByteLength @unit(B) = default(1 * 1B); 
        bool enableCrossLayerRouting = default(false);
        // visualization parameters
        bool displayBubbles = default(false);	// display bubble messages about changes in routing state for packets
//        //////////////////////////////////////////////////////////////////////////
//        // Emit Hop Count Signal (Musab)
//        //////////////////////////////////////////////////////////////////////////
        @signal[hopCount](type=unsigned int);
        @statistic[hopCount](source=hopCount; record=vector);
        //////////////////////////////////////////////////////////////////////////
        // Route not found (Musab)
        //////////////////////////////////////////////////////////////////////////
        @signal[routingFailed](type=simtime_t);
        @statistic[routingFailed](source=routingFailed; record=vector,histogram,count);
        //////////////////////////////////////////////////////////////////////////
        // Switch to Perimeter Routing (Musab)
        //////////////////////////////////////////////////////////////////////////
        @signal[greedyForwardingFailed](type=simtime_t);
        @statistic[greedyForwardingFailed](source=greedyForwardingFailed; record=vector,histogram,count);
    gates:
        input ipIn;
        output ipOut;
}

