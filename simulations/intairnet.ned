//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

package gpsr_intairnet.simulations;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.inet.ManetRouter;
import inet.node.inet.StandardHost;
import inet.physicallayer.contract.packetlevel.IRadioMedium;
import inet.visualizer.integrated.IntegratedMultiVisualizer;
import inet.visualizer.contract.IIntegratedVisualizer;
import data_traffic_applications.UdpTraceBasedApp;
import tdma.scheduler.TdmaScheduler;
import inet.common.scenario.ScenarioManager;
import inet.node.ethernet.Eth1G;
import inet.node.ethernet.EtherSwitch;




//module AircraftManet extends ManetRouter {
//    connections: // network node internal connections
//        at.out++ --> eth[0].upperLayerIn;
//        at.in++ <-- eth[0].upperLayerOut;
//        eth[0].phys <--> ethg[0]; // Ethernet sends frames to cable
//}

network IntAirNet
{
    parameters:
        int numAircrafts = default(0);
        int numGroundStations = default(1);
        string mobility_file = "NAT1515d3_data/NAT1515d3_mobility.txt";
        string scenarioManager_file = "NAT1515d3_data/NAT1515d3_scenarioManager.xml";

        @display("bgb=10000000,10000000;bgl=2");
        @figure[title](type=label; pos=0,-1; anchor=sw; color=darkblue);
    submodules:
        scenarioManager: ScenarioManager;
        //		responsible of assigning slots to the aircrafts based in their transmission buffer		
        scheduler: TdmaScheduler {
            parameters:
                @display("p=100,400;is=s");
        }
        
        radioMedium: <default("UnitDiskRadioMedium")> like IRadioMedium {
            @display("p=580,275");
        }

        visualizer: IntegratedMultiVisualizer {
            @display("p=100,200");
        }
        configurator: Ipv4NetworkConfigurator {
            parameters:
                @display("p=100,100;is=s");
        }

        aircraft[numAircrafts]: ManetRouter {
        }

        groundStation[numGroundStations]: ManetRouter {
            parameters:
                @display("i=device/receiverdish_vl");
        }
        
        switchGS: EtherSwitch {
            parameters:
                @display("p=4700000.285,1381949.583");
            gates: 
                ethg[400];
        }
    connections allowunconnected:
        groundStation[0].ethg++ <--> Eth1G <--> switchGS.ethg++ ;
}