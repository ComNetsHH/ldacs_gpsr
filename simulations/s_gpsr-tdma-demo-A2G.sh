#!/bin/bash -l
#SBATCH -p smp    	 
#SBATCH --ntasks 1           
#SBATCH --cpus-per-task 1
#SBATCH --mem-per-cpu 30G  
#SBATCH --time=5-12:00:00

# Execute simulation
make gpsr-tdma-demo-A2G NUM_CPUS=1

# Exit job
exit7