#ifndef THD_EDT_INCLUDED
#define THD_EDT_INCLUDED

#define BIG FLT_MAX            // from float.h

/* struct of quantities for running Euler Distance Transform (EDT) 

    only2D       : (str) name of 2D plane to work within; so EDT is calced
                   across whole FOV, but only planewise with each of the
                   given planes

    dist_sq      : if False (def), the output image of EDT values is distance
                   values; otherwise, the values are distance**2 (because
                   that is what the program works with).

    bounds_are_zero : switch for how to treat FOV boundaries for
                   nonzero-ROIs; True means they are ROI boundaries
                   (so FOV is a "closed" boundary for the ROI), and
                   False means that the ROI continues "infinitely" at
                   FOV boundary (so it is "open").  Zero-valued ROIs
                   (= background) are not affected by this value.

    zeros_are_zeroed : if False, EDT values are output for the
                   zero-valued region; otherwise, zero them out, so EDT
                   values are only reported in the non-zero ROIs. NB: the
                   EDT values are still calculated everywhere; it is just
                   a question of zeroing out later (no time saved for True).

    ignore_voxdims : we have implemented an EDT alg that works in
                   terms of physical distance and can use the voxel dimension
                   info in each direction.  However, users might want 'depth'
                   just in voxel units (so, delta=1 everywhere), which 
                   using this option will give 'em.                   

    edims        : (len=3 fl arr) element dimensions (here, voxel edge lengths)

    shape        : (len=3 int arr) matrix size in each direction

    axes_to_proc : (len=3 int arr) switches for running EDT along selected 
                   axes;  default to run with all of them.

*/
typedef struct {

   char *input_name;      
   char *mask_name;      
   char *prefix;          

   int zeros_are_zeroed;  
   int zeros_are_neg;  
   int nz_are_neg;  
   int bounds_are_zero;   
   int ignore_voxdims;
   int dist_sq;           

   float edims[3];        
   int   shape[3];        

   char *only2D;          
   int axes_to_proc[3];

   int verb;

} PARAMS_euler_dist;

/* function to initialize EDT params */
PARAMS_euler_dist set_euler_dist_defaults(void);

// ---------------------------------------------------------------------------

int sort_vox_ord_desc(int N, float *Ledge, int *ord);

int choose_axes_for_plane( THD_3dim_dataset *dset, char *which_slice,
                           int *onoff_arr, int verb );

int apply_opts_to_edt_arr( float ***arr_dist, PARAMS_euler_dist opts,
                           THD_3dim_dataset *dset_roi, int ival);

int calc_EDT_3D( THD_3dim_dataset *dset_edt, PARAMS_euler_dist opts,
                 THD_3dim_dataset *dset_roi, THD_3dim_dataset *dset_mask,
                 int ival);

int calc_EDT_3D_dim0( float ***arr_dist, PARAMS_euler_dist opts,
                      THD_3dim_dataset *dset_roi, int ival,
                      float *flarr, int *maparr );
int calc_EDT_3D_dim1( float ***arr_dist, PARAMS_euler_dist opts,
                      THD_3dim_dataset *dset_roi, int ival,
                      float *flarr, int *maparr );
int calc_EDT_3D_dim2( float ***arr_dist, PARAMS_euler_dist opts,
                      THD_3dim_dataset *dset_roi, int ival,
                      float *flarr, int *maparr );

int run_EDTD_per_line( float *dist2_line, int *roi_line, int Na,
                       float delta, int bounds_are_zero );

float * Euclidean_DT_delta(float *f0, int n, float delta);

#endif
