//-----------------------------------------------------------------------------
//
// This source file is part of the VAMPIRE open source package under the
// GNU GPL (version 2) licence (see licence file for details).
//
// (c) R F L Evans 2014. All rights reserved.
//
//-----------------------------------------------------------------------------

// C++ standard library headers
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>

// Vampire headers
#include "errors.hpp"
#include "stats.hpp"
#include "vmpi.hpp"
#include "vio.hpp"

namespace stats{

//------------------------------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------------------------------
standard_deviation_statistic_t::standard_deviation_statistic_t (){}

//------------------------------------------------------------------------------------------------------
// Function to initialize data structures
//------------------------------------------------------------------------------------------------------
void standard_deviation_statistic_t::initialize(stats::magnetization_statistic_t& mag_stat) {

   // Check that magnetization statistic is properly initialized
   if(!mag_stat.is_initialized()){
      terminaltextcolor(RED);
      std::cerr << "Programmer Error - Uninitialized magnetization statistic passed to standard_deviation statistic - please initialize first." << std::endl;
      terminaltextcolor(WHITE);
      zlog << zTs() << "Programmer Error - Uninitialized magnetization statistic passed to standard_deviation statistic - please initialize first." << std::endl;
      err::vexit();
   }

   // Determine number of magnetization statistics*4
   std::vector<double> temp = mag_stat.get_magnetization();
   num_elements = temp.size()/4;

   // Now set number of standard_deviation values to match
   residual_sq.resize(4*num_elements,0.0);
   
   // and set the mean at start to 0
   mean.resize(4*num_elements,0.0);

   // initialize mean counter
   mean_counter = 0.0;

   // Set flag indicating correct initialization
   initialized=true;

}

//------------------------------------------------------------------------------------------------------
// Function to calculate standard_deviation of the magnetisation and retain the mean value
// Welford's algorithm gives a running total. 
//
//-------------------------------------------------------------------------------------------------------
void standard_deviation_statistic_t::update(const std::vector<double>& magnetization){

   mean_counter+=1.0; // increment first, as divides by this. 
   // loop over all elements
   for(int id=0; id< num_elements; ++id){
      // copy reduced magnetisation
      for(int dir=0;dir<4;++dir){
          idx = 4*id +dir; // element id + direction, mx, my, mz, mm
          const double m = magnetization[idx];
          res1 = m - mean[idx];
          mean[idx] += res1/mean_counter;
          res2 = m - mean[idx];
          residual_sq[idx]+=res1*res2;
      }

   }

   return;

}

//------------------------------------------------------------------------------------------------------
// Function to reset statistical averages
//------------------------------------------------------------------------------------------------------
void standard_deviation_statistic_t::reset_averages(){

   // reinitialise mean magnetization and residuals to zero
   std::fill(residual_sq.begin(),residual_sq.end(),0.0);
   std::fill(mean.begin(),mean.end(),0.0);

   // reset data counter
   mean_counter = 0.0;
   return;

}

//------------------------------------------------------------------------------------------------------
// Function to output mean standard_deviation values as string
//------------------------------------------------------------------------------------------------------
std::string standard_deviation_statistic_t::output_standard_deviation(){

   // result string stream
   std::ostringstream result;

   // set custom precision if enabled
   if(vout::custom_precision){
      result.precision(vout::precision);
      if(vout::fixed) result.setf( std::ios::fixed, std::ios::floatfield );
   }

   // determine inverse mean counter and its square
   const double imean_counter = 1.0/mean_counter;

   // loop over all elements
   for(int id=0; id< num_elements -1; ++id){ // ignore last element as always contains non-magnetic atoms

      const double std_x = sqrt((residual_sq[4*id + 0]*imean_counter));
      const double std_y = sqrt(residual_sq[4*id + 1]*imean_counter);
      const double std_z = sqrt(residual_sq[4*id + 2]*imean_counter);
      const double std_m = sqrt(residual_sq[4*id + 3]*imean_counter);

      result <<std_x << "\t" << std_y << "\t" << std_z << "\t" << std_m << "\t";
      //result <<std_x << "\t" <<std_y  << "\t" << std_z << "\t" << mean_counter << "\t";


   }

   return result.str();

}

} // end of namespace stats
