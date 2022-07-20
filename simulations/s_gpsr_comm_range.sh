#!/bin/bash -l
#SBATCH -p smp    	 
#SBATCH --ntasks 1           
#SBATCH --cpus-per-task 1
#SBATCH --mem-per-cpu 5G  
#SBATCH --array=0-129
#SBATCH --time=5-12:00:00

# Execute simulation
opp_runall -j1 ../out/gcc-release/src/intairnet-gpsr omnetpp.ini -c intairnet-gpsr -n ../src:./:../../inet4/src/:../../tdma/tdma/src:../../intairnet-tracebasedapp/src -r ${SLURM_ARRAY_TASK_ID}

# Exit job
exit7