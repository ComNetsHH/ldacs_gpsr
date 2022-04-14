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
import tdma.scheduler.TdmaScheduler;
import inet.node.ethernet.Eth1G;
import inet.node.ethernet.EtherSwitch;


//module AircraftManet extends ManetRouter {
//    connections: // network node internal connections
//        at.out++ --> eth[0].upperLayerIn;
//        at.in++ <-- eth[0].upperLayerOut;
//        eth[0].phys <--> ethg[0]; // Ethernet sends frames to cable
//}

network Modifiedgpsr
{
    parameters:
        @display("bgb=650,500;bgl=2");
        @figure[title](type=label; pos=0,-1; anchor=sw; color=darkblue);
    submodules:
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

        //		responsible of assigning slots to the aircrafts based in their transmission buffer		
        scheduler: TdmaScheduler {
            parameters:
                @display("p=100,400;is=s");
        }

        hostA: ManetRouter {
            parameters:
                @display("p=50,325");
        }

        hostB: ManetRouter {
            parameters:
                @display("p=250,325");
        }

        groundStation: ManetRouter {
            parameters:
                @display("p=250,125");
        }
}

network ModifiedgpsrA2G extends Modifiedgpsr
{
    submodules:
        switch1: EtherSwitch {
            parameters:
                @display("p=250,200");
            gates:
                ethg[];
        }
    connections:
        switch1.ethg++  <--> Eth1G <--> hostA.ethg++;
        switch1.ethg++  <--> Eth1G <--> hostB.ethg++;
        groundStation.ethg++ <--> Eth1G <--> switch1.ethg++ ;
}
