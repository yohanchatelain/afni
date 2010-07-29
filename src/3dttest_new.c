#include "mrilib.h"

/*------------------------------- prototypes, etc. ---------------------------*/

#undef  MAXCOV
#define MAXCOV 31

#undef  MAX_LABEL_SIZE
#define MAX_LABEL_SIZE 12

#undef  LTRUNC
#define LTRUNC(ss) \
 do{ if( strlen(ss) > MAX_LABEL_SIZE ){(ss)[MAX_LABEL_SIZE] = '\0'; }} while(0)

static int toz = 0 ;  /* convert t-statistics to z-scores? */
static int twosam = 0 ;

void regress_toz( int numA , float *zA ,
                  int numB , float *zB , int opcode ,
                  int mcov ,
                  float *xA , float *psinvA , float *xtxinvA ,
                  float *xB , float *psinvB , float *xtxinvB ,
                  float *outvec , float *workspace             ) ;

float_pair ttest_toz( int numx, float *xar, int numy, float *yar, int opcode ) ;

double GIC_student_t2z( double tt , double dof ) ;

static NI_element         *covnel=NULL ;       /* covariates */
static NI_str_array       *covlab=NULL ;

static int        num_covset_col=0 ;
static int             allow_cov=1 ;
static MRI_vectim   **covvim_AAA=NULL ;
static MRI_vectim   **covvim_BBB=NULL ;
static floatvec     **covvec_AAA=NULL ;
static floatvec     **covvec_BBB=NULL ;

static unsigned int testA, testB, testAB ;

static int mcov  = 0 ;
static int nvout = 0 ;
MRI_IMAGE *Axxim=NULL , *Bxxim=NULL ;
static float *Axx=NULL , *Axx_psinv=NULL , *Axx_xtxinv=NULL ;
static float *Bxx=NULL , *Bxx_psinv=NULL , *Bxx_xtxinv=NULL ;

static char *prefix = "TTnew" ;
static byte *mask   = NULL ;
static int  nmask   = 0 ;
static int  nvox    = 0 ;
static int  nmask_hits = 0 ;

static int ttest_opcode = 0 ;  /* 0=pooled, 1=unpooled, 2=paired */

static int               ndset_AAA=0 , nval_AAA=0 ;
static char              *snam_AAA=NULL , *lnam_AAA=NULL ;
static char             **name_AAA=NULL ;
static char             **labl_AAA=NULL ;
static THD_3dim_dataset **dset_AAA=NULL ;
static MRI_vectim      *vectim_AAA=NULL ;

static int               ndset_BBB=0 , nval_BBB=0 ;
static char              *snam_BBB=NULL , *lnam_BBB=NULL ;
static char             **name_BBB=NULL ;
static char             **labl_BBB=NULL ;
static THD_3dim_dataset **dset_BBB=NULL ;
static MRI_vectim      *vectim_BBB=NULL ;

/*--------------------------------------------------------------------------*/

static int string_search( char *targ , int nstr , char **str )
{
   int ii ;

   if( targ == NULL || *targ == '\0' || str == NULL || nstr < 1 ) return -1 ;

   for( ii=0 ; ii < nstr ; ii++ )
     if( str[ii] != NULL && strcmp(targ,str[ii]) == 0 ) return ii ;

   return -1 ;
}

/*----------------------------------------------------------------------------*/

static void vstep_print(void)   /* pacifier */
{
   static char xx[10] = "0123456789" ; static int vn=0 ;
   fprintf(stderr , "%c" , xx[vn%10] ) ;
   if( vn%10 == 9) fprintf(stderr,".") ; vn++ ;
}

/*--------------------------------------------------------------------------*/

void display_help_menu(void)
{
   printf(
      "Gosset (Student) t-test of sets of 3D datasets.\n"
      "\n"
      "* Usage can be similar to the old 3dttest; for example:\n"
      "\n"
      "    3dttest_new -setA a+tlrc'[3]' b+tlrc'[3]' ...\n"
      "\n"
      "* OR, usage can be similar to 3dMEMA; for example:\n"
      "\n"
      "    3dttest_new -setA Green sub001 a+tlrc'[3]' \\\n"
      "                            sub002 b+tlrc'[3]' \\\n"
      "                            sub003 c+tlrc'[3]' \\\n"
      "                            ...                \\\n"
      "                -covariates Cfile\n"
      "\n"
      "* You can input 1 or 2 sets of data (labeled 'A' and 'B').\n"
      "\n"
      "* With 1 set ('-setA'), the mean across input datasets (usually subjects)\n"
      "   is tested against 0.\n"
      "\n"
      "* With 2 sets, the difference in means across each set is tested\n"
      "   against 0.  The 1 sample results for each set are also provided, since\n"
      "   these are often of interest to the investigator (e.g., YOU).\n"
      "\n"
      "* Covariates can be per-dataset (input=1 number) and/or per-voxel/per-dataset\n"
      "   (input=1 dataset sub-brick).  Please note that voxel-level covariates will\n"
      "   slow the program down, since the regression matrix for the covariates must\n"
      "   be re-inverted for each voxel separately.\n"
      "\n"
      "* This program is meant (for most uses) to replace the original 3dttest,\n"
      "   which was written in 1994, \"When grass was green and grain was yellow\".\n"
      "\n"
      "-----------\n"
      "SET OPTIONS\n"
      "-----------\n"
      "* At least the '-setA' option must be given.  '-setB' is optional, and\n"
      "   if it isn't used, then the mean of the dataset values from '-setA' is\n"
      "   t-tested against 0 (1 sample t-test).\n"
      "\n"
      "* Two forms for the '-setX' (X='A' or 'B') options are allowed.  The first\n"
      "   (short) form is similar to the original 3dttest program, where the option\n"
      "   is just followed by a list of datasets to use.\n"
      "\n"
      "* The second (long) form is similar to the 3dMEMA program, where you specify\n"
      "   a label for each input dataset sub-brick (a difference between this\n"
      "   option and the version in 3dMEMA is only that you do not give a second\n"
      "   dataset ('T_DSET') with each sample in this program).\n"
      "\n"
      "** SHORT FORM **\n"
      "\n"
      " -setA BETA_DSET BETA_DSET ...\n"
      "[-setB]\n"
      "* In this form of input, you specify the datasets for each set\n"
      "   directly following the '-setX' option.\n"
      "  ++ Unlike 3dttest, you can specify multiple sub-bricks in a dataset:\n"
      "        -setA a+tlrc'[1..13(2)]'\n"
      "     which inputs 7 sub-bricks at once (1,3,5,7,9,11,13).\n"
      "\n"
      "** LONG FORM **\n"
      "\n"
      " -setA SETNAME            \\\n"
      "[-setB]  LABL_1 BETA_DSET \\\n"
      "         LABL_2 BETA_DSET \\\n"
      "         ...    ...       \\\n"
      "         LABL_N BETA_DSET\n"
      "* Specify the data for one of the test variables.\n"
      "   SETNAME   is the name assigned to the set (used in the output labels).\n"
      "   LABL_K    is the label for the Kth input dataset name, whose name follows.\n"
      "   BETA_DSET is the name of the dataset of the beta coefficient or GLT.\n"
      "             ++ only 1 sub-brick can be specified here!\n"
      "   Note that the labels 'SETNAME' and 'LABL_K' are limited to 12\n"
      "   characters -- any more will be thrown away without warning.\n"
      "\n"
      " -labelA SETNAME = for the short form of '-setX', this option allows you\n"
      "[-labelB]          to attach a label to the set, which will be used in\n"
      "                   the sub-brick labels in the output dataset.  If you don't\n"
      "                   give a SETNAME, then '-setA' will be named 'SetA', etc.\n"
      "\n"
      "  ***** NOTE WELL: The sign of a two sample test is A - B           *****\n"
      "  ***              Thus, '-setB' corresponds to '-set1' in 3dttest,   ***\n"
      "  ***                and '-setA' corresponds to '-set2' in 3dttest.   ***\n"
      "  *****            This ordering of A and B matches 3dGroupInCorr.  *****\n"
      "\n"
      "----------\n"
      "COVARIATES\n"
      "----------\n"
      " -covariates COVAR_FILE\n"
      "\n"
      "* COVAR_FILE is the name of a text file with a table for the covariate(s).\n"
      "   Each column in the file is treated as a separate covariate, and each\n"
      "   row contains the values of these covariates for each sample. Note that\n"
      "   you can use '-covariates' only once -- the COVAR_FILE should contain\n"
      "   the covariates for ALL input samples from both sets.\n"
      "  ++ Rows in COVAR_FILE that don't match a dataset label are ignored.\n"
      "\n"
      "* The format of COVAR_FILE is like that for 3dMEMA and 3dGroupInCorr:\n"
      "     FIRST LINE -->   subject IQ   age  GMfrac\n"
      "     LATER LINES -->  Elvis   143   42  Elvis_GM+tlrc[8]\n"
      "                      Fred     85   59  Fred_GM+tlrc[8]\n"
      "                      Ethel   109   49  Ethel_GM+tlrc[8]\n"
      "                      Lucy    133   32  Lucy_GM+tlrc[8]\n"
      "                      Ricky   121   37  Ricky_GM+tlrc[8]\n"
      "\n"
      "* The first column contains the labels that must match the dataset\n"
      "   LABL_K labels given in the '-setX' option(s).\n"
      "\n"
      "* If you used a short form '-setX' option, each dataset label is\n"
      "   the dataset's prefix name, truncated to 12 characters.\n"
      "\n"
      "* '-covariates' can only be used with the short form '-setX' option\n"
      "   if each input dataset has only 1 sub-brick (so that each label\n"
      "   refers to exactly 1 volume of data).\n"
      "\n"
      "* The later columns in COVAR_FILE contain numbers (e.g., 'IQ' and 'age',\n"
      "    above), or dataset names.  In the latter case, you are specifying a\n"
      "    voxel-wise covariate (e.g., 'GMfrac').\n"
      "\n"
      "* A column can contain numbers only, or datasets only.  But one\n"
      "   column CANNOT contain a mix of numbers and dataset names!\n"
      "\n"
      "* The first line of COVAR_FILE contains column headers.  The header label\n"
      "   for the first column isn't used for anything.  The later header labels\n"
      "   are used in the sub-brick labels sent to AFNI.\n"
      "\n"
      "* Only the -paired and -pooled options can be used with covariates.\n"
      "  ++ If you use -unpooled, it will be changed to -pooled.\n"
      "\n"
      "* If you use -paired, then the covariates for -setB will be the same\n"
      "   as those for -setA, even if the dataset labels are different!\n"
      "\n"
      "* Each covariate column in the regression matrix will have its mean\n"
      "   removed (centered). If there are 2 sets of subjects, each set's\n"
      "   matrix will be centered separately.\n"
      "\n"
      "* See the section 'STRUCTURE OF THE OUTPUT DATASET' for details of\n"
      "   what is calculated and stored by 3dttest_new.\n"
      "\n"
      "* A maximum of 31 covariates are allowed.  If you have more, then\n"
      "   seriously consider the likelihood that you are completely demented.\n"
      "\n"
      "* N.B.: The simpler forms of the COVAR_FILE that 3dMEMA allows are\n"
      "        NOT supported here!\n"
      "\n"
      "-------------\n"
      "OTHER OPTIONS\n"
      "-------------\n"
      "\n"
      " -paired   = Specifies the use of a paired-sample t-test to\n"
      "              compare setA and setB.  If this option is used,\n"
      "              setA and setB must have the same cardinality (duh).\n"
      "             ++ Recall that if '-paired' is used with '-covariates',\n"
      "                 the covariates for setB will be the same as for setA.\n"
      "\n"
      " -unpooled = Specifies that the variance estimates for setA and\n"
      "              setB be computed separately (not pooled together).\n"
      "             ++ This only makes sense if -paired is NOT given.\n"
      "             ++ '-unpooled' cannot be used with '-covariates'.\n"
      "             ++ Unpooled variance estimates are supposed to\n"
      "                 provide some protection against heteroscedasticty.\n"
      "                 Our experience is that for most FMRI data, using\n"
      "                 '-unpooled' is not needed.\n"
      "\n"
      " -toz      = Convert output t-statistics to z-scores\n"
      "             ++ This might be useful for the '-unpooled' case, where\n"
      "                 the t-statistics aren't directly comparable since\n"
      "                 the degrees of freedom will vary between voxels.\n"
      "\n"
      " -mask mmm = Only compute results for voxels in the specified mask.\n"
      "             ++ Voxels not in the mask will be set to 0 in the output.\n"
      "             ++ If '-mask' is not used, all voxels will be tested.\n"
      "\n"
      " -prefix p = Gives the name of the output dataset file.\n"
      "\n"
      "-------------------------------\n"
      "STRUCTURE OF THE OUTPUT DATASET\n"
      "-------------------------------\n"
      "* The output dataset is stored in float format; there is no option\n"
      "   to store it in scaled short format.\n"
      "\n"
      "* For each covariate, 2 sub-bricks are produced:\n"
      "  ++ The estimated slope of the beta values vs covariate\n"
      "  ++ The t-statistic of this slope\n"
      "  ++ If there are 2 sets of subjects, then each pair of sub-bricks is\n"
      "      produced for the setA-setB, setA, and setB cases, so that you'll\n"
      "      get 6 sub-bricks per covariate (plus 6 more for the mean, which\n"
      "      is treated as a special covariate whose values are all 1).\n"
      "  ++ Thus the number of sub-bricks produced is 6*(m+1) for the two-sample\n"
      "      case and 2*(m+1) for the one-sample case, where m=number of covariates.\n"
      "\n"
      "* For example, if there is one covariate 'IQ', and a two sample analysis\n"
      "   is carried out ('-setA' and '-setB' both used), then the output\n"
      "   dataset will contain the following 12 sub-bricks:\n"
      "      #0  SetA-SetB_mean      = difference of means [covariates removed]\n"
      "      #1  SetA-SetB_Tstat\n"
      "      #2  SetA-SetB_IQ        = difference of slopes wrt covariate IQ\n"
      "      #3  SetA-SetB_IQ_Tstat\n"
      "      #4  SetA_mean           = mean of SetA [covariates removed]\n"
      "      #5  SetA_Tstat\n"
      "      #6  SetA_IQ             = slope of SetA wrt covariate IQ\n"
      "      #7  SetA_IQ_Tstat\n"
      "      #8  SetB_mean           = mean of SetB [covariates removed]\n"
      "      #9  SetB_Tstat\n"
      "      #10 SetB_IQ             = slope of SetB wrt covariate IQ\n"
      "      #11 SetB_IQ_Tstat\n"
      "\n"
      "* In the above, 'wrt' is standard mathematical shorthand for the\n"
      "   phrase 'with respect to'.\n"
      "\n"
      "* If option '-toz' is used, the 'Tstat' will be replaced with 'Zscr'\n"
      "   in the statistical sub-brick labels.\n"
      "\n"
      "* If the long form of '-setA' is used, or '-labelA' is given, then\n"
      "   'SetA' is the sub-brick labels is replaced with the corresponding\n"
      "   SETNAME.  (Mutatis mutandis for 'SetB', of course.)\n"
      "\n"
      "* If you produce a NIfTI-1 file, then the sub-brick labels are saved\n"
      "   in the AFNI extension in the .nii file.  Processing further in\n"
      "   non-AFNI programs will probably cause these labels to be lost\n"
      "   (along with other niceties, such as the history field).\n"
      "\n"
      "* Although you might not care about some of the results, there is no\n"
      "   option to turn off the output of all the sub-bricks described above.\n"
      "\n"
      "-------------------------\n"
      "VARIOUS LINKS OF INTEREST\n"
      "-------------------------\n"
      "* http://en.wikipedia.org/wiki/T_test\n"
      "* http://www.statsoft.com/textbook/basic-statistics/\n"
      "* http://en.wikipedia.org/wiki/Mutatis_mutandis\n"
      "\n"
      "----------------------------------------------------\n"
      "AUTHOR -- RW Cox -- don't whine TO me; wine WITH me.\n"
      "----------------------------------------------------\n"
  ) ;

  PRINT_COMPILE_DATE ; exit(0) ;
}

/*----------------------------------------------------------------------------*/

int main( int argc , char *argv[] )
{
   int nopt , nbad , ii,jj,kk , kout,ivox , vstep ;
   MRI_vectim *vimout ;
   float *workspace=NULL , *datAAA , *datBBB , *resar ;
   float_pair tpair ;
   THD_3dim_dataset *outset ;
   char blab[64] , *stnam ;
   float dof_AB=0.0f , dof_A=0.0f , dof_B=0.0f ;

   /*--- help the piteous luser? ---*/

   if( argc < 2 || strcmp(argv[1],"-help") == 0 ) display_help_menu() ;

   /*--- record things for posterity, et cetera ---*/

   mainENTRY("3dttest_new main"); machdep(); AFNI_logger("3dttest_new",argc,argv);
   PRINT_VERSION("3dttest_new") ; AUTHOR("The Bob") ;

   /*--- read the options from the command line ---*/

   nopt = 1 ;
   while( nopt < argc ){

     /*----- prefix -----*/

     if( strcmp(argv[nopt],"-prefix") == 0 ){
       if( ++nopt >= argc )
         ERROR_exit("Need argument after '%s'",argv[nopt-1]) ;
       prefix = strdup(argv[nopt]) ;
       if( !THD_filename_ok(prefix) )
         ERROR_exit("Prefix '%s' is not acceptable",prefix) ;
       nopt++ ; continue ;
     }

     /*----- mask -----*/

     if( strcmp(argv[nopt],"-mask") == 0 ){
       bytevec *bvec ;
       if( mask != NULL )
         ERROR_exit("Can't use '-mask' twice!") ;
       if( ++nopt >= argc )
         ERROR_exit("Need argument after '%s'",argv[nopt-1]) ;
       bvec = THD_create_mask_from_string(argv[nopt]) ;
       if( bvec == NULL )
         ERROR_exit("Can't create mask from '-mask' option") ;
       mask = bvec->ar ; nmask = bvec->nar ;
       nmask_hits = THD_countmask( nmask , mask ) ;
       if( nmask_hits > 0 )
         INFO_message("%d voxels in -mask dataset",nmask_hits) ;
       else
         ERROR_exit("no nonzero voxels in -mask dataset") ;
       nopt++ ; continue ;
     }

     /*----- statistics options -----*/

     if( strcasecmp(argv[nopt],"-pooled") == 0 ){
       ttest_opcode = 0 ; nopt++ ; continue ;
     }

     if( strcasecmp(argv[nopt],"-unpooled") == 0 ){
       ttest_opcode = 1 ; nopt++ ; continue ;
     }

     if( strcasecmp(argv[nopt],"-paired") == 0 ){
       ttest_opcode = 2 ; nopt++ ; continue ;
     }

     if( strcasecmp(argv[nopt],"-toz") == 0 ){
       toz = 1 ; nopt++ ; continue ;
     }

     if( strcasecmp(argv[nopt],"-labelA") == 0 ){
       if( ++nopt >= argc )
         ERROR_exit("Need argument after '%s'",argv[nopt-1]) ;
       lnam_AAA = strdup(argv[nopt]) ; LTRUNC(lnam_AAA) ;
       nopt++ ; continue ;
     }

     if( strcasecmp(argv[nopt],"-labelB") == 0 ){
       if( ++nopt >= argc )
         ERROR_exit("Need argument after '%s'",argv[nopt-1]) ;
       lnam_BBB = strdup(argv[nopt]) ; LTRUNC(lnam_BBB) ;
       nopt++ ; continue ;
     }

     /*----- the various flavours of '-set' -----*/

     if( strncmp(argv[nopt],"-set",4) == 0 ){
       char cc=argv[nopt][4] , *onam=argv[nopt] , *cpt ;
       int nds=0 , ids , nv=0 ; char **nams=NULL , **labs=NULL , *snam=NULL ;
       THD_3dim_dataset *qset , **dset=NULL ;

       if( cc == 'A' ){
         if( ndset_AAA > 0 ) ERROR_exit("Can't use '-setA' twice!") ;
       } else if( cc == 'B' ){
         if( ndset_BBB > 0 ) ERROR_exit("Can't use '-setB' twice!") ;
       } else {
         ERROR_exit("'%s' is not a recognized '-set' option",argv[nopt]);
       }
       if( ++nopt >= argc-1 )
         ERROR_exit("Need arguments after '%s'",argv[nopt-1]) ;

       /* if next arg is a dataset, then all of the next args are datasets */

       qset = THD_open_dataset( argv[nopt] ) ;
       if( ISVALID_DSET(qset) ){
         nds  = 1 ;
         nams = (char **)malloc(sizeof(char *)) ;
         labs = (char **)malloc(sizeof(char *)) ;
         dset = (THD_3dim_dataset **)malloc(sizeof(THD_3dim_dataset *)) ;
         nams[0] = strdup(argv[nopt]) ; dset[0] = qset ;
         labs[0] = strdup(THD_trailname(argv[nopt],0)) ; LTRUNC(labs[0]) ;
         cpt = strchr(labs[0]+1,'+') ; if( cpt != NULL ) *cpt = '\0' ;
         cpt = strchr(labs[0]+1,'.') ; if( cpt != NULL ) *cpt = '\0' ;
         nv = DSET_NVALS(qset) ;
         for( nopt++ ; nopt < argc && argv[nopt][0] != '-' ; nopt++ ){
           qset = THD_open_dataset( argv[nopt] ) ;
           if( !ISVALID_DSET(qset) )
             ERROR_exit("Option %s: cannot open dataset '%s'",onam,argv[nopt]) ;
           nds++ ; nv += DSET_NVALS(qset) ;
           nams = (char **)realloc(nams,sizeof(char *)*nds) ;
           labs = (char **)realloc(labs,sizeof(char *)*nds) ;
           dset = (THD_3dim_dataset **)realloc(dset,sizeof(THD_3dim_dataset *)*nds) ;
           nams[nds-1] = strdup(argv[nopt]) ; dset[nds-1] = qset ;
           labs[nds-1] = strdup(THD_trailname(argv[nopt],0)) ; LTRUNC(labs[nds-1]) ;
           cpt = strchr(labs[nds-1]+1,'+') ; if( cpt != NULL ) *cpt = '\0' ;
           cpt = strchr(labs[nds-1]+1,'.') ; if( cpt != NULL ) *cpt = '\0' ;
         }

         if( nv > nds ) allow_cov = 0 ;

         if( nv < 2 )
           ERROR_exit("Option %s (short form): need at least 2 datasets or sub-bricks",onam) ;

       } else {  /* not a dataset => label label dset label dset ... */

         snam = strdup(argv[nopt]) ; LTRUNC(snam) ;
         for( nopt++ ; nopt < argc && argv[nopt][0] != '-' ; nopt+=2 ){
           if( nopt+1 >= argc || argv[nopt+1][0] == '-' )
             ERROR_exit("Option %s: ends prematurely",onam) ;
           qset = THD_open_dataset( argv[nopt+1] ) ;
           if( !ISVALID_DSET(qset) )
             ERROR_exit("Option %s: can't open dataset '%s'",onam,argv[nopt+1]) ;
           if( DSET_NVALS(qset) > 1 )
             ERROR_exit("Option %s: dataset '%s' has more than one sub-brick",
                        onam,argv[nopt+1]) ;
           nds++ ; nv++ ;
           nams = (char **)realloc(nams,sizeof(char *)*nds) ;
           labs = (char **)realloc(labs,sizeof(char *)*nds) ;
           dset = (THD_3dim_dataset **)realloc(dset,sizeof(THD_3dim_dataset *)*nds) ;
           nams[nds-1] = strdup(argv[nopt+1]) ; dset[nds-1] = qset ;
           labs[nds-1] = strdup(argv[nopt]  ) ; LTRUNC(labs[nds-1]) ;
         }

         if( nv < 2 )
           ERROR_exit("Option %s (long form): need at least 2 datasets",onam) ;
       }

       /* check for grid size mismatch */

       for( nbad=0,ids=1 ; ids < nds ; ids++ ){
         if( DSET_NVOX(dset[ids]) != DSET_NVOX(dset[0]) ){
           ERROR_message("Option %s: dataset '%s' does match first one in size",
                         onam,nams[ids]) ; nbad++ ;
         }
       }
       if( nbad > 0 ) ERROR_exit("Cannot go on after such an error!") ;

       /* assign results to global variables */

       if( cc == 'A' ){
         ndset_AAA = nds  ; snam_AAA = snam ; nval_AAA = nv ;
          name_AAA = nams ; labl_AAA = labs ; dset_AAA = dset ;
       } else {
         ndset_BBB = nds  ; snam_BBB = snam ; nval_BBB = nv ;
          name_BBB = nams ; labl_BBB = labs ; dset_BBB = dset ;
       }

       continue ;  /* nopt already points to next option */

     } /*----- end of '-set' -----*/

     /*----- covariates -----*/

     if( strcasecmp(argv[nopt],"-covariates") == 0 ){  /* 20 May 2010 */
       char *lab ; float sig ;
       if( ++nopt >= argc ) ERROR_exit("need 1 argument after option '%s'",argv[nopt-1]);
       if( covnel != NULL ) ERROR_exit("can't use -covariates twice!") ;
       covnel = THD_mixed_table_read( argv[nopt] ) ;
       if( covnel == NULL )
         ERROR_exit("Can't read table from -covariates file '%s'",argv[nopt]) ;
       INFO_message("Covariates file: %d columns, each with %d rows",
                    covnel->vec_num , covnel->vec_len ) ;
       mcov = covnel->vec_num - 1 ;
       if( mcov < 1 )
         ERROR_exit("Need at least 2 columns in -covariates file!") ;
       else if( mcov > MAXCOV )
         ERROR_exit("%d covariates in file, more than max allowed (%d)",mcov,MAXCOV) ;
       lab = NI_get_attribute( covnel , "Labels" ) ;
       if( lab != NULL ){
         ININFO_message("Covariate column labels: %s",lab) ;
         covlab = NI_decode_string_list( lab , ";," ) ;
         if( covlab == NULL || covlab->num < mcov+1 )
           ERROR_exit("can't decode labels properly?!") ;
       } else {
         ERROR_exit("Can't get labels from -covariates file '%s'",argv[nopt]) ;
       }
       for( nbad=0,jj=1 ; jj <= mcov ; jj++ ){
         if( covnel->vec_typ[jj] == NI_FLOAT ){  /* numeric column */
           meansigma_float(covnel->vec_len,(float *)covnel->vec[jj],NULL,&sig) ;
           if( sig <= 0.0f ){
             ERROR_message("Covariate '%s' is constant; how can this be used?!" ,
                           covlab->str[jj] ) ; nbad++ ;
           }
         } else {                                /* string column: */
           num_covset_col++ ;              /* count number of them */
         }
         LTRUNC(covlab->str[jj]) ;
       }
       if( nbad > 0 ) ERROR_exit("Cannot continue past above ERROR%s :-(",
                                  (nbad==1) ? "\0" : "s" ) ;
       nopt++ ; continue ;
     }

     /*----- bad user, bad bad bad -----*/

     ERROR_exit("3dttest_new: don't recognize option '%s'",argv[nopt]) ;

   }  /*-------------------- end of option parsing --------------------*/

   /*----- check some stuff -----*/

   twosam = (nval_BBB > 1) ; /* 2 sample test? */

   if( nval_AAA <= 0 )
     ERROR_exit("No '-setA' option?  Please please read the instructions!") ;

   if( nval_AAA != nval_BBB && ttest_opcode == 2 )
     ERROR_exit("Can't do '-paired' with unequal set sizes: #A=%d #B=%d",
                nval_AAA , nval_BBB ) ;

   nvox = DSET_NVOX(dset_AAA[0]) ;
   if( twosam && DSET_NVOX(dset_BBB[0]) != nvox )
     ERROR_exit("-setA and -setB datasets don't match number of voxels") ;

   if( nmask > 0 && nmask != nvox )
     ERROR_exit("-mask doesn't match datasets number of voxels") ;

   if( nval_AAA - mcov < 2 ||
       ( twosam && (nval_BBB - mcov < 2) ) )
     ERROR_exit("Too many covariates compared to number of datasets") ;

   if( mcov > 0 && !allow_cov )
     ERROR_exit(
     "-covariates not allowed with -set that has multiple sub-bricks from one dataset");

   if( ttest_opcode == 1 && mcov > 0 ){
     WARNING_message("-covariates does not support unpooled variance") ;
     ttest_opcode = 0 ;
   }

   if( nmask == 0 ){
     INFO_message("no mask ==> processing all %d voxels",nvox) ;
     nmask_hits = nvox ;
   }

   if( snam_AAA == NULL )
     snam_AAA = (lnam_AAA != NULL) ? lnam_AAA : strdup("SetA") ;
   if( snam_BBB == NULL )
     snam_BBB = (lnam_BBB != NULL) ? lnam_BBB : strdup("SetB") ;

   /*----- convert each input set of datasets to a vectim -----*/

   INFO_message("loading input datasets") ;

   vectim_AAA = THD_dset_list_to_vectim( ndset_AAA , dset_AAA , mask ) ;

   if( twosam )
     vectim_BBB = THD_dset_list_to_vectim( ndset_BBB , dset_BBB , mask ) ;

   /*----- set up covariates in a very lengthy aside now -----*/

   if( mcov > 0 ){
     THD_3dim_dataset **qset ; int nkbad ; float sum ;

     /*-- convert covariates to vectors to be loaded into matrices --*/

     /* for covariates which are just numbers, create float vector
        covvec_BBB[jj] for the jj-th covariate (jj=0..mcov-1),
        which holds the array of covariate values, nval_BBB long. */

     /* for covariates which are datasets,
        create covvim_BBB[jj] for the jj-th covariate (jj=0..mcov-1),
        which holds the vectim of covariate values, nval_BBB long.  */

     /* note that if covariates are used, nval_XXX == ndset_XXX */

     INFO_message("loading covariates") ;

     nbad = 0 ; /* total error count */
     if( twosam ){
       qset = (THD_3dim_dataset **)malloc(sizeof(THD_3dim_dataset *)*ndset_BBB) ;
       covvim_BBB = (MRI_vectim **)malloc(sizeof(MRI_vectim *)*mcov) ;
       covvec_BBB = (floatvec   **)malloc(sizeof(floatvec   *)*mcov) ;
       for( jj=0 ; jj < mcov ; jj++ ){                /* loop over covariates */
         covvim_BBB[jj] = NULL ;         /* initialize output vectors to NULL */
         covvec_BBB[jj] = NULL ;
         for( nkbad=kk=0 ; kk < ndset_BBB ; kk++ ){     /* loop over datasets */
           ii = string_search( labl_BBB[kk] ,     /* ii = covariate row index */
                               covnel->vec_len ,
                               (char **)covnel->vec[0] ) ;
           if( ii < 0 ){                     /* can't find it ==> this is bad */
             if( jj == 0 )
               ERROR_message("Can't find label '%s' in covariates file" ,
                             labl_BBB[kk] ) ;
               nbad++ ; nkbad++ ; continue ;
           }
           if( covnel->vec_typ[jj+1] == NI_STRING ){  /* a dataset name field */
             char **qpt = (char **)covnel->vec[jj+1] ;   /* column of strings */
             qset[kk] = THD_open_dataset(qpt[ii]) ;      /* covariate dataset */
             if( qset[kk] == NULL ){
               ERROR_message("Can't open dataset '%s' from covariates file" ,
                             qpt[ii] ) ; nbad++ ; nkbad++ ;
             } else if( DSET_NVALS(qset[kk]) > 1 ){
               ERROR_message("Dataset '%s' from covariates file has %d sub-bricks",
                             qpt[ii] , DSET_NVALS(qset[kk]) ) ; nbad++ ; nkbad++ ;
             }
           } /* end of creating dataset #kk in column #jj */
           else {                                           /* a number field */
             float *fpt = (float *)covnel->vec[jj+1] ;    /* column of floats */
             if( covvec_BBB[jj] == NULL )             /* create output vector */
               MAKE_floatvec(covvec_BBB[jj],ndset_BBB) ;
             covvec_BBB[jj]->ar[kk] = fpt[ii] ;             /* save the value */
           }
         } /* end of kk loop over BBB datasets */
         if( covnel->vec_typ[jj+1] == NI_STRING ){     /* a dataset covariate */
           if( nkbad == 0 ){  /* all dataset opens good ==> convert to vectim */
             covvim_BBB[jj] = THD_dset_list_to_vectim( ndset_BBB, qset, mask ) ;
             if( covvim_BBB[jj] == NULL ){
               ERROR_message("Can't assemble dataset vectors for covariate #%d",jj+1) ;
               nbad++ ;
             }
           }
           for( kk=0 ; kk < ndset_BBB ; kk++ )         /* tossola la trashola */
             if( qset[kk] != NULL ) DSET_delete(qset[kk]) ;
         }
       } /* end of jj loop = covariates column index */
       free(qset) ;
     } /* end of BBB covariates datasets processing */

     /* repeat for the AAA datasets */

     if( ndset_AAA > 0 ){
       qset = (THD_3dim_dataset **)malloc(sizeof(THD_3dim_dataset *)*ndset_AAA) ;
       covvim_AAA = (MRI_vectim **)malloc(sizeof(MRI_vectim *)*mcov) ;
       covvec_AAA = (floatvec   **)malloc(sizeof(floatvec   *)*mcov) ;
       for( jj=0 ; jj < mcov ; jj++ ){                /* loop over covariates */
         covvim_AAA[jj] = NULL ;         /* initialize output vectors to NULL */
         covvec_AAA[jj] = NULL ;
         for( nkbad=kk=0 ; kk < ndset_AAA ; kk++ ){     /* loop over datasets */
           ii = string_search( labl_AAA[kk] ,     /* ii = covariate row index */
                               covnel->vec_len ,
                               (char **)covnel->vec[0] ) ;
           if( ii < 0 ){                     /* can't find it ==> this is bad */
             if( jj == 0 )
               ERROR_message("Can't find label '%s' in covariates file" ,
                             labl_AAA[kk] ) ;
               nbad++ ; nkbad++ ; continue ;
           }
           if( covnel->vec_typ[jj+1] == NI_STRING ){  /* a dataset name field */
             char **qpt = (char **)covnel->vec[jj+1] ;   /* column of strings */
             qset[kk] = THD_open_dataset(qpt[ii]) ;      /* covariate dataset */
             if( qset[kk] == NULL ){
               ERROR_message("Can't open dataset '%s' from covariates file" ,
                             qpt[ii] ) ; nbad++ ; nkbad++ ;
             } else if( DSET_NVALS(qset[kk]) > 1 ){
               ERROR_message("Dataset '%s' from covariates file has %d sub-bricks",
                             qpt[ii] , DSET_NVALS(qset[kk]) ) ; nbad++ ; nkbad++ ;
             }
           } /* end of creating dataset #kk in column #jj */
           else {                                           /* a number field */
             float *fpt = (float *)covnel->vec[jj+1] ;    /* column of floats */
             if( covvec_AAA[jj] == NULL )             /* create output vector */
               MAKE_floatvec(covvec_AAA[jj],ndset_AAA) ;
             covvec_AAA[jj]->ar[kk] = fpt[ii] ;             /* save the value */
           }
         } /* end of kk loop over AAA datasets */
         if( covnel->vec_typ[jj+1] == NI_STRING ){      /* a dataset covariate */
           if( nkbad == 0 ){   /* all dataset opens good ==> convert to vectim */
             covvim_AAA[jj] = THD_dset_list_to_vectim( ndset_AAA, qset, mask ) ;
             if( covvim_AAA[jj] == NULL ){
               ERROR_message("Can't assemble dataset vectors for covariate #%d",jj+1) ;
               nbad++ ;
             }
           }
           for( kk=0 ; kk < ndset_AAA ; kk++ )       /* toss out the trashola */
             if( qset[kk] != NULL ) DSET_delete(qset[kk]) ;
         }
       } /* end of jj loop = covariates column index */
       free(qset) ;
     } /* end of AAA covariates datasets processing */

     /* Alas Babylon! */

     if( nbad > 0 ) ERROR_exit("Cannot continue past above ERROR%s :-(",
                                (nbad==1) ? "\0" : "s" ) ;

     /*--- end of loading covariate vectors ---*/

     /*--- next, setup the regression matrices ---*/

#undef  AXX
#define AXX(i,j) Axx[(i)+(j)*(nval_AAA)]    /* i=0..nval_AAA-1 , j=0..mcov */
#undef  BXX
#define BXX(i,j) Bxx[(i)+(j)*(nval_BBB)]    /* i=0..nval_BBB-1 , j=0..mcov */

     /*-- setA matrix --*/

     Axxim = mri_new( nval_AAA , mcov+1 , MRI_float ) ;
     Axx   = MRI_FLOAT_PTR(Axxim) ;
     for( kk=0 ; kk < nval_AAA ; kk++ ) AXX(kk,0) = 1.0f ; /* the mean */
     for( jj=1 ; jj <= mcov ; jj++ ){
       if( covvec_AAA[jj-1] != NULL ){  /* this column is a fixed value */
         for( sum=0.0f,kk=0 ; kk < nval_AAA ; kk++ ){
           AXX(kk,jj) = covvec_AAA[jj-1]->ar[kk] ; sum += AXX(kk,jj) ;
         }
         sum /= nval_AAA ;  /* remove mean */
         for( kk=0 ; kk < nval_AAA ; kk++ ) AXX(kk,jj) -= sum ;
       }
     }
     if( num_covset_col == 0 ){  /* process the A matrix now */
       MRI_IMARR *impr ; float sum ;
       /* Compute inv[X'X] and the pseudo-inverse inv[X'X]X' for this matrix */
       impr = mri_matrix_psinv_pair( Axxim , 0.0f ) ;
       if( impr == NULL ) ERROR_exit("Can't process setA covariate matrix?! :-(") ;
       Axx_psinv  = MRI_FLOAT_PTR(IMARR_SUBIM(impr,0)) ;
       Axx_xtxinv = MRI_FLOAT_PTR(IMARR_SUBIM(impr,1)) ;
#if 0
INFO_message("Axx:") ; mri_write_1D("-",Axxim) ;
INFO_message("Axx_psinv:") ; mri_write_1D("-",IMARR_SUBIM(impr,0)) ;
INFO_message("Axx_xtxinv:") ; mri_write_1D("-",IMARR_SUBIM(impr,1)) ;
#endif
     }

     /*-- setB matrix ---*/

     if( twosam && ttest_opcode != 2 ){  /* un-paired case */
       Bxxim = mri_new( nval_BBB , mcov+1 , MRI_float ) ;
       Bxx   = MRI_FLOAT_PTR(Bxxim) ;
       for( kk=0 ; kk < nval_BBB ; kk++ ) BXX(kk,0) = 1.0f ; /* the mean */
       for( jj=1 ; jj <= mcov ; jj++ ){
         if( covvec_BBB[jj-1] != NULL ){  /* this column is a fixed value */
           for( sum=0.0f,kk=0 ; kk < nval_BBB ; kk++ ){
             BXX(kk,jj) = covvec_BBB[jj-1]->ar[kk] ; sum += BXX(kk,jj) ;
           }
           sum /= nval_BBB ;  /* remove mean */
           for( kk=0 ; kk < nval_BBB ; kk++ ) BXX(kk,jj) -= sum ;
         }
       }
       if( num_covset_col == 0 ){  /* process the B matrix now */
         MRI_IMARR *impr ; float sum ;
         /* Compute inv[X'X] and the pseudo-inverse inv[X'X]X' for this matrix */
         impr = mri_matrix_psinv_pair( Bxxim , 0.0f ) ;
         if( impr == NULL ) ERROR_exit("Can't process setB covariate matrix?! :-(") ;
         Bxx_psinv  = MRI_FLOAT_PTR(IMARR_SUBIM(impr,0)) ;
         Bxx_xtxinv = MRI_FLOAT_PTR(IMARR_SUBIM(impr,1)) ;
       }

     } else if( twosam && ttest_opcode == 2 ){  /* paired case */

       Bxx = Axx ; Bxx_psinv = Axx_psinv ; Bxx_xtxinv = Axx_xtxinv ;

     }

   }  /*-- end of covariates setup --*/

   /*---------- create empty output dataset -----------*/

   nvout  = ((twosam) ? 6 : 2) * (mcov+1) ;  /* number of output volumes */

   outset = EDIT_empty_copy( dset_AAA[0] ) ;

   EDIT_dset_items( outset ,
                      ADN_prefix    , prefix ,
                      ADN_nvals     , nvout  ,
                      ADN_ntt       , 0      ,
                      ADN_brick_fac , NULL   ,
                      ADN_type      , HEAD_FUNC_TYPE ,
                      ADN_func_type , FUNC_BUCK_TYPE ,
                    ADN_none ) ;

   if( THD_deathcon() && THD_is_file(DSET_HEADNAME(outset)) )
       ERROR_exit("Output dataset %s already exists!",
                  DSET_HEADNAME(outset) ) ;

   tross_Make_History( "3dttest_new" , argc,argv , outset ) ;

   /* make up some brick labels [[[man, this is tediously boring work]]] */

   if( mcov > 0 ){
     workspace = (float *)malloc(sizeof(float)*(2*mcov+nval_AAA+nval_BBB+32)) ;
     if( twosam ){
       testAB = testA = testB = (unsigned int)(-1) ;
     } else {
       testAB = testB = 0 ; testA = (unsigned int)(-1) ;
     }
   }
   dof_A = nval_AAA - (mcov+1) ;
   if( twosam ){
    dof_B  = nval_BBB - (mcov+1) ;
    dof_AB = (ttest_opcode==2) ? dof_A : dof_A+dof_B ;
   }

   stnam = (toz) ? "Zscr" : "Tstat" ;
   if( mcov <= 0 ){                    /*--- no covariates ---*/
     if( !twosam ){
       sprintf(blab,"%s_mean",snam_AAA);       EDIT_BRICK_LABEL(outset,0,blab);
       sprintf(blab,"%s_%s"  ,snam_AAA,stnam); EDIT_BRICK_LABEL(outset,1,blab);
          if( toz ) EDIT_BRICK_TO_FIZT(outset,1) ;
          else      EDIT_BRICK_TO_FITT(outset,1,dof_A) ;
     } else {
       sprintf(blab,"%s-%s_mean",snam_AAA,snam_BBB)      ; EDIT_BRICK_LABEL(outset,0,blab);
       sprintf(blab,"%s-%s_%s"  ,snam_AAA,snam_BBB,stnam); EDIT_BRICK_LABEL(outset,1,blab);
          if( toz ) EDIT_BRICK_TO_FIZT(outset,1) ;
          else      EDIT_BRICK_TO_FITT(outset,1,dof_AB) ;
       sprintf(blab,"%s_mean",snam_AAA)                  ; EDIT_BRICK_LABEL(outset,2,blab);
       sprintf(blab,"%s_%s"  ,snam_AAA,stnam)            ; EDIT_BRICK_LABEL(outset,3,blab);
          if( toz ) EDIT_BRICK_TO_FIZT(outset,3) ;
          else      EDIT_BRICK_TO_FITT(outset,3,dof_A) ;
       sprintf(blab,"%s_mean",snam_BBB)                  ; EDIT_BRICK_LABEL(outset,4,blab);
       sprintf(blab,"%s_%s"  ,snam_BBB,stnam)            ; EDIT_BRICK_LABEL(outset,5,blab);
          if( toz ) EDIT_BRICK_TO_FIZT(outset,5) ;
          else      EDIT_BRICK_TO_FITT(outset,5,dof_B) ;
     }
   } else {                            /*--- have covariates ---*/
     kk = 0 ;
     if( testAB ){
       sprintf(blab,"%s-%s_mean",snam_AAA,snam_BBB);
         EDIT_BRICK_LABEL(outset,kk,blab); kk++;
       sprintf(blab,"%s-%s_%s"  ,snam_AAA,snam_BBB,stnam);
         EDIT_BRICK_LABEL(outset,kk,blab);
         if( toz ) EDIT_BRICK_TO_FIZT(outset,kk) ;
         else      EDIT_BRICK_TO_FITT(outset,kk,dof_AB) ;
         kk++;
       for( jj=1 ; jj <= mcov ; jj++ ){
         sprintf(blab,"%s-%s_%s",snam_AAA,snam_BBB,covlab->str[jj]) ;
           EDIT_BRICK_LABEL(outset,kk,blab); kk++;
         sprintf(blab,"%s-%s_%s_%s",snam_AAA,snam_BBB,covlab->str[jj],stnam) ;
           EDIT_BRICK_LABEL(outset,kk,blab);
         if( toz ) EDIT_BRICK_TO_FIZT(outset,kk) ;
         else      EDIT_BRICK_TO_FITT(outset,kk,dof_AB) ;
         kk++;
       }
     }
     if( testA ){
       sprintf(blab,"%s_mean",snam_AAA) ;
         EDIT_BRICK_LABEL(outset,kk,blab); kk++;
       sprintf(blab,"%s_%s",snam_AAA,stnam) ;
         EDIT_BRICK_LABEL(outset,kk,blab);
         if( toz ) EDIT_BRICK_TO_FIZT(outset,kk) ;
         else      EDIT_BRICK_TO_FITT(outset,kk,dof_A) ;
         kk++;
       for( jj=1 ; jj <= mcov ; jj++ ){
         sprintf(blab,"%s_%s",snam_AAA,covlab->str[jj]) ;
           EDIT_BRICK_LABEL(outset,kk,blab); kk++;
         sprintf(blab,"%s_%s_%s",snam_AAA,covlab->str[jj],stnam) ;
           EDIT_BRICK_LABEL(outset,kk,blab);
           if( toz ) EDIT_BRICK_TO_FIZT(outset,kk) ;
           else      EDIT_BRICK_TO_FITT(outset,kk,dof_A) ;
           kk++;
       }
     }
     if( testB ){
       sprintf(blab,"%s_mean",snam_BBB) ;
         EDIT_BRICK_LABEL(outset,kk,blab); kk++;
       sprintf(blab,"%s_%s",snam_BBB,stnam) ;
         EDIT_BRICK_LABEL(outset,kk,blab);
         if( toz ) EDIT_BRICK_TO_FIZT(outset,kk) ;
         else      EDIT_BRICK_TO_FITT(outset,kk,dof_B) ;
         kk++;
       for( jj=1 ; jj <= mcov ; jj++ ){
         sprintf(blab,"%s_%s",snam_BBB,covlab->str[jj]) ;
           EDIT_BRICK_LABEL(outset,kk,blab); kk++;
         sprintf(blab,"%s_%s_%s",snam_BBB,covlab->str[jj],stnam) ;
           EDIT_BRICK_LABEL(outset,kk,blab);
           if( toz ) EDIT_BRICK_TO_FIZT(outset,kk) ;
           else      EDIT_BRICK_TO_FITT(outset,kk,dof_B) ;
           kk++;
       }
     }
   }

   /*----- create space to store results before dataset-izing them -----*/

   MAKE_VECTIM(vimout,nmask_hits,nvout) ; vimout->ignore = 0 ;

   /**********==========---------- process data ----------==========**********/

   vstep = (nmask_hits > 666) ? nmask_hits/50 : 0 ;
   if( vstep > 0 ) fprintf(stderr,"++ t-testing:") ;

   for( kout=ivox=0 ; ivox < nvox ; ivox++ ){

     if( mask != NULL && mask[ivox] == 0 ) continue ;  /* don't process */

     vimout->ivec[kout] = ivox ;  /* table of what voxels are in vimout */

     if( vstep > 0 && kout%vstep==vstep/2 ) vstep_print() ;

                  datAAA = VECTIM_PTR(vectim_AAA,kout) ;  /* data arrays */
     if( twosam ) datBBB = VECTIM_PTR(vectim_BBB,kout) ;

     resar = VECTIM_PTR(vimout,kout) ;                    /* results array */

     if( mcov == 0 ){  /*--- no covariates ==> standard t-tests ---*/

       if( twosam ){
         tpair = ttest_toz( nval_AAA,datAAA , nval_BBB,datBBB , ttest_opcode ) ;
         resar[0] = tpair.a ; resar[1] = tpair.b ;
         tpair = ttest_toz( nval_AAA,datAAA , 0 ,NULL   , ttest_opcode ) ;
         resar[2] = tpair.a ; resar[3] = tpair.b ;
         tpair = ttest_toz( nval_BBB,datBBB , 0 ,NULL   , ttest_opcode ) ;
         resar[4] = tpair.a ; resar[5] = tpair.b ;
       } else {
         tpair = ttest_toz( nval_AAA,datAAA , 0 ,NULL   , ttest_opcode ) ;
         resar[0] = tpair.a ; resar[1] = tpair.b ;
       }

     } else {          /*--- covariates ==> regression analysis ---*/

       /*-- if covariate datasets are being used,
            must fill in the Axx and Bxx matrices now --*/

       if( num_covset_col > 0 ){
         float *fpt , sum ;
         for( jj=1 ; jj <= mcov ; jj++ ){
           if( covvim_AAA[jj-1] != NULL ){
             fpt = VECTIM_PTR(covvim_AAA[jj-1],kout) ;
             for( sum=0.0f,kk=0 ; kk < nval_AAA ; kk++ ){
               AXX(kk,jj) = fpt[kk] ; sum += AXX(kk,jj) ;
             }
             sum /= nval_AAA ;  /* remove mean */
             for( kk=0 ; kk < nval_AAA ; kk++ ) AXX(kk,jj) -= sum ;
           }
           if( twosam && ttest_opcode != 2 && covvim_BBB[jj-1] != NULL ){
             fpt = VECTIM_PTR(covvim_BBB[jj-1],kout) ;
             for( sum=0.0f,kk=0 ; kk < nval_BBB ; kk++ ){
               BXX(kk,jj) = fpt[kk] ; sum += BXX(kk,jj) ;
             }
             sum /= nval_BBB ;  /* remove mean */
             for( kk=0 ; kk < nval_BBB ; kk++ ) BXX(kk,jj) -= sum ;
           }
         }
       }

       /*-- and do the work --*/

       regress_toz( nval_AAA , datAAA , nval_BBB , datBBB , ttest_opcode ,
                    mcov ,
                    Axx , Axx_psinv , Axx_xtxinv ,
                    Bxx , Bxx_psinv , Bxx_xtxinv , resar , workspace ) ;

     }

     kout++ ;
   }  /* end of loop over voxels */

   if( vstep > 0 ) fprintf(stderr,"!\n") ;

   /*-------- get rid of the input data now --------*/

   if( workspace != NULL ) free(workspace) ;

                            VECTIM_destroy(vectim_AAA) ;
   if( vectim_BBB != NULL ) VECTIM_destroy(vectim_BBB) ;

   if( covvim_AAA != NULL ){
     for( jj=0 ; jj < mcov ; jj++ )
       if( covvim_AAA[jj] != NULL ) VECTIM_destroy(covvim_AAA[jj]) ;
     free(covvim_AAA) ;
   }
   if( covvec_AAA != NULL ){
     for( jj=0 ; jj < mcov ; jj++ )
       if( covvec_AAA[jj] != NULL ) KILL_floatvec(covvec_AAA[jj]) ;
     free(covvec_AAA) ;
   }

   if( covvim_BBB != NULL ){
     for( jj=0 ; jj < mcov ; jj++ )
       if( covvim_BBB[jj] != NULL ) VECTIM_destroy(covvim_BBB[jj]) ;
     free(covvim_BBB) ;
   }
   if( covvec_BBB != NULL ){
     for( jj=0 ; jj < mcov ; jj++ )
       if( covvec_BBB[jj] != NULL ) KILL_floatvec(covvec_BBB[jj]) ;
     free(covvec_BBB) ;
   }

   /*---------- fill in the output dataset with some numbers ----------*/

   INFO_message("saving results") ;

   for( kk=0 ; kk < nvout ; kk++ )
     EDIT_substitute_brick( outset , kk , MRI_float , NULL ) ;

   THD_vectim_to_dset( vimout , outset ) ;
   VECTIM_destroy( vimout ) ;

   mri_fdr_setmask(mask) ;
   kk = THD_create_all_fdrcurves(outset) ;
   if( kk > 0 )
     INFO_message("Added %d FDR curve%s to dataset",kk,(kk==1)?"\0":"s");
   else
     WARNING_message("Failed to add FDR curves to dataset?!") ;

   DSET_write(outset) ; WROTE_DSET(outset) ; exit(0) ;

} /* end of main program */

/*---------------------------------------------------------------------------*/

#undef  PA
#undef  PB
#undef  XA
#undef  XB
#define PA(i,j) psinvA[(i)+(j)*mm]  /* i=0..mm-1 , j=0..numA-1 */
#define PB(i,j) psinvB[(i)+(j)*mm]
#define XA(i,j) xA[(i)+(j)*(nA)]    /* i=0..nA-1 , j=0..mm-1 */
#define XB(i,j) xB[(i)+(j)*(nB)]

#undef  xtxA
#undef  xtxB
#define xtxA(i) xtxinvA[(i)+(i)*mm] /* diagonal elements */
#define xtxB(i) xtxinvB[(i)+(i)*mm]

#undef  VBIG
#define VBIG 1.0e+24f

#undef  TMAX
#define TMAX 19.0f

/*---------------------------------------------------------------------------*/
/*  opcode defines what to do for 2-sample tests:
      0 ==> unpaired, pooled variance
      1 ==> unpaired, unpooled variance (not implemented here)
      2 ==> paired (numA==numB required)

    xA      = numA X (mcov+1) matrix -- in column-major order
    psinvA  = (mcov+1) X numA matrix -- in column-major order
    xtxinvA = (mcov+1) X (mcov+1) matrix = inv[xA'xA]
*//*-------------------------------------------------------------------------*/

void regress_toz( int numA , float *zA ,
                  int numB , float *zB , int opcode ,
                  int mcov ,
                  float *xA , float *psinvA , float *xtxinvA ,
                  float *xB , float *psinvB , float *xtxinvB ,
                  float *outvec , float *workspace             )
{
   int kt=0,nws , mm=mcov+1 , nA=numA , nB=numB ;
   float *betA=NULL , *betB=NULL , *zdifA=NULL , *zdifB=NULL ;
   float ssqA=0.0f , ssqB=0.0f , varA=0.0f , varB=0.0f ; double dof=0.0 ;
   register float val ; register int ii,jj,tt ;

   nws = 0 ;
   if( testA || testAB ){
     betA  = workspace + nws ; nws += mm ;
     zdifA = workspace + nws ; nws += nA ;
   }
   if( testB || testAB ){
     betB  = workspace + nws ; nws += mm ;
     zdifB = workspace + nws ; nws += nB ;
   }

   /*-- compute estimates for A parameters --*/

   if( testA || testAB ){
     MRI_IMAGE *axxim_psinv=NULL , *axxim_xtxinv=NULL ;
     if( psinvA == NULL || xtxinvA == NULL ){  /* matrix wasn't pre-inverted */
       MRI_IMARR *impr ; MRI_IMAGE *axxim ;
       axxim = mri_new_vol_empty( nA , mcov+1 , 1 , MRI_float ) ;
       mri_fix_data_pointer(xA,axxim) ;
       impr = mri_matrix_psinv_pair(axxim,0.0f) ;
       mri_clear_data_pointer(axxim) ; mri_free(axxim) ;
       if( impr == NULL ){ ERROR_message("psinv setA matrix fails"); return; }
       axxim_psinv  = IMARR_SUBIM(impr,0); psinvA  = MRI_FLOAT_PTR(axxim_psinv );
       axxim_xtxinv = IMARR_SUBIM(impr,1); xtxinvA = MRI_FLOAT_PTR(axxim_xtxinv);
       FREE_IMARR(impr) ;
     }
     for( ii=0 ; ii < mm ; ii++ ){
       for( val=0.0f,jj=0 ; jj < nA ; jj++ ) val += PA(ii,jj)*zA[jj] ;
       betA[ii] = val ;
     }
     for( jj=0 ; jj < nA ; jj++ ){
       val = -zA[jj] ;
       for( ii=0 ; ii < mm ; ii++ ) val += XA(jj,ii)*betA[ii] ;
       zdifA[ii] = val ; ssqA += val*val ;
     }
     if( testA ){ varA = ssqA / (nA-mm) ; if( varA <= 0.0f ) varA = VBIG ; }
     mri_free(axxim_psinv) ; mri_free(axxim_xtxinv) ; /* if they're not NULL */
   }

   /*-- compute estimates for B parameters --*/

   if( testB || testAB ){
     MRI_IMAGE *bxxim_psinv=NULL , *bxxim_xtxinv=NULL ;
     if( psinvB == NULL || xtxinvB == NULL ){  /* matrix wasn't pre-inverted */
       MRI_IMARR *impr ; MRI_IMAGE *bxxim ;
       bxxim = mri_new_vol_empty( nB , mcov+1 , 1 , MRI_float ) ;
       mri_fix_data_pointer(xB,bxxim) ;
       impr = mri_matrix_psinv_pair(bxxim,0.0f) ;
       mri_clear_data_pointer(bxxim) ; mri_free(bxxim) ;
       if( impr == NULL ){ ERROR_message("psinv setB matrix fails"); return; }
       bxxim_psinv  = IMARR_SUBIM(impr,0); psinvB  = MRI_FLOAT_PTR(bxxim_psinv );
       bxxim_xtxinv = IMARR_SUBIM(impr,1); xtxinvB = MRI_FLOAT_PTR(bxxim_xtxinv);
       FREE_IMARR(impr) ;
     }
     for( ii=0 ; ii < mm ; ii++ ){
       for( val=0.0f,jj=0 ; jj < nB ; jj++ ) val += PB(ii,jj)*zB[jj] ;
       betB[ii] = val ;
     }
     for( jj=0 ; jj < nB ; jj++ ){
       val = -zB[jj] ;
       for( ii=0 ; ii < mm ; ii++ ) val += XB(jj,ii)*betB[ii] ;
       zdifB[ii] = val ; ssqB += val*val ;
     }
     if( testB ){ varB = ssqB / (nB-mm) ; if( varB <= 0.0f ) varB = VBIG ; }
     mri_free(bxxim_psinv) ; mri_free(bxxim_xtxinv) ;
   }

   /*-- carry out 2-sample (A-B) tests, if any --*/

   if( testAB ){
     float varAB ;

     if( opcode == 2 ){  /* paired (nA==nB, xA==xB, etc.) */

       for( varAB=0.0f,ii=0 ; ii < nA ; ii++ ){
         val = zdifA[ii] - zdifB[ii] ; varAB += val*val ;
       }
       varAB /= (nA-mm) ; if( varAB <= 0.0f ) varAB = VBIG ;

       dof = nA - mm ;
       for( tt=0 ; tt < mm ; tt++ ){
         if( (testAB & (1 << tt)) == 0 ) continue ;  /* bitwase AND */
         outvec[kt++] = betA[tt] - betB[tt] ;
         val          = outvec[kt-1] / sqrtf( varAB*xtxA(tt) ) ;
         outvec[kt++] = (toz) ? (float)GIC_student_t2z( (double)val , dof )
                              : val ;
       }

     } else {            /* unpaired, pooled variance */

       varAB = (ssqA+ssqB)/(nA+nB-2*mm) ; if( varAB <= 0.0f ) varAB = VBIG ;

       dof = nA + nB - 2*mm ;
       for( tt=0 ; tt < mm ; tt++ ){
         if( (testAB & (1 << tt)) == 0 ) continue ;  /* bitwase AND */
         outvec[kt++] = betA[tt] - betB[tt] ;
         val          = outvec[kt-1] / sqrtf( varAB*(xtxA(tt)+xtxB(tt)) );
         outvec[kt++] = (toz) ? (float)GIC_student_t2z( (double)val , dof )
                              : val ;
       }
     } /* end of unpaired pooled variance */
   }

   /*-- carry out 1-sample A tests, if any --*/

   if( testA ){
     dof = nA - mm ;
     for( tt=0 ; tt < mm ; tt++ ){
       if( (testA & (1 << tt)) == 0 ) continue ;  /* bitwise AND */
       outvec[kt++] = betA[tt] ;
       val          = betA[tt] / sqrtf( varA * xtxA(tt) ) ;
       outvec[kt++] = (toz) ? (float)GIC_student_t2z( (double)val , dof )
                            : val ;
     }
   }

   /*-- carry out 1-sample B tests, if any --*/

   if( testB ){
     dof = nB - mm ;
     for( tt=0 ; tt < mm ; tt++ ){
       if( (testB & (1 << tt)) == 0 ) continue ;  /* bitwise AND */
       outvec[kt++] = betB[tt] ;
       val          = betB[tt] / sqrtf( varB * xtxB(tt) ) ;
       outvec[kt++] = (toz) ? (float)GIC_student_t2z( (double)val , dof )
                            : val ;
     }
   }

   return ;
}

/*----------------------------------------------------------------------------*/
/*! Various sorts of t-tests; output = Z-score.
   - numx = number of points in the first sample (must be > 1)
   - xar  = array with first sample
   - numy = number of points in the second sample
             - numy = 0 ==> a 1 sample test of first sample against mean=0
  DISABLED   - numy = 1 ==> a 1 sample test of first sample against mean=yar[0]
             - numy > 1 ==> a 2 sample test; opcode determines what kind
   - opcode = 0 for unpaired test with pooled variance
   - opcode = 1 for unpaired test with unpooled variance
   - opcode = 2 for paired test (numx == numy is required)
   - The return value is the Z-score of the t-statistic.
*//*--------------------------------------------------------------------------*/

float_pair ttest_toz( int numx, float *xar, int numy, float *yar, int opcode )
{
   float_pair result = {0.0f,0.0f} ;
   register int ii ; register float val ;
   float avx,sdx , avy,sdy , dof , tstat=0.0f,delta=0.0f ;
   int paired=(opcode==2) , pooled=(opcode==0) ;

#if 0
   /* check inputs for stoopidities or other things that need to be changed */

   if( numx < 2 || xar == NULL                 ) return result ; /* bad */
   if( paired && (numy != numx || yar == NULL) ) return result ; /* bad */
#endif

   if( numy < 2 || yar == NULL ){ numy = paired = pooled = 0 ; yar = NULL ; }

   if( paired ){   /* Case 1: paired t test */

     avx = 0.0f ;
     for( ii=0 ; ii < numx ; ii++ ) avx += xar[ii]-yar[ii] ;
     avx /= numx ; sdx = 0.0f ;
     for( ii=0 ; ii < numx ; ii++ ){ val = xar[ii]-yar[ii]-avx; sdx += val*val; }
     if( sdx > 0.0f )      tstat = avx / sqrtf( sdx/((numx-1.0f)*numx) ) ;
     else if( avx > 0.0f ) tstat =  19.0f ;
     else if( avx < 0.0f ) tstat = -19.0f ;
     else                  tstat =   0.0f ;
     dof = numx-1.0f ; delta = avx ;  /* delta = diff in means */

   } else if( numy == 0 ){  /* Case 2: 1 sample test against mean==0 */

     avx = 0.0f ;
     for( ii=0 ; ii < numx ; ii++ ) avx += xar[ii] ;
     avx /= numx ; sdx = 0.0f ;
     for( ii=0 ; ii < numx ; ii++ ){ val = xar[ii]-avx ; sdx += val*val ; }
     if( sdx > 0.0f )      tstat = avx / sqrtf( sdx/((numx-1.0f)*numx) ) ;
     else if( avx > 0.0f ) tstat =  19.0f ;
     else if( avx < 0.0f ) tstat = -19.0f ;
     else                  tstat =   0.0f ;
     dof = numx-1.0f ; delta = avx ; /* delta = mean */

   } else {  /* Case 3: 2 sample test (pooled or unpooled) */

     avx = 0.0f ;
     for( ii=0 ; ii < numx ; ii++ ) avx += xar[ii] ;
     avx /= numx ; sdx = 0.0f ;
     for( ii=0 ; ii < numx ; ii++ ){ val = xar[ii] - avx ; sdx += val*val ; }

     avy = 0.0f ;
     for( ii=0 ; ii < numy ; ii++ ) avy += yar[ii] ;
     avy /= numy ; sdy = 0.0f ;
     for( ii=0 ; ii < numy ; ii++ ){ val = yar[ii] - avy ; sdy += val*val ; }

     delta = avx - avy ; /* difference in means */

     if( sdx+sdy == 0.0f ){

            if( delta > 0.0f ) tstat =  19.0f ;
       else if( delta < 0.0f ) tstat = -19.0f ;
       else                    tstat =   0.0f ;
       dof = numx+numy-2.0f ;

     } else if( pooled ){  /* Case 3a: pooled variance estimate */

       sdx   = (sdx+sdy) / (numx+numy-2.0f) ;
       tstat = delta / sqrtf( sdx*(1.0f/numx+1.0f/numy) ) ;
       dof   = numx+numy-2.0f ;

     } else {       /* Case 3b: unpooled variance estimate */

       sdx  /= (numx-1.0f)*numx ; sdy /= (numy-1.0f)*numy ; val = sdx+sdy ;
       tstat = delta / sqrtf(val) ;
       dof   = (val*val) / (sdx*sdx/(numx-1.0f) + sdy*sdy/(numy-1.0f) ) ;

     }

   } /* end of all possible cases */

   result.a = delta ;
   result.b = (toz) ? (float)GIC_student_t2z( (double)tstat , (double)dof )
                    : tstat ;
   return result ;
}

/*=======================================================================*/
/** The following routines are for the t-to-z conversion, and are
    adapted from mri_stats.c to be parallelizable (no static data).
=========================================================================*/

static double GIC_qginv( double p )
{
   double dp , dx , dt , ddq , dq ;
   int    newt ;                       /* not Gingrich, but Isaac */

   dp = (p <= 0.5) ? (p) : (1.0-p) ;   /* make between 0 and 0.5 */

   if( dp <= 1.e-37 ){
      dx = 13.0 ;                      /* 13 sigma has p < 10**(-38) */
      return ( (p <= 0.5) ? (dx) : (-dx) ) ;
   }

/**  Step 1:  use 26.2.23 from Abramowitz and Stegun **/

   dt = sqrt( -2.0 * log(dp) ) ;
   dx = dt
        - ((.010328*dt + .802853)*dt + 2.515517)
        /(((.001308*dt + .189269)*dt + 1.432788)*dt + 1.) ;

/**  Step 2:  do 3 Newton steps to improve this
              (uses the math library erfc function) **/

   for( newt=0 ; newt < 3 ; newt++ ){
     dq  = 0.5 * erfc( dx / 1.414213562373095 ) - dp ;
     ddq = exp( -0.5 * dx * dx ) / 2.506628274631000 ;
     dx  = dx + dq / ddq ;
   }

   if( dx > 13.0 ) dx = 13.0 ;
   return ( (p <= 0.5) ? (dx) : (-dx) ) ;  /* return with correct sign */
}

#ifdef NO_GAMMA
/*-----------------------------------------------------------------------*/
/* If the system doesn't provide lgamma() for some primitive reason.
-------------------------------------------------------------------------*/

/**----- log of gamma, for argument between 1 and 2 -----**/

static double gamma_12( double y )
{
   double x , g ;
   x = y - 1.0 ;
   g = ((((((( 0.035868343 * x - 0.193527818 ) * x
                               + 0.482199394 ) * x
                               - 0.756704078 ) * x
                               + 0.918206857 ) * x
                               - 0.897056937 ) * x
                               + 0.988205891 ) * x
                               - 0.577191652 ) * x + 1.0 ;
   return log(g) ;
}

/**----- asymptotic expansion of ln(gamma(x)) for large positive x -----**/

#define LNSQRT2PI 0.918938533204672  /* ln(sqrt(2*PI)) */

static double gamma_asympt(double x)
{
   double sum ;

   sum = (x-0.5)*log(x) - x + LNSQRT2PI + 1.0/(12.0*x) - 1./(360.0*x*x*x) ;
   return sum ;
}

/**----- log of gamma, argument positive (not very efficient!) -----**/

static double GIC_lgamma( double x )
{
   double w , g ;

   if( x <= 0.0 ) return 0.0 ;  /* should not happen */

   if( x <  1.0 ) return gamma_12( x+1.0 ) - log(x) ;
   if( x <= 2.0 ) return gamma_12( x ) ;
   if( x >= 6.0 ) return gamma_asympt(x) ;

   g = 0 ; w = x ;
   while( w > 2.0 ){ w -= 1.0 ; g += log(w) ; }
   return ( gamma_12(w) + g ) ;
}

#define lgamma GIC_lgamma

#endif  /*----- NO_GAMMA ------------------------------------------------*/

/*----------------------------------------------------------------------*/

static double GIC_lnbeta( double p , double q )
{
   return (lgamma(p) + lgamma(q) - lgamma(p+q)) ;
}

/*----------------------------------------------------------------------*/

#define ZERO 0.0
#define ONE  1.0
#define ACU  1.0e-15

static double GIC_incbeta( double x , double p , double q , double beta )
{
   double betain , psq , cx , xx,pp,qq , term,ai , temp , rx ;
   int indx , ns ;

   if( p <= ZERO || q <= ZERO ) return -1.0 ;  /* error! */

   if( x <= ZERO ) return ZERO ;
   if( x >= ONE  ) return ONE ;

   /**  change tail if necessary and determine s **/

   psq = p+q ;
   cx  = ONE-x ;
   if(  p < psq*x ){
      xx   = cx ; cx   = x ; pp   = q ; qq   = p ; indx = 1 ;
   } else {
      xx   = x ; pp   = p ; qq   = q ; indx = 0 ;
   }

   term   = ONE ;
   ai     = ONE ;
   betain = ONE ;
   ns     = qq + cx*psq ;

   /** use soper's reduction formulae **/

      rx = xx/cx ;

lab3:
      temp = qq-ai ;
      if(ns == 0) rx = xx ;

lab4:
      term   = term*temp*rx/(pp+ai) ;
      betain = betain+term ;
      temp   = fabs(term) ;
      if(temp <= ACU && temp <= ACU*betain) goto lab5 ;

      ai = ai+ONE ;
      ns = ns-1 ;
      if(ns >= 0) goto lab3 ;
      temp = psq ;
      psq  = psq+ONE ;
      goto lab4 ;

lab5:
      betain = betain*exp(pp*log(xx)+(qq-ONE)*log(cx)-beta)/pp ;
      if(indx) betain=ONE-betain ;

   return betain ;
}

/*----------------------------------------------------------------------*/

#undef  ZMAX
#define ZMAX 13.0

double GIC_student_t2z( double tt , double dof )
{
   double xx , pp , bb ;

   bb = GIC_lnbeta( 0.5*dof , 0.5 ) ;

   xx = dof/(dof + tt*tt) ;
   pp = GIC_incbeta( xx , 0.5*dof , 0.5 , bb ) ;

   if( tt > 0.0 ) pp = 1.0 - 0.5 * pp ;
   else           pp = 0.5 * pp ;

   xx = - GIC_qginv(pp) ;
   if( xx > ZMAX ) xx = ZMAX ; else if( xx < -ZMAX ) xx = -ZMAX ;
   return xx ;
}
