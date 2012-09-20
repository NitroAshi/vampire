//====================================================================================
//
//														mpi_generic
//
//									Functions for basic mpi functionality
//
//										Version 1.0 R Evans 07/08/2009
//
//====================================================================================

#include "errors.hpp"
#include "vmpi.hpp"
#include <iostream>
#include <fstream>

#ifdef MPICF
namespace vmpi{

int initialise(){
	//====================================================================================
	//
	//												initialise_mpi
	//
	//									Startup MPI and Output host information
	//
	//										Version 1.0 R Evans 16/07/2009
	//
	//====================================================================================
	//
	//		Locally allocated variables: 	
	//
	//====================================================================================
	// C++ MPI Binding Reference:
	// https://computing.llnl.gov/tutorials/mpi/
	//====================================================================================

	//----------------------------------------------------------
	// check calling of routine if error checking is activated
	//----------------------------------------------------------
	if(err::check==true){std::cout << "initialise_mpi has been called" << std::endl;}

	int resultlen;

	// Initialise MPI
	MPI::Init();

	// Get number of processors and rank
	vmpi::my_rank = MPI::COMM_WORLD.Get_rank();
	vmpi::num_processors = MPI::COMM_WORLD.Get_size();
	MPI::Get_processor_name(vmpi::hostname, resultlen);

	// Start MPI Timer
	vmpi::start_time=MPI_Wtime();

	return EXIT_SUCCESS;	
}

int hosts(){
	//====================================================================================
	//
	//													mpi_hosts
	//
	//										Print MPI hostnames to screen
	//
	//										Version 1.0 R Evans 07/08/2009
	//
	//====================================================================================

	// check calling of routine if error checking is activated
	if(err::check==true){std::cout << "mpi_hosts has been called" << std::endl;}

	// Wait for all processors
	//MPI::COMM_WORLD.Barrier();

   //for(int i=0;i<vmpi::num_processors;i++){
   //	if(vmpi::my_rank==i){
	if(vmpi::num_processors<=512){
			// Output rank, num_procs, hostname to screen
			std::cout << "Processor " << vmpi::my_rank+1 << " of " << vmpi::num_processors;
			std::cout << " online on host " << vmpi::hostname << std::endl;
			//	}
			//MPI::COMM_WORLD.Barrier();
	   //}
	}
	// Wait for all processors
			//MPI::COMM_WORLD.Barrier();

	return EXIT_SUCCESS;	
}

int finalise(){
	//====================================================================================
	//
	//												finalise_mpi
	//
	//									Finalise MPI and output wall time
	//
	//										Version 1.0 R Evans 07/08/2009
	//
	//====================================================================================

	// check calling of routine if error checking is activated
	if(err::check==true){std::cout << "finalise_mpi has been called" << std::endl;}

	// Wait for all processors
   MPI::COMM_WORLD.Barrier();

	
	// Output MPI Timings to disk
	// Get sizes of arrays
	//std::vector<int> sizes(vmpi::num_processors);
	//int MPITimingDataSize = ComputeTimeArray.size();
	
	// MPI_Gather (&sendbuf,sendcnt,sendtype,&recvbuf, recvcount,recvtype,root,comm)
	//MPI_Gather(&MPITimingDataSize,1,MPI_INT,&sizes[0],1,MPI_INT,0,MPI_COMM_WORLD); 
	//for(int p=0; p<vmpi::num_processors;p++){
	//	std::cout << "node01:" << p << " " << sizes.at(p) << std::endl;
	//}
	
	// Gather timings
	if(DetailedMPITiming){
		std::vector<double> AllTimes(0);
		if(my_rank==0) AllTimes.resize(num_processors*WaitTimeArray.size());
		
		MPI_Gather(&WaitTimeArray[0],WaitTimeArray.size(),MPI_DOUBLE,&AllTimes[0],WaitTimeArray.size(),MPI_DOUBLE,0,MPI_COMM_WORLD); 

		if(my_rank==0){
			std::ofstream WaitTimesOFS;
			WaitTimesOFS.open("MPI-wait-times");
		
			// Column Row format
			for(int idx=0;idx<WaitTimeArray.size();idx++){
				WaitTimesOFS << idx << "\t";
				for(int p=0;p<vmpi::num_processors;p++){
					WaitTimesOFS << AllTimes.at(idx+p*WaitTimeArray.size()) << "\t";
				}
				WaitTimesOFS << std::endl;
			}
			WaitTimesOFS.close();
		}
		
		MPI_Gather(&ComputeTimeArray[0],ComputeTimeArray.size(),MPI_DOUBLE,&AllTimes[0],ComputeTimeArray.size(),MPI_DOUBLE,0,MPI_COMM_WORLD); 

		if(my_rank==0){
			std::ofstream ComputeTimesOFS;
			ComputeTimesOFS.open("MPI-compute-times");
		
			// Column Row format
			for(int idx=0;idx<ComputeTimeArray.size();idx++){
				ComputeTimesOFS << idx << "\t";
				for(int p=0;p<vmpi::num_processors;p++){
					ComputeTimesOFS << AllTimes.at(idx+p*ComputeTimeArray.size()) << "\t";
				}
				ComputeTimesOFS << std::endl;
			}
			ComputeTimesOFS.close();
		}
	}
	
	// Stop MPI Timer and output to screen
	vmpi::end_time=MPI_Wtime();
	if(vmpi::my_rank==0){
		std::cout << "MPI Simulation Time: " << vmpi::end_time-vmpi::start_time << std::endl;
	}

	// Finalise MPI
	MPI::Finalize();

	return EXIT_SUCCESS;	
}
//-------------------------------------------------------------
// Function to swap timer and return time between calls
//
// Objective is to time spent waiting and computing. Since 
// wait time is encapslated by MPIWait and MPIBarrier 
// everything else is defined as compute time.
//
// Example:
// 
// Start with compute_time=time and do some calculations. 
// When we get to first MPIWait call we now stop the compute
// timer and start the wait timer, so call the swap function.
//
// total_compute_time += SwapTimer(compute_time, wait_time);
//
// This sets the wait time to time and returns the compute time. 
// Now call to wait returns, so swap timers back again:
// 
// total_wait_time += SwapTimer(wait_time, compute_time);
// 
// This returns total time since wait called and resets 
// compute time until next call.
//
// (c) Richard F. L. Evans 2012
//
double SwapTimer(double OldTimer, double& NewTimer){

	// get current time
	double time = MPI_Wtime();
	
	// set start time for NewTimer
	NewTimer=time;
	
	// Calculate time elapsed since last call for OldTimer
	return time-OldTimer;
	
}

} // end of namespace vmpi
#endif 

