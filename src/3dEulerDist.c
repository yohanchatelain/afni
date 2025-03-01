/* 
   Working from P. Lauren's distanceField.c, which was made in
   connection with PA Taylor's Python library for this.

ver = 2.0;  date = Nov 29, 2021
+ [PT] this program has been a longtime coming.  This version merges
  P Taylor's Python version in lib_EDT.py with P Lauren's concurrent
  work on a C version (which had been compared/developed in part with 
  the aformentioned lib_EDT.py).

ver = 2.1;  date = Dec 1, 2021
+ [PT] Rearrange how main run_EDT_3D works, and how calc_EDT_3D() is
  called. Mostly, we want to make it easier for other programs to use
  the latter func

ver = 2.2;  date = Dec 1, 2021
+ [PT] Bug fix: had delta set incorrectly in 2 out of 3
  calc_EDT_3d_dim?() funcs, because was using *DZ* everywhere.

ver = 2.3;  date = Dec 8, 2021
+ [PT] new opt, '-only2D ..' so that EDT can be calced in only 2D, if
  desired.  For DRG, may he use it well.

ver = 2.4;  date = Dec 9, 2021
+ [PT] internal tweak: allocate some tmp arrays less, doesn't really
  make much difference in speed :(

ver = 2.5;  date = Dec 9, 2021
+ [PT] fix 2D selection---was only correct for some dset orientations

*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <float.h>
#include "debugtrace.h"
#include "mrilib.h"
#include "3ddata.h"
#include "thd_euler_dist.c"

int run_EDT_3D( int comline, PARAMS_euler_dist opts,
                int argc, char *argv[] );

// --------------------------------------------------------------------------

int usage_3dEulerDist() 
{
   char *author = "PA Taylor and P Lauren (SSCC, NIMH, NIH)";

   printf(
"\n"
"Overview ~1~ \n"
"\n"
"This program calculates the Eulerian Distance Transform (EDT).\n"
"\n"
"Basically, this means calculating the Euclidean distance of each\n"
"voxel's centroid to the nearest boundary  with a separate ROI (well, to be\n"
"brutally technical, to centroid of the nearest voxel in a neighboring ROI.\n"
"The input dataset should be a map of ROIs (so, integer-valued). The\n"
"EDT values are calculated throughout the entire FOV by default,\n"
"even in the zero/background regions (there is an option to control this).\n"
"\n"
"written by: %s\n"
"\n"
"Description ~2~ \n"
"\n"
"This code calculates the Euclidean Distance Transform (EDT) for 3D\n"
"volumes following this nice, efficient algorithm, by Felzenszwalb\n"
"and Huttenlocher (2012;  FH2012):\n"
"\n"
"   Felzenszwalb PF, Huttenlocher DP (2012). Distance Transforms of\n"
"   Sampled Functions. Theory of Computing 8:415-428.\n"
"   https://cs.brown.edu/people/pfelzens/papers/dt-final.pdf\n"
"\n"
"Thanks to C. Rorden for pointing this paper out and discussing it.\n"
"\n"
"The current code here extends/tweaks the FH2012 algorithm to a more\n"
"general case of having several different ROIs present, for running\n"
"in 3D (trivial extension), and for having voxels of non-unity and\n"
"non-isotropic lengths.  It does this by utilizing the fact that at\n"
"its very heart, the FH2012 algorithm works line by line and can even\n"
"be thought of as working boundary-by-boundary.\n"
"\n"
"Here, the zero-valued 'background' is also just treated like an ROI,\n"
"with one difference.  At a FOV boundary, the zero-valued\n"
"ROI/backgroud is treated as open, so that the EDT value at each\n"
"'zero' voxel is always to one of the shapes within the FOV.  For\n"
"nonzero ROIs, one can treat the FOV boundary *either* as an ROI edge\n"
"(EDT value there will be 1 edge length) *or* as being open.\n"
"\n"
"==========================================================================\n"
"\n"
"Command usage and option list ~1~ \n"
"\n"
"    3dEulerDist [options] -prefix PREF -input DSET\n"
"\n"
"where: \n"
"\n"
"  -input DSET      :(req) input dataset\n"
"\n"
"  -prefix PREF     :(req) output prefix name\n"
"\n"
"  -mask  MASK      :mask dataset.  NB: this mask is only applied *after*\n"
"                    the EDT has been calculated.  Therefore, the boundaries\n"
"                    of this mask have no affect on the calculated distance\n"
"                    values, except for potentially zeroing some out at the\n"
"                    end.\n"
"\n"
"  -dist_sq         :by default, the output EDT volume contains distance\n"
"                    values.  By using this option, the output values are\n"
"                    distance**2.\n"
"\n"
"  -zeros_are_zero  :by default, EDT values are output for the full FOV,\n"
"                    even zero-valued regions.  If this option is used, EDT\n"
"                    values are only reported within the nonzero locations\n"
"                    of the input dataset.\n"
"\n"
"  -zeros_are_neg   :if this option is used, EDT in the zero/background\n"
"                    of the input will be negative (def: they are positive).\n"
"                    This opt cannot be used if '-zeros_are_zero' is.\n"
"\n"
"  -nz_are_neg      :if this option is used, EDT in the nonzero ROI regions\n"
"                    of the input will be negative (def: they are positive).\n"
"\n"
"  -bounds_are_zero :this flag affects how FOV boundaries are treated for\n"
"                    nonzero ROIs: by default, they are viewed as ROI\n"
"                    boundaries (so the FOV is a closed boundary for an ROI);\n"
"                    but when this option is used, the ROI behaves as if it\n"
"                    continued 'infinitely' at the FOV boundary (so it is\n"
"                    an open boundary).  Zero-valued ROIs (= background)\n"
"                    are not affected by this option.\n"
"\n"
" -ignore_voxdims   :this EDT algorithm works in terms of physical distance\n"
"                    and uses the voxel dimension info in each direction, by\n"
"                    default.  However, using this option will ignore voxel\n"
"                    size, producing outputs as if each voxel dimension was\n"
"                    unity.\n"
"\n"
"  -only2D   SLI    :instead of running full 3D EDT, run just in 2D, per.\n"
"                    plane.  Provide the slice plane you want to run along\n"
"                    as the single argument SLI:\n"
"                       \"axi\"  -> for axial slice\n"
"                       \"cor\"  -> for coronal slice\n"
"                       \"sag\"  -> for sagittal slice\n"
"\n"
" -verb V           :manage verbosity when running code (def: 1).\n"
"                    Providing a V of 0 means to run quietly.\n"
"\n"
"==========================================================================\n"
"\n"
"Examples ~1~\n"
"\n"
"1) Basic case:\n"
"   3dEulerDist                                                     \\\n"
"       -input  roi_map.nii.gz                                      \\\n"
"       -prefix roi_map_EDT.nii.gz                                  \n"
"\n"
"2) Same as above, but only output distances within nonzero regions/ROIs:\n"
"   3dEulerDist                                                     \\\n"
"       -zeros_are_zero                                             \\\n"
"       -input  roi_map.nii.gz                                      \\\n"
"       -prefix roi_map_EDT_ZZ.nii.gz                               \n"
"\n"
"3) Output distance-squared at each voxel:\n"
"   3dEulerDist                                                     \\\n"
"       -dist_sq                                                    \\\n"
"       -input  mask.nii.gz                                         \\\n"
"       -prefix mask_EDT_SQ.nii.gz                                  \n"
"\n"
"4) Distinguish ROIs from nonzero background by making the former have\n"
"   negative distance values in output:\n"
"   3dEulerDist                                                     \\\n"
"       -nz_are_neg                                                 \\\n"
"       -input  roi_map.nii.gz                                      \\\n"
"       -prefix roi_map_EDT_NZNEG.nii.gz                            \n"
"\n"
"5) Have output voxel values represent (number of vox)**2 from a boundary;\n"
"   voxel dimensions are ignored here:\n"
"   3dEulerDist                                                     \\\n"
"       -ignore_voxdims                                             \\\n"
"       -dist_sq                                                    \\\n"
"       -input  roi_map.nii.gz                                      \\\n"
"       -prefix roi_map_EDT_SQ_VOX.nii.gz                           \n"
"\n"
"==========================================================================\n"
"\n",
author );

	return 0;
}

int main(int argc, char *argv[]) {

   int ii = 0;
   int iarg;
   PARAMS_euler_dist InOpts;
   int itmp;

   mainENTRY("3dEulerDist"); machdep(); 
  
   // fill option struct with defaults
   InOpts = set_euler_dist_defaults();

   // ****************************************************************
   //                  parse command line arguments
   // ****************************************************************
	
   if (argc == 1) { usage_3dEulerDist(); exit(0); }

   /* scan through args */
   iarg = 1; 
   while( iarg < argc && argv[iarg][0] == '-' ){

      if( strcmp(argv[iarg],"-help") == 0 || 
          strcmp(argv[iarg],"-h") == 0 ) {
         usage_3dEulerDist();
         exit(0);
      }
			 
      if( strcmp(argv[iarg],"-input") == 0 ){
         if( ++iarg >= argc ) 
            ERROR_exit("Need argument after '%s'", argv[iarg-1]);

         InOpts.input_name = strdup(argv[iarg]) ;
         iarg++ ; continue ;
      }

      if( strcmp(argv[iarg],"-mask") == 0 ){
         if( ++iarg >= argc ) 
            ERROR_exit("Need argument after '%s'", argv[iarg-1]);

         InOpts.mask_name = strdup(argv[iarg]) ;			
         iarg++ ; continue ;
      }

      if( strcmp(argv[iarg],"-prefix") == 0 ){
         if( ++iarg >= argc ) 
            ERROR_exit("Need argument after '%s'", argv[iarg-1]);

         InOpts.prefix = strdup(argv[iarg]) ;

         if( !THD_filename_ok(InOpts.prefix) ) 
            ERROR_exit("Illegal name after '%s'", argv[iarg-1]);
         iarg++ ; continue ;
      }

      if( strcmp(argv[iarg],"-zeros_are_zero") == 0) {
         InOpts.zeros_are_zeroed = 1;
         iarg++ ; continue ;
      }

      if( strcmp(argv[iarg],"-zeros_are_neg") == 0) {
         InOpts.zeros_are_neg = 1;
         iarg++ ; continue ;
      }

      if( strcmp(argv[iarg],"-nz_are_neg") == 0) {
         InOpts.nz_are_neg = 1;
         iarg++ ; continue ;
      }

      if( strcmp(argv[iarg],"-bounds_are_not_zero") == 0) {
         InOpts.bounds_are_zero = 0;
         iarg++ ; continue ;
      }

      if( strcmp(argv[iarg],"-dist_sq") == 0) {
         InOpts.dist_sq = 1;
         iarg++ ; continue ;
      }

      if( strcmp(argv[iarg],"-ignore_voxdims") == 0) {
         InOpts.ignore_voxdims = 1;
         iarg++ ; continue ;
      }

      if( strcmp(argv[iarg],"-only2D") == 0) {
         if( ++iarg >= argc ) 
            ERROR_exit("Need argument after '%s'", argv[iarg-1]);

         if( strcmp(argv[iarg],"cor") == 0 || \
             strcmp(argv[iarg],"axi") == 0 || \
             strcmp(argv[iarg],"sag") == 0 )
            InOpts.only2D = strdup(argv[iarg]);
         else
            ERROR_exit("Need either \"cor\", \"axi\" or \"sag\" "
                       "after '%s'", argv[iarg-1]);

         iarg++ ; continue ;
      }

      if( strcmp(argv[iarg],"-verb") == 0) {
         if( ++iarg >= argc ) 
            ERROR_exit("Need int>0 argument after '%s'", argv[iarg-1]);

         itmp = atoi(argv[iarg]);
         if( itmp <= 0 )
            InOpts.verb = 0;
         else
            InOpts.verb = itmp;

         iarg++ ; continue ;
      }

      ERROR_message("Bad option '%s'\n",argv[iarg]);
      suggest_best_prog_option(argv[0], argv[iarg]);
      exit(1);
   }
	
   // ****************************************************************
   //               verify presence+behavior of inputs
   // ****************************************************************

   if ( InOpts.verb )
      INFO_message("EDT: starting to check inputs...");

   if ( !InOpts.input_name ) { 
      ERROR_message("You need to provide an input dset with '-input ..'");
      exit(1);
   }

   if ( !InOpts.prefix )
      ERROR_exit("Need an output name via '-prefix ..'\n");

   if ( InOpts.zeros_are_zeroed && InOpts.zeros_are_neg  )
      ERROR_exit("Cannot combine '-zeros_are_zero' and '-zeros_are_neg'.  "
                 "You must choose which you *really* want.\n");

   // DONE FILLING, now call
   ii = run_EDT_3D(1, InOpts, argc, argv);

   return 0;
}


int run_EDT_3D( int comline, PARAMS_euler_dist opts,
                int argc, char *argv[] )
{
   int i, j, k, idx;
   int nn;
   int nx, ny, nz, nxy, nvox, nvals;
	THD_3dim_dataset *dset_roi = NULL;          // input
   THD_3dim_dataset *dset_mask = NULL;         // mask
	THD_3dim_dataset *dset_edt = NULL;          // output
   
   ENTRY("run_EDT_3D");

   dset_roi = THD_open_dataset(opts.input_name);
   if( (dset_roi == NULL ))
      ERROR_exit("Can't open dataset '%s'", opts.input_name);
   DSET_load(dset_roi); CHECK_LOAD_ERROR(dset_roi);

   if( opts.mask_name ) {
      dset_mask = THD_open_dataset(opts.mask_name);
      if( dset_mask == NULL )
         ERROR_exit("Can't open dataset '%s'", opts.mask_name);
      DSET_load(dset_mask); CHECK_LOAD_ERROR(dset_mask);

      if( THD_dataset_mismatch( dset_roi , dset_mask ) )
         ERROR_exit("Mismatch between input and mask dsets!\n");
   }

   // if only running in 2D, figure out which slice that is
   if ( opts.only2D )
      i = choose_axes_for_plane( dset_roi, opts.only2D,
                                 opts.axes_to_proc, opts.verb );
   
   nx = DSET_NX(dset_roi);
   ny = DSET_NY(dset_roi);
   nz = DSET_NZ(dset_roi);
   nxy = nx*ny;
   nvox = DSET_NVOX(dset_roi);
   nvals = DSET_NVALS(dset_roi);

   /* Prepare header for output by copying that of input, and then
      changing items as necessary */
   dset_edt = EDIT_empty_copy( dset_roi ); 
   EDIT_dset_items(dset_edt,
                   ADN_datum_all, MRI_float ,    
                   ADN_prefix, opts.prefix,
                   ADN_none );

   for( nn=0 ; nn<nvals ; nn++ ){
      i = calc_EDT_3D(dset_edt, opts, dset_roi, dset_mask, nn);
   } // end of loop over nvals

   // free input dset
	DSET_delete(dset_roi); 
  	free(dset_roi); 

   // prepare to write output
	THD_load_statistics( dset_edt );
	if( !THD_ok_overwrite() && THD_is_ondisk(DSET_HEADNAME(dset_edt)) )
		ERROR_exit("Can't overwrite existing dataset '%s'",
					  DSET_HEADNAME(dset_edt));
	tross_Make_History("3dEulerDist", argc, argv, dset_edt);

   // write and free dset 
	THD_write_3dim_dataset(NULL, NULL, dset_edt, True);
	DSET_delete(dset_edt); 
  	free(dset_edt); 

   // free more
   if( dset_mask ){
      DSET_delete(dset_mask);
      free(dset_mask);
   }

   return 0;
}

