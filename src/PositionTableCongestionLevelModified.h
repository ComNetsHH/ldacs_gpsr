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

#ifndef __INET_POSITIONTABLECONGESTIONLEVELMODIFIED_H
#define __INET_POSITIONTABLECONGESTIONLEVELMODIFIED_H

#include <map>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/common/geometry/common/Coord.h"
#include "inet/networklayer/common/L3Address.h"

namespace inet {

/**
 * This class provides a mapping between node addresses and their positions.
 */
class PositionTableCongestionLevelModified
{
  private:
    //////////////////////////////////////////////////////////////////////////
    // Cross-layer routing (Musab)
    //////////////////////////////////////////////////////////////////////////
    typedef std::tuple<simtime_t, Coord, int> AddressToPositionCongestionLevelMapValue; 
    typedef std::map<L3Address, AddressToPositionCongestionLevelMapValue> AddressToPositionCongestionLevelMap;
    AddressToPositionCongestionLevelMap addressToPositionCongestionLevelMap;

  public:
    PositionTableCongestionLevelModified() {}

    std::vector<L3Address> getAddresses() const;

    bool hasPosition(const L3Address& address) const;
    Coord getPosition(const L3Address& address) const;
    //////////////////////////////////////////////////////////////////////////
    // Cross-layer routing (Musab)
    //////////////////////////////////////////////////////////////////////////
    int getCongestionLevel(const L3Address& address) const;
    void setPositionCongestionLevel(const L3Address& address, const Coord& coord, const int& congestionLevel);

    void removePosition(const L3Address& address);
    void removeOldPositions(simtime_t timestamp);

    void clear();

    simtime_t getOldestPosition() const;

    friend std::ostream& operator << (std::ostream& o, const PositionTableCongestionLevelModified& t);
};

} // namespace inet

#endif // ifndef __INET_POSITIONTABLECONGESTIONLEVELMODIFIED_H

