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

//////////////////////////////////////////////////////////////////////////
// Cross-layer routing (Musab)
//////////////////////////////////////////////////////////////////////////
#include "PositionTableCongestionLevelModified.h"
#include <tuple>   //include tuple in order to map the address also to the corresponding congestionLevel


namespace inet {

std::vector<L3Address> PositionTableCongestionLevelModified::getAddresses() const
{
    std::vector<L3Address> addresses;
    for (const auto & elem : addressToPositionCongestionLevelMap)
        addresses.push_back(elem.first);
    return addresses;
}

bool PositionTableCongestionLevelModified::hasPosition(const L3Address& address) const
{
    AddressToPositionCongestionLevelMap::const_iterator it = addressToPositionCongestionLevelMap.find(address);
    return it != addressToPositionCongestionLevelMap.end();
}

Coord PositionTableCongestionLevelModified::getPosition(const L3Address& address) const
{
    AddressToPositionCongestionLevelMap::const_iterator it = addressToPositionCongestionLevelMap.find(address);
    if (it == addressToPositionCongestionLevelMap.end())
        return Coord(NaN, NaN, NaN);
    else
        return std::get<1>(it->second);   
}

int PositionTableCongestionLevelModified::getCongestionLevel(const L3Address& address) const
{
    AddressToPositionCongestionLevelMap::const_iterator it = addressToPositionCongestionLevelMap.find(address);
    if (it == addressToPositionCongestionLevelMap.end())
        return 0;
    else
        return std::get<2>(it->second);     //Rudan
}

void PositionTableCongestionLevelModified::setPositionCongestionLevel(const L3Address& address, const Coord& coord, const int& congestionLevel)
{
    ASSERT(!address.isUnspecified());
    addressToPositionCongestionLevelMap[address] = AddressToPositionCongestionLevelMapValue(simTime(), coord, congestionLevel);
}

void PositionTableCongestionLevelModified::removePosition(const L3Address& address)
{
    auto it = addressToPositionCongestionLevelMap.find(address);
    addressToPositionCongestionLevelMap.erase(it);
}

void PositionTableCongestionLevelModified::removeOldPositions(simtime_t timestamp)
{
    for (auto it = addressToPositionCongestionLevelMap.begin(); it != addressToPositionCongestionLevelMap.end(); )
        if (std::get<0>(it->second) <= timestamp)
            addressToPositionCongestionLevelMap.erase(it++);
        else
            it++;

}

void PositionTableCongestionLevelModified::clear()
{
    addressToPositionCongestionLevelMap.clear();
}

simtime_t PositionTableCongestionLevelModified::getOldestPosition() const
{
    simtime_t oldestPosition = SimTime::getMaxTime();
    for (const auto & elem : addressToPositionCongestionLevelMap) {
        const simtime_t& time = std::get<0>(elem.second);
        if (time < oldestPosition)
            oldestPosition = time;
    }
    return oldestPosition;
}

std::ostream& operator << (std::ostream& o, const PositionTableCongestionLevelModified& t)
{
    o << "{ ";
    for(auto elem : t.addressToPositionCongestionLevelMap) {
        o << elem.first << ":(" << std::get<0>(elem.second) << ";" << std::get<1>(elem.second) << ";" << std::get<2>(elem.second) << ") ";
    }
    o << "}";
    return o;
}

} // namespace inet

