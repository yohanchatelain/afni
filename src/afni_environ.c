/*****************************************************************************
   Major portions of this software are copyrighted by the Medical College
   of Wisconsin, 1994-2000, and are released under the Gnu General Public
   License, Version 2.  See the file README.Copyright for details.
******************************************************************************/

#include "mrilib.h"

/*------------------------------------------------------------------------
   Read an entire file into a character string.  When you are
   done with the returned string, free() it.  If the string pointer
   is returned as NULL, something bad happened.
--------------------------------------------------------------------------*/

char * AFNI_suck_file( char *fname )
{
   int len , fd , ii ;
   char *buf ;

   if( fname == NULL || fname[0] == '\0' ) return NULL ;

   len = THD_filesize( fname ) ;
   if( len <= 0 ) return NULL ;

   fd = open( fname , O_RDONLY ) ;
   if( fd < 0 ) return NULL ;

   buf = (char *) malloc( sizeof(char) * (len+4) ) ;
   ii  = read( fd , buf , len ) ;
   close( fd ) ;
   if( ii <= 0 ){ free(buf) ; return NULL; }

   buf[len] = '\0' ;  /* 27 July 1998: 'len' used to be 'ii+1', which is bad */
   return buf ;
}

/*-----------------------------------------------------------------------
   Read environment section only from an AFNI setup file.
   [See also afni_setup.c]
-------------------------------------------------------------------------*/

#define ISTARRED(s) ( (s)[0]=='*' && (s)[1]=='*' && (s)[2]=='*' )

#define EOLSKIP                                                          \
  do{ for( ; fptr[0] != '\n' && fptr[0] != '\0' ; fptr++ ) ; /* nada */  \
      if( fptr[0] == '\0' ) goto done ;                                  \
      fptr++ ; } while(0)

#define GETSSS                                                            \
  do{ int nu=0,qq;                                                        \
      if( fptr-fbuf >= nbuf || fptr[0] == '\0' ) goto done ;              \
      str[0]='\0'; qq=sscanf(fptr,"%127s%n",str,&nu); nused+=nu;fptr+=nu; \
      if( str[0]=='\0' || qq==0 || nu==0 ) goto done ;                    \
    } while(0)

#define GETSTR                                                            \
  do{ GETSSS ;                                                            \
      while(str[0]=='!' || (str[0]=='/' && str[1]=='/') ||                \
            (str[0]=='#' && str[1]=='\0') ){EOLSKIP; GETSSS;}             \
    } while(0)

#define GETEQN                                      \
  do{ GETSTR ; if(ISTARRED(str)) goto SkipSection ; \
      strcpy(left,str) ;                            \
      GETSTR ; if(ISTARRED(str)) goto SkipSection ; \
      strcpy(middle,str) ;                          \
      GETSTR ; if(ISTARRED(str)) goto SkipSection ; \
      strcpy(right,str) ; } while(0)

#define NSBUF 256

static int afni_env_done = 0 ;

/*---------------------------------------------------------------------------*/

void AFNI_mark_environ_done(void){ afni_env_done = 1 ; return ; }

/*---------------------------------------------------------------------------*/

char * my_getenv( char *ename )
{
   if( !afni_env_done ){
      char *sysenv = getenv("AFNI_SYSTEM_AFNIRC") ;       /* 16 Apr 2000 */
      if( sysenv != NULL ) AFNI_process_environ(sysenv) ; /* 16 Apr 2000 */
      AFNI_process_environ(NULL) ;
   }
   return getenv( ename ) ;
}

/*---------------------------------------------------------------------------*/

void AFNI_process_environ( char *fname )
{
   int   nbuf , nused , ii ;
   char *fbuf , *fptr ;
   char str[NSBUF] , left[NSBUF] , middle[NSBUF] , right[NSBUF] ;

ENTRY("AFNI_process_environ") ;
   if( fname != NULL ){
     strcpy(str,fname) ;
   } else {
     char *home ;
     if( afni_env_done ) EXRETURN ;
     home = getenv("HOME") ;
     if( home != NULL ){ strcpy(str,home) ; strcat(str,"/.afnirc") ; }
     else              { strcpy(str,".afnirc") ; }
     if( !THD_is_file(str) ){                      /* 19 Sep 2007 */
       if( home != NULL ){ strcpy(str,home) ; strcat(str,"/AFNI.afnirc") ; }
       else              { strcpy(str,"AFNI.afnirc") ; }
     }
     afni_env_done = 1 ;
   }

   fbuf = AFNI_suck_file( str ) ; if( fbuf == NULL ) EXRETURN ;
   nbuf = strlen(fbuf) ;          if( nbuf == 0    ) EXRETURN ;

   fptr = fbuf ; nused = 0 ;

   /** scan for section strings, which start with "***" **/

   str[0] = '\0' ;  /* initialize string */

   while( nused < nbuf ){

      /**----------------------------------------**/
      /**-- skip ahead to next section keyword --**/

      SkipSection: while( ! ISTARRED(str) ){ GETSTR; }

      /*- 04 Jun 1999 -*/

      if( strcmp(str,"***END") == 0 ) break ;  /* exit main loop */

      if( strcmp(str,"***ENVIRONMENT") != 0 ){ GETSTR ; goto SkipSection ; }

      /**---------------------------------------**/
      /**-- ENVIRONMENT section [04 Jun 1999] --**/

      if( strcmp(str,"***ENVIRONMENT") == 0 ){  /* loop, looking for environment settings */
         char *enveqn ; int nl , nr ;

         while(1){                          /* loop, looking for 'name = value' */
            GETEQN ;

            if( !THD_filename_pure(left) ) continue ;

            nl = strlen(left) ; nr = strlen(right) ;
            enveqn = (char *) malloc(nl+nr+4) ;
            strcpy(enveqn,left) ; strcat(enveqn,"=") ; strcat(enveqn,right) ;
            putenv(enveqn) ;
         }

         continue ;  /* to end of outer while */
      } /* end of ENVIRONMENT */

   }  /* end of while loop */

 done:
   free(fbuf) ; EXRETURN ;
}

/*-----------------------------------------------------------------*/

int AFNI_yesenv( char *ename )     /* 21 Jun 2000 */
{
   char *ept ;
   if( ename == NULL ) return 0 ;
   ept = my_getenv(ename) ;
   return YESSISH(ept) ;
}

/*------------------------------------------------------------------*/

int AFNI_noenv( char *ename )     /* 21 Jun 2000 */
{
   char *ept ;
   if( ename == NULL ) return 0 ;
   ept = my_getenv(ename) ;
   return NOISH(ept) ;
}

/*------------------------------------------------------------------*/

double AFNI_numenv( char *ename )  /* 23 Aug 2003 */
{
   char *ept,*ccc ; double val ;
   if( ename == NULL ) return 0.0 ;
   ept = my_getenv(ename) ;
   if( ept   == NULL ) return 0.0 ;
   val = strtod(ept,&ccc) ;
        if( *ccc == 'k' || *ccc == 'K' ) val *= 1024.0 ;
   else if( *ccc == 'm' || *ccc == 'M' ) val *= 1024.0*1024.0 ;
   else if( *ccc == 'g' || *ccc == 'G' ) val *= 1024.0*1024.0*1024.0 ;
   return val ;
}

double AFNI_numenv_def( char *ename, double dd ) /* 18 Sep 2007 */
{
   char *ept,*ccc ; double val=dd ;
   if( ename == NULL ) return val ;
   ept = my_getenv(ename) ;
   if( ept   == NULL ) return val ;
   val = strtod(ept,&ccc) ; if( ccc == ept ) val = dd ;
   return val ;
}

/*------------------------------------------------------------------*/
/*! Input is "name value".  Return is 0 if OK, -1 if not OK. */

int AFNI_setenv( char *cmd )
{
   char nam[256]="\0" , val[1024]="\0" , eqn[1280] , *eee ;

   if( cmd == NULL || strlen(cmd) < 3 ) return(-1) ;

   sscanf( cmd , "%255s %1023s" , nam , val ) ;
   if( nam[0] == '\0' || val[0] == '\0' && strchr(cmd,'=') != NULL ){
     char *ccc = strdup(cmd) ;
     eee = strchr(ccc,'=') ; *eee = ' ' ;
     sscanf( ccc , "%255s %1023s" , nam , val ) ;
     free((void *)ccc) ;
   }
   if( nam[0] == '\0' || val[0] == '\0' ) return(-1) ;

   sprintf(eqn,"%s=%s",nam,val) ;
   eee = strdup(eqn) ; putenv(eee) ;
   return(0) ;
}

/*-------------------------------------------------------------------------*/

int AFNI_prefilter_args( int *argc , char **argv )
{
   int narg=*argc , ii,jj , nused , ttt ;
   char *used , *eee ;

   if( narg <= 1 || argv == NULL ) return(0) ;

   used = (char *)calloc((size_t)narg,sizeof(char)) ;

   eee = getenv("AFNI_TRACE") ; ttt = YESSISH(eee) ;
   if( ttt )
     fprintf(stderr,"++ AFNI_prefilter_args() processing argv[1..%d]\n",narg-1) ;

   /*--- scan thru argv[];
         see if any should be processed now and marked as 'used up' ---*/

   for( ii=1 ; ii < narg ; ii++ ){

     /*** empty argument (should never happen in Unix) ***/

     if( argv[ii] == NULL ){
       if( ttt ) fprintf(stderr,"++ argv[%d] is NULL\n",ii) ;
       used[ii] = 1 ; continue ;
     }

     /*** -Dname=val to set environment variable ***/

     if( strncmp(argv[ii],"-D",2) == 0 && strchr(argv[ii],'=') != NULL ){
       if( ttt ) fprintf(stderr,"++ argv[%d] does setenv %s\n",ii,argv[ii]) ;
       (void)AFNI_setenv(argv[ii]+2) ; used[ii] = 1 ; continue ;
     }

     /*** -overwrite to set AFNI_DECONFLICT ***/

     if( strcmp(argv[ii],"-overwrite") == 0 ){
       if( ttt ) fprintf(stderr,"++ argv[%d] is -overwrite\n",ii) ;
       AFNI_setenv("AFNI_DECONFLICT=OVERWRITE") ; used[ii] = 1 ; continue ;
     }

     /*** -skip_afnirc to avoid .afnirc file ***/

     if( strcmp(argv[ii],"-skip_afnirc") == 0 ){
       if( ttt ) fprintf(stderr,"++ argv[%d] is -skip_afnirc\n",ii) ;
       AFNI_mark_environ_done() ; used[ii] = 1 ; continue ;
     }

     /*** if get to here, argv[ii] is nothing special ***/

   } /* end of loop over argv[] */

   /*--- compress out used up argv[] entries ---*/

   for( nused=0,ii=narg-1 ; ii >= 1 ; ii-- ){
     if( !used[ii] ) continue ;
     for( jj=ii+1 ; jj < narg ; jj++ ) argv[jj-1] = argv[jj] ;
     argv[narg-1] = NULL ; narg-- ; nused++ ;
   }

   if( ttt && nused > 0 )
     fprintf(stderr,"++ 'used up' %d argv[] entries, leaving %d\n",nused,narg) ;

   free((void *)used) ; *argc = narg ; return(nused);
}
