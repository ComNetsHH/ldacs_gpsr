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

import inet.common.INETDefs;
import inet.common.TlvOptions;
import inet.common.geometry.Geometry;
import inet.common.packet.chunk.Chunk;
import inet.networklayer.common.L3Address;

cplusplus {{
#include "GpsrModifiedDefs.h"
}}

namespace inet;

enum GpsrForwardingMode {
    GPSR_GREEDY_ROUTING = 1;
    GPSR_PERIMETER_ROUTING = 2;
};

enum GpsrPlanarizationMode {
    GPSR_NO_PLANARIZATION = 0;
    GPSR_GG_PLANARIZATION = 1;
    GPSR_RNG_PLANARIZATION = 2;
};

//
// The GPSR beacon packet is sent periodically by all GPSR routers to notify
// the neighbors about the router's address and position.
//
class GpsrBeaconModified extends FieldsChunk
{
    L3Address address;
    Coord position;
}

//////////////////////////////////////////////////////////////////////////
// Cross-layer routing (Musab)
//////////////////////////////////////////////////////////////////////////
class GpsrBeaconCongestionLevelModified extends FieldsChunk
{
    L3Address address;
    Coord position;
    int congestionLevel;
}

//
// The GPSROption is used to add extra routing information for network datagrams.
//
// For more information, see the GPSR paper
// http://www.eecs.harvard.edu/~htk/publication/2000-mobi-karp-kung.pdf
//
class GpsrOption extends TlvOptionBase {
    GpsrForwardingMode routingMode;
    Coord destinationPosition;               // D
    // unsigned int DestinationIndex;
    Coord perimeterRoutingStartPosition;     // Lp
    Coord perimeterRoutingForwardPosition;   // Lf
    L3Address currentFaceFirstSenderAddress;   // e0
    L3Address currentFaceFirstReceiverAddress; // e0
    L3Address senderAddress; // TODO: this field is not strictly needed by GPSR (should be eliminated)
//    //////////////////////////////////////////////////////////////////////////
//    // Record Hop Count Signal (Musab)
//    //////////////////////////////////////////////////////////////////////////
//    unsigned int hopCount;    // The number of hops traversed by each application message.
}

