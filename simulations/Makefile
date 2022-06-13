###
# This file is responsible for providing the full toolchain of creating result graphs.
# Each target is responsible for one scenario, and should
# - start the simulation(s)
# - parse the resulting .sca and .vec files
# - and plot the parsed results into .pdfs
# 
# Before each target, some brief comments should describe what the scenario is about.
# Also, variables can be created that specify parameters of the respective target. These can be shared among several targets (where e.g. each target runs some sub-scenario).
# Naming these variables should follow a SCENARIO_VARNAME syntax.
###
build-debug:	
	cd ../../; \
	echo -e "\ntracebasedapp"; \
	cd intairnet-traceBasedApp/src; opp_makemake -f -s --deep -O out -KINET4_PROJ=../../inet4 -DINET_IMPORT -I../../inet4 -I. -I../../inet4/src -L../../inet4/src -lINET_dbg; make -j8 MODE=debug; cd ../..; \
	echo -e "\nTDMA"; \
	cd tdma/tdma/src; opp_makemake --make-so -f --deep -KINET_PROJ=../../../inet4 -DINET_IMPORT -I../../../inet4/src -L../../../inet4/src -lINET_dbg; make -j8 MODE=debug; cd ../../..; \
	echo -e "\nIntAirNetGPSR"; \
	cd intairnet-gpsr/src; opp_makemake -f --deep -O out -KINET4_PROJ=../../inet4 -DINET_IMPORT -I../../inet4 -I../../intairnet-traceBasedApp/src -I../../tdma/tdma/src -I. -I../../inet4/src -L../../inet4/src -L../../intairnet-traceBasedApp/out/gcc-debug/src/ -L../../tdma/tdma/out/gcc-debug/src/ -lINET_dbg -lintairnet-traceBasedApp_dbg -ltdma_dbg; make -j$(NUM_CPUS) MODE=debug; \

build-release:	
	cd ../../; \
	echo -e "\ntracebasedapp"; \
	cd intairnet-traceBasedApp/src; opp_makemake -f -s --deep -O out -KINET4_PROJ=../../inet4 -DINET_IMPORT -I../../inet4 -I. -I../../inet4/src -L../../inet4/src -lINET; make -j8 MODE=release; cd ../..; \
	echo -e "\nTDMA"; \
	cd tdma/tdma/src; opp_makemake --make-so -f --deep -KINET_PROJ=../../../inet4 -DINET_IMPORT -I../../../inet4/src -L../../../inet4/src -lINET; make -j8 MODE=release; cd ../../..; \
	echo -e "\nIntAirNetGPSR"; \
	cd intairnet-gpsr/src; opp_makemake -f --deep -O out -KINET4_PROJ=../../inet4 -DINET_IMPORT -I../../inet4 -I../../intairnet-traceBasedApp/src -I../../tdma/tdma/src -I. -I../../inet4/src -L../../inet4/src -L../../intairnet-traceBasedApp/out/gcc-release/src/ -L../../tdma/tdma/out/gcc-release/src/ -lINET -lintairnet-traceBasedApp -ltdma; make -j$(NUM_CPUS) MODE=release


# GLOBAL VARIABLES  can be shared among all targets
# default binary in RELEASE mode
SIM_BINARY = ../out/gcc-release/src/intairnet-gpsr
SIM_BINARY_DBG = ../out/gcc-debug/src/intairnet-gpsr_dbg
# include .ned files
INCL = -n ../src:./:../../inet4/src/:../../tdma/tdma/src:../../intairnet-traceBasedApp/src

# END-to-END SIMUlATIONS

GPSR_TDMA_DEMO_3_CONFIGNAME = intairnet-gpsr-without-a2g
GPSR_TDMA_DEMO_3_A2G_CONFIGNAME = intairnet-gpsr
GPSR_TDMA_DEMO_CONFIGNAME = original-gpsr
GPSR_TDMA_DEMO_A2G_CONFIGNAME = intairnet-gpsr

# run from intairnet-gpsr/simulations
gpsr-tdma-demo-3-qtenv:
	opp_runall -j1 $(SIM_BINARY) omnetpp.ini -c $(GPSR_TDMA_DEMO_3_CONFIGNAME) $(INCL) -u Qtenv

gpsr-tdma-demo-3-debug:
	gdbgui --args $(SIM_BINARY_DBG) omnetpp.ini -u Cmdenv -c $(GPSR_TDMA_DEMO_3_CONFIGNAME) $(INCL)

gpsr-tdma-demo-3-A2G-qtenv:
	opp_runall -j1 $(SIM_BINARY) omnetpp.ini -c $(GPSR_TDMA_DEMO_3_A2G_CONFIGNAME) $(INCL) -u Qtenv

gpsr-tdma-demo-3-A2G-debug:
	gdbgui --args $(SIM_BINARY_DBG) omnetpp.ini -u Cmdenv -c $(GPSR_TDMA_DEMO_3_A2G_CONFIGNAME) $(INCL)

gpsr-tdma-demo-qtenv:
	opp_runall -j1 $(SIM_BINARY) intairnet.ini -c $(GPSR_TDMA_DEMO_CONFIGNAME) $(INCL) -u Qtenv

gpsr-tdma-demo-debug:
	gdbgui --args $(SIM_BINARY_DBG) intairnet.ini -u Cmdenv -c $(GPSR_TDMA_DEMO_CONFIGNAME) $(INCL)

gpsr-tdma-demo-A2G:
	opp_runall -j1 $(SIM_BINARY) intairnet.ini -c $(GPSR_TDMA_DEMO_A2G_CONFIGNAME) $(INCL)

gpsr-tdma-demo-A2G-qtenv:
	opp_runall -j1 $(SIM_BINARY) intairnet.ini -c $(GPSR_TDMA_DEMO_A2G_CONFIGNAME) $(INCL) -u Qtenv 

gpsr-tdma-demo-A2G-debug:
	gdbgui --args $(SIM_BINARY_DBG) intairnet.ini -u Cmdenv -c $(GPSR_TDMA_DEMO_A2G_CONFIGNAME) $(INCL)
