//------------------------------------------------------------------------------
//
//   This file is part of the VAMPIRE open source package under the
//   Free BSD licence (see licence file for details).
//
//   (c) Andrea Meo and Richard F L Evans 2016. All rights reserved.
//
//------------------------------------------------------------------------------
//

// C++ standard library headers
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Vampire headers
#include "cells.hpp"
#include "dipole.hpp"
#include "errors.hpp"
#include "material.hpp"
#include "sim.hpp"
#include "vio.hpp"
#include "vmpi.hpp"
#include "vutil.hpp"

#include <fenv.h>
#include <signal.h>

// dipole module headers
#include "internal.hpp"

namespace dipole{

   //----------------------------------------------------------------------------
   // Function to initialize dipole module
   //----------------------------------------------------------------------------
   void initialize(const int cells_num_atoms_in_unit_cell,
                  int cells_num_cells, /// number of macrocells
                  int cells_num_local_cells, /// number of local macrocells
                  const double cells_macro_cell_size,
                  std::vector <int>& cells_local_cell_array,
                  std::vector <int>& cells_num_atoms_in_cell, /// number of atoms in each cell
                  std::vector <int>& cells_num_atoms_in_cell_global, /// number of atoms in each cell
                  std::vector < std::vector <int> >& cells_index_atoms_array,
                  const std::vector<double>& cells_volume_array,
                  std::vector<double>& cells_pos_and_mom_array, // array to store positions and moment of cells
                  std::vector < std::vector <double> >& cells_atom_in_cell_coords_array_x,
                  std::vector < std::vector <double> >& cells_atom_in_cell_coords_array_y,
                  std::vector < std::vector <double> >& cells_atom_in_cell_coords_array_z,
                  const std::vector<int>& atom_type_array,
                  const std::vector<int>& atom_cell_id_array,

                  const std::vector<double>& atom_coords_x, //atomic coordinates
                  const std::vector<double>& atom_coords_y,
                  const std::vector<double>& atom_coords_z,

                  const int num_atoms
				){

	   //-------------------------------------------------------------------------------------
      // Check for dipole calculation enabled, if not do nothing
      //-------------------------------------------------------------------------------------
      if(!dipole::activated) return;

		// check for prior initialisation
		if(dipole::internal::initialised){
      	zlog << zTs() << "Warning:  Dipole field calculation already initialised. Continuing." << std::endl;
      	return;
		}

      // output informative message
      std::cout << "Initialising dipole field calculation" << std::endl;
		zlog << zTs() << "Initialising dipole field calculation" << std::endl;

      // allocate memory for rij matrix
      dipole::internal::allocate_memory(cells_num_local_cells, cells_num_cells);

      //-------------------------------------------------------------------------------------
      // Set const for functions
      //-------------------------------------------------------------------------------------

      dipole::internal::num_atoms                  = num_atoms;
      dipole::internal::atom_type_array            = atom_type_array;
      dipole::internal::atom_cell_id_array         = atom_cell_id_array;

      dipole::internal::cells_num_cells            = cells_num_cells;
      dipole::internal::cells_num_local_cells      = cells_num_local_cells;
      dipole::internal::cells_local_cell_array     = cells_local_cell_array;
      dipole::internal::cells_num_atoms_in_cell    = cells_num_atoms_in_cell;
      dipole::internal::cells_volume_array         = cells_volume_array;

      dipole::internal::cells_pos_and_mom_array    = cells_pos_and_mom_array;

		//-------------------------------------------------------------------------------------
		// Starting calculation of dipolar field
		//-------------------------------------------------------------------------------------

      // instantiate timer
      vutil::vtimer_t timer;

      // start timer
      timer.start();

      // Check memory requirements and print to screen
      zlog << zTs() << "Fast dipole field calculation has been enabled and requires " << double(dipole::internal::cells_num_cells)*double(dipole::internal::cells_num_local_cells*6)*8.0/1.0e6 << " MB of RAM" << std::endl;
      std::cout << "Fast dipole field calculation has been enabled and requires " << double(dipole::internal::cells_num_cells)*double(dipole::internal::cells_num_local_cells*6)*8.0/1.0e6 << " MB of RAM" << std::endl;

      // For MPI version, only add local atoms
      #ifdef MPICF
       int num_local_atoms = vmpi::num_core_atoms+vmpi::num_bdry_atoms;
      #else
       int num_local_atoms = num_atoms;
      #endif


      zlog << zTs() << "Number of local cells for dipole calculation = " << dipole::internal::cells_num_local_cells << std::endl;
      zlog << zTs() << "Number of total cells for dipole calculation = " << dipole::internal::cells_num_cells << std::endl;

      // calculate matrix prefactors
      zlog << zTs() << "Precalculating rij matrix for dipole calculation... " << std::endl;
      std::cout     << "Precalculating rij matrix for dipole calculation"     << std::flush;

      //==========================================================
      //----------------------------------------------------------
      // Calculation of dipolar tensor
      //----------------------------------------------------------
      //==========================================================

      dipole::internal::initialize_tensor_solver(cells_num_atoms_in_unit_cell, cells_num_cells, cells_num_local_cells, cells_macro_cell_size, cells_local_cell_array,
                                                 cells_num_atoms_in_cell, cells_num_atoms_in_cell_global, cells_index_atoms_array, cells_volume_array, cells_pos_and_mom_array,
                                                 cells_atom_in_cell_coords_array_x, cells_atom_in_cell_coords_array_y, cells_atom_in_cell_coords_array_z,
                                                 atom_type_array, atom_cell_id_array, atom_coords_x, atom_coords_y, atom_coords_z, num_atoms);

      cells::num_cells = dipole::internal::cells_num_cells;
      cells::num_atoms_in_cell = dipole::internal::cells_num_atoms_in_cell;

      // hold parallel calculation until all processors have completed the dipole calculation
      vmpi::barrier();

      // stop timer
      timer.stop();

      std::cout << "Done! [ " << timer.elapsed_time() << " s ]" << std::endl;
      zlog << zTs() << "Precalculation of rij matrix for dipole calculation complete. Time taken: " << timer.elapsed_time() << " s"<< std::endl;

      // Set initialised flag
      dipole::internal::initialised=true;

      // start timer
      timer.start();

      // now calculate fields
      dipole::calculate_field();

      // hold parallel calculation until all processors have completed the update
      vmpi::barrier();

      // stop timer
      timer.stop();

      zlog << zTs() << "Time required for dipole update: " << timer.elapsed_time() << " s." << std::endl;

	   //-------------------------------------------------------//
	   //------- CPUs OUTPUT Dij on different fiels ------------//
	   //-------------------------------------------------------//

      // Output informative message to log file
      zlog << zTs() << "Outputting dipole matrix " << std::endl;

      // Output Demag tensor only if first step of simulation since depending only on shape
      if(sim::time == 0){

         int num_atoms_magnetic = 0.0;   // Initialise tot num of magnetic atoms
         // Calculate number of magnetic atoms
         for(int lc=0; lc<dipole::internal::cells_num_local_cells; lc++){
            int i = cells::cell_id_array[lc];
            num_atoms_magnetic += dipole::internal::cells_num_atoms_in_cell[i];
         }

         // Define and initialise Demag factor N tensor components
         double Nxx = 0.0;
         double Nxy = 0.0;
         double Nxz = 0.0;
         double Nyy = 0.0;
         double Nyz = 0.0;
         double Nzz = 0.0;

         // Every cpus print to check dipolar martrix inter term
         for(int lc=0; lc<dipole::internal::cells_num_local_cells; lc++){
            int i = cells::cell_id_array[lc];
            if(dipole::internal::cells_num_atoms_in_cell[i]>0){

               for(unsigned int j=0; j<dipole::internal::rij_inter_xx[lc].size(); j++){
                  if(dipole::internal::cells_num_atoms_in_cell[j]>0){

                     // To obtain dipolar matrix free of units, multiply tensor by "factor"
                     const double Vatomic = dipole::internal::cells_volume_array[j]/double(dipole::internal::cells_num_atoms_in_cell[j]);
                     const double factor = Vatomic*double(dipole::internal::cells_num_atoms_in_cell[j]) * double(dipole::internal::cells_num_atoms_in_cell[i]);
                     // Sum over dipolar tensor to obtain total tensor
                     Nxx += factor*(dipole::internal::rij_intra_xx[lc][j]+dipole::internal::rij_inter_xx[lc][j]);
                     Nxy += factor*(dipole::internal::rij_intra_xy[lc][j]+dipole::internal::rij_inter_xy[lc][j]);
                     Nxz += factor*(dipole::internal::rij_intra_xz[lc][j]+dipole::internal::rij_inter_xz[lc][j]);
                     Nyy += factor*(dipole::internal::rij_intra_yy[lc][j]+dipole::internal::rij_inter_yy[lc][j]);
                     Nyz += factor*(dipole::internal::rij_intra_yz[lc][j]+dipole::internal::rij_inter_yz[lc][j]);
                     Nzz += factor*(dipole::internal::rij_intra_zz[lc][j]+dipole::internal::rij_inter_zz[lc][j]);
                  }
               }
            }
         }

         // Reduce values of num of magnetic atoms and demag factors on all procs
         #ifdef MPICF
            MPI_Allreduce(MPI_IN_PLACE, &num_atoms_magnetic, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
            MPI_Allreduce(MPI_IN_PLACE, &Nxx, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
            MPI_Allreduce(MPI_IN_PLACE, &Nxy, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
            MPI_Allreduce(MPI_IN_PLACE, &Nxz, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
            MPI_Allreduce(MPI_IN_PLACE, &Nyy, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
            MPI_Allreduce(MPI_IN_PLACE, &Nyz, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
            MPI_Allreduce(MPI_IN_PLACE, &Nzz, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
         #endif

         // Compute demag factor tensor from dipolar matrix adding self term
         Nxx = ((Nxx /num_atoms_magnetic)-4.0*M_PI/3.0)/(-4.0*M_PI);
         Nxy =  (Nxy /num_atoms_magnetic              )/(-4.0*M_PI);
         Nxz =  (Nxz /num_atoms_magnetic              )/(-4.0*M_PI);
         Nyy = ((Nyy /num_atoms_magnetic)-4.0*M_PI/3.0)/(-4.0*M_PI);
         Nyz =  (Nyz /num_atoms_magnetic              )/(-4.0*M_PI);
         Nzz = ((Nzz /num_atoms_magnetic)-4.0*M_PI/3.0)/(-4.0*M_PI);

         // Write demag factor to log file zlog << zTs() <<
         zlog << zTs() << "Demagnetisation tensor in format Nxx\t\tNxy\t\tNxz\t\tNyx\t\tNyy\tNyz\t\tNzx\t\tNzy\t\tNzz :\n";
         zlog << zTs() << Nxx << "\t" << Nxy << "\t" << Nxz << "\t";
         zlog <<          Nxy << "\t" << Nyy << "\t" << Nyz << "\t";
         zlog <<          Nxz << "\t" << Nyz << "\t" << Nzz << "\n";

      } // close if loop for sim::time == 0
	   //--------------------------------------------------/
      // End of outptu dipolar tensor
	   //--------------------------------------------------/

    	return;
    }
} // end of dipole namespace
