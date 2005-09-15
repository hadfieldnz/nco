/* $Header: /data/zender/nco_20150216/nco/src/nco/mpncecat.c,v 1.10 2005-09-15 00:22:55 zender Exp $ */

/* ncecat -- netCDF ensemble concatenator */

/* Purpose: Join variables across files into a new record variable */

/* Copyright (C) 1995--2005 Charlie Zender

   This software may be modified and/or re-distributed under the terms of the GNU General Public License (GPL) Version 2
   The full license text is at http://www.gnu.ai.mit.edu/copyleft/gpl.html 
   and in the file nco/doc/LICENSE in the NCO source distribution.
   
   As a special exception to the terms of the GPL, you are permitted 
   to link the NCO source code with the HDF, netCDF, OPeNDAP, and UDUnits
   libraries and to distribute the resulting executables under the terms 
   of the GPL, but in addition obeying the extra stipulations of the 
   HDF, netCDF, OPeNDAP, and UDUnits licenses.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
   See the GNU General Public License for more details.
   
   The original author of this software, Charlie Zender, wants to improve it
   with the help of your suggestions, improvements, bug-reports, and patches.
   Please contact the NCO project at http://nco.sf.net or by writing
   Charlie Zender
   Department of Earth System Science
   University of California at Irvine
   Irvine, CA 92697-3100 */

#ifdef HAVE_CONFIG_H
#include <config.h> /* Autotools tokens */
#endif /* !HAVE_CONFIG_H */

/* Standard header files */
#include <math.h> /* sin cos cos sin 3.14159 */
#include <stdio.h> /* stderr, FILE, NULL, etc. */
#include <stdlib.h> /* atof, atoi, malloc, getopt */
#include <string.h> /* strcmp. . . */
#include <sys/stat.h> /* stat() */
#include <time.h> /* machine time */
#include <unistd.h> /* POSIX stuff */
#ifndef HAVE_GETOPT_LONG
#include "nco_getopt.h"
#else /* !NEED_GETOPT_LONG */ 
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif /* !HAVE_GETOPT_H */ 
#endif /* HAVE_GETOPT_LONG */

/* 3rd party vendors */
#include <netcdf.h> /* netCDF definitions and C library */
#ifdef ENABLE_MPI
#include <mpi.h> /* MPI definitions */
#endif /* !ENABLE_MPI */

/* Personal headers */
/* #define MAIN_PROGRAM_FILE MUST precede #include libnco.h */
#define MAIN_PROGRAM_FILE
#include "libnco.h" /* netCDF Operator (NCO) library */

int 
main(int argc,char **argv)
{
  bool EXCLUDE_INPUT_LIST=False; /* Option c */
  bool EXTRACT_ALL_COORDINATES=False; /* Option c */
  bool EXTRACT_ASSOCIATED_COORDINATES=True; /* Option C */
  bool FILE_RETRIEVED_FROM_REMOTE_LOCATION;
  bool FL_LST_IN_APPEND=True; /* Option H */
  bool FL_LST_IN_FROM_STDIN=False; /* [flg] fl_lst_in comes from stdin */
  bool FMT_64BIT=False; /* Option Z */
  bool FORCE_APPEND=False; /* Option A */
  bool FORCE_OVERWRITE=False; /* Option O */
  bool FORTRAN_IDX_CNV=False; /* Option F */
  bool HISTORY_APPEND=True; /* Option h */
  bool CNV_CCM_CCSM_CF;
  bool REMOVE_REMOTE_FILES_AFTER_PROCESSING=True; /* Option R */
  bool TOKEN_FREE=True; /* [flg] Allow MPI workers write-access to output file */

  char **fl_lst_abb=NULL; /* Option a */
  char **fl_lst_in;
  char **var_lst_in=NULL_CEWI;
  char *cmd_ln;
  char *fl_in=NULL;
  char *fl_out=NULL; /* Option o */
  char *fl_out_tmp=NULL; /* MPI CEWI */
  char *fl_pth=NULL; /* Option p */
  char *fl_pth_lcl=NULL; /* Option l */
  char *lmt_arg[NC_MAX_DIMS];
  char *optarg_lcl=NULL; /* [sng] Local copy of system optarg */
  char *time_bfr_srt;

  const char * const CVS_Id="$Id: mpncecat.c,v 1.10 2005-09-15 00:22:55 zender Exp $"; 
  const char * const CVS_Revision="$Revision: 1.10 $";
  const char * const opt_sht_lst="ACcD:d:FHhl:n:Oo:p:rRv:xZ-:";
  const double sleep_tm=0.04; /* [s] Token request interval */
  const int info_bfr_lng=3; /* [nbr] Number of elements in info_bfr */
  const int wrk_id_bfr_lng=1; /* [nbr] Number of elements in wrk_id_bfr */

  dmn_sct *rec_dmn;
  dmn_sct **dim;
  dmn_sct **dmn_out;
 
  double srt_tm; /* Start the clock */

  extern char *optarg;
  extern int optind;

  int fll_md_old; /* [enm] Old fill mode */
  int idx;
  int fl_idx;
  int in_id;  
  int info_bfr[3]; /* [bfr] Buffer containing var, idx, tkn_rsp */
  int out_id;
  int proc_id; /* [id] Process ID */
  int proc_nbr=0; /* [nbr] Number of MPI processes */
  int abb_arg_nbr=0;
  int nbr_dmn_fl;
  int lmt_nbr=0; /* Option d. NB: lmt_nbr gets incremented */
  int msg_typ; /* [enm] MPI message type */  
  int nbr_var_fl;
  int nbr_var_fix; /* nbr_var_fix gets incremented */
  int nbr_var_prc; /* nbr_var_prc gets incremented */
  int tkn_rsp; /* [enm] Mangager response [0,1] = [Wait,Allow] */
  int var_wrt_nbr=0; /* [nbr] Variables written to output file until now */
  int var_lst_in_nbr=0;
  int nbr_xtr=0; /* nbr_xtr won't otherwise be set for -c with no -v */
  int nbr_dmn_xtr;
  int fl_nbr=0;
  int fl_nm_lng; /* [nbr] Output file name length */
  int opt;
  int rcd=NC_NOERR; /* [rcd] Return code */
  int rec_dmn_id=NCO_REC_DMN_UNDEFINED;
  int wrk_id; /* [id] Sender node ID */
  int wrk_id_bfr[1]; /* [bfr] Buffer for wrk_id */
 
  lmt_sct **lmt;
  
  long idx_rec_out=0L; /* idx_rec_out gets incremented */

#ifdef ENABLE_MPI
  MPI_Status mpi_stt; /* [enm] Status check to decode msg_typ */
#endif /* !ENABLE_MPI */
  
  nm_id_sct *dmn_lst;
  nm_id_sct *xtr_lst=NULL; /* xtr_lst may be alloc()'d from NULL with -c option */
  
  time_t time_crr_time_t;
  
  var_sct **var;
  var_sct **var_fix;
  var_sct **var_fix_out;
  var_sct **var_out;
  var_sct **var_prc;
  var_sct **var_prc_out;
  
  static struct option opt_lng[]=
    { /* Structure ordered by short option key if possible */
      {"append",no_argument,0,'A'},
      {"coords",no_argument,0,'c'},
      {"crd",no_argument,0,'c'},
      {"no-coords",no_argument,0,'C'},
      {"no-crd",no_argument,0,'C'},
      {"debug",required_argument,0,'D'},
      {"dbg_lvl",required_argument,0,'D'},
      {"dimension",required_argument,0,'d'},
      {"dmn",required_argument,0,'d'},
      {"fortran",no_argument,0,'F'},
      {"ftn",no_argument,0,'F'},
      {"fl_lst_in",no_argument,0,'H'},
      {"file_list",no_argument,0,'H'},
      {"history",no_argument,0,'h'},
      {"hst",no_argument,0,'h'},
      {"local",required_argument,0,'l'},
      {"lcl",required_argument,0,'l'},
      {"nintap",required_argument,0,'n'},
      {"overwrite",no_argument,0,'O'},
      {"ovr",no_argument,0,'O'},
      {"output",required_argument,0,'o'},
      {"fl_out",required_argument,0,'o'},
      {"path",required_argument,0,'p'},
      {"retain",no_argument,0,'R'},
      {"rtn",no_argument,0,'R'},
      {"revision",no_argument,0,'r'},
      {"variable",required_argument,0,'v'},
      {"version",no_argument,0,'r'},
      {"vrs",no_argument,0,'r'},
      {"exclude",no_argument,0,'x'},
      {"xcl",no_argument,0,'x'},
      {"64-bit-offset",no_argument,0,'Z'},
      {"help",no_argument,0,'?'},
      {0,0,0,0}
    }; /* end opt_lng */
  int opt_idx=0; /* Index of current long option into opt_lng array */

#ifdef ENABLE_MPI
  /* MPI Initialization */
  MPI_Init(&argc,&argv);
  MPI_Comm_size(MPI_COMM_WORLD,&proc_nbr);
  MPI_Comm_rank(MPI_COMM_WORLD,&proc_id);
  srt_tm=MPI_Wtime();
#endif /* !ENABLE_MPI */

  /* Start clock and save command line */ 
  cmd_ln=nco_cmd_ln_sng(argc,argv);
  time_crr_time_t=time((time_t *)NULL);
  time_bfr_srt=ctime(&time_crr_time_t); time_bfr_srt=time_bfr_srt; /* Avoid compiler warning until variable is used for something */
  
  /* Get program name and set program enum (e.g., prg=ncra) */
  prg_nm=prg_prs(argv[0],&prg);

  /* Parse command line arguments */
  while((opt = getopt_long(argc,argv,opt_sht_lst,opt_lng,&opt_idx)) != EOF){
    switch(opt){
    case 'A': /* Toggle FORCE_APPEND */
      FORCE_APPEND=!FORCE_APPEND;
      break;
    case 'C': /* Extract all coordinates associated with extracted variables? */
      EXTRACT_ASSOCIATED_COORDINATES=False;
      break;
    case 'c':
      EXTRACT_ALL_COORDINATES=True;
      break;
    case 'D': /* Debugging level. Default is 0. */
      dbg_lvl=(unsigned short)strtol(optarg,(char **)NULL,10);
      break;
    case 'd': /* Copy argument for later processing */
      lmt_arg[lmt_nbr]=(char *)strdup(optarg);
      lmt_nbr++;
      break;
    case 'F': /* Toggle index convention. Default is 0-based arrays (C-style). */
      FORTRAN_IDX_CNV=!FORTRAN_IDX_CNV;
      break;
    case 'H': /* Toggle writing input file list attribute */
      FL_LST_IN_APPEND=!FL_LST_IN_APPEND;
      break;
    case 'h': /* Toggle appending to history global attribute */
      HISTORY_APPEND=!HISTORY_APPEND;
      break;
    case 'l': /* Local path prefix for files retrieved from remote file system */
      fl_pth_lcl=(char *)strdup(optarg);
      break;
    case 'n': /* NINTAP-style abbreviation of files to process */
      fl_lst_abb=lst_prs_2D(optarg,",",&abb_arg_nbr);
      if(abb_arg_nbr < 1 || abb_arg_nbr > 5){
	(void)fprintf(stdout,"%s: ERROR Incorrect abbreviation for file list\n",prg_nm);
	(void)nco_usg_prn();
	nco_exit(EXIT_FAILURE);
      } /* end if */
      break;
    case 'O': /* Toggle FORCE_OVERWRITE */
      FORCE_OVERWRITE=!FORCE_OVERWRITE;
      break;
    case 'o': /* Name of output file */
      fl_out=(char *)strdup(optarg);
      break;
    case 'p': /* Common file path */
      fl_pth=(char *)strdup(optarg);
      break;
    case 'R': /* Toggle removal of remotely-retrieved-files. Default is True. */
      REMOVE_REMOTE_FILES_AFTER_PROCESSING=!REMOVE_REMOTE_FILES_AFTER_PROCESSING;
      break;
    case 'r': /* Print CVS program information and copyright notice */
      (void)copyright_prn(CVS_Id,CVS_Revision);
      (void)nco_lbr_vrs_prn();
      nco_exit(EXIT_SUCCESS);
      break;
    case 'v': /* Variables to extract/exclude */
      /* Replace commas with hashes when within braces (convert back later) */
      optarg_lcl=(char *)strdup(optarg);
      (void)nco_lst_comma2hash(optarg_lcl);
      var_lst_in=lst_prs_2D(optarg_lcl,",",&var_lst_in_nbr);
      optarg_lcl=(char *)nco_free(optarg_lcl);
      nbr_xtr=var_lst_in_nbr;
      break;
    case 'x': /* Exclude rather than extract variables specified with -v */
      EXCLUDE_INPUT_LIST=True;
      break;
    case 'Z': /* [flg] Create output file with 64-bit offsets */
      FMT_64BIT=True;
      break;
    case '?': /* Print proper usage */
      (void)nco_usg_prn();
      nco_exit(EXIT_SUCCESS);
      break;
    case '-': /* Long options are not allowed */
      (void)fprintf(stderr,"%s: ERROR Long options are not available in this build. Use single letter options instead.\n",prg_nm_get());
      nco_exit(EXIT_FAILURE);
      break;
    default: /* Print proper usage */
      (void)nco_usg_prn();
      nco_exit(EXIT_FAILURE);
      break;
    } /* end switch */
  } /* end while loop */
  
  /* Process positional arguments and fill in filenames */
  fl_lst_in=nco_fl_lst_mk(argv,argc,optind,&fl_nbr,&fl_out,&FL_LST_IN_FROM_STDIN);

  /* Make uniform list of user-specified dimension limits */
  lmt=nco_lmt_prs(lmt_nbr,lmt_arg);
    
  /* Parse filename */
  fl_in=nco_fl_nm_prs(fl_in,0,&fl_nbr,fl_lst_in,abb_arg_nbr,fl_lst_abb,fl_pth);
  /* Make sure file is on local system and is readable or die trying */
  fl_in=nco_fl_mk_lcl(fl_in,fl_pth_lcl,&FILE_RETRIEVED_FROM_REMOTE_LOCATION);
  /* Open file for reading */
  rcd=nco_open(fl_in,NC_NOWRITE,&in_id);
  
  /* Get number of variables, dimensions, and record dimension ID of input file */
  (void)nco_inq(in_id,&nbr_dmn_fl,&nbr_var_fl,(int *)NULL,&rec_dmn_id);
  
  /* Form initial extraction list which may include extended regular expressions */
  xtr_lst=nco_var_lst_mk(in_id,nbr_var_fl,var_lst_in,EXTRACT_ALL_COORDINATES,&nbr_xtr);

  /* Change included variables to excluded variables */
  if(EXCLUDE_INPUT_LIST) xtr_lst=nco_var_lst_xcl(in_id,nbr_var_fl,xtr_lst,&nbr_xtr);

  /* Add all coordinate variables to extraction list */
  if(EXTRACT_ALL_COORDINATES) xtr_lst=nco_var_lst_add_crd(in_id,nbr_dmn_fl,xtr_lst,&nbr_xtr);

  /* Make sure coordinates associated extracted variables are also on extraction list */
  if(EXTRACT_ASSOCIATED_COORDINATES) xtr_lst=nco_var_lst_ass_crd_add(in_id,xtr_lst,&nbr_xtr);

  /* Sort extraction list by variable ID for fastest I/O */
  if(nbr_xtr > 1) xtr_lst=nco_lst_srt_nm_id(xtr_lst,nbr_xtr,False);
    
  /* We now have final list of variables to extract. Phew. */
  
  /* Find coordinate/dimension values associated with user-specified limits */
  for(idx=0;idx<lmt_nbr;idx++) (void)nco_lmt_evl(in_id,lmt[idx],0L,FORTRAN_IDX_CNV);
  
  /* Find dimensions associated with variables to be extracted */
  dmn_lst=nco_dmn_lst_ass_var(in_id,xtr_lst,nbr_xtr,&nbr_dmn_xtr);

  /* Fill in dimension structure for all extracted dimensions */
  dim=(dmn_sct **)nco_malloc(nbr_dmn_xtr*sizeof(dmn_sct *));
  for(idx=0;idx<nbr_dmn_xtr;idx++) dim[idx]=nco_dmn_fll(in_id,dmn_lst[idx].id,dmn_lst[idx].nm);
  /* Dimension list no longer needed */
  dmn_lst=nco_nm_id_lst_free(dmn_lst,nbr_dmn_xtr);
  
  /* Merge hyperslab limit information into dimension structures */
  if(lmt_nbr > 0) (void)nco_dmn_lmt_mrg(dim,nbr_dmn_xtr,lmt,lmt_nbr);

  /* Duplicate input dimension structures for output dimension structures */
  dmn_out=(dmn_sct **)nco_malloc(nbr_dmn_xtr*sizeof(dmn_sct *));
  for(idx=0;idx<nbr_dmn_xtr;idx++){
    dmn_out[idx]=nco_dmn_dpl(dim[idx]);
    (void)nco_dmn_xrf(dim[idx],dmn_out[idx]); 
  } /* end loop over idx */

  /* Is this an CCM/CCSM/CF-format history tape? */
  CNV_CCM_CCSM_CF=nco_cnv_ccm_ccsm_cf_inq(in_id);

  /* Fill in variable structure list for all extracted variables */
  var=(var_sct **)nco_malloc(nbr_xtr*sizeof(var_sct *));
  var_out=(var_sct **)nco_malloc(nbr_xtr*sizeof(var_sct *));
  for(idx=0;idx<nbr_xtr;idx++){
    var[idx]=nco_var_fll(in_id,xtr_lst[idx].id,xtr_lst[idx].nm,dim,nbr_dmn_xtr);
    var_out[idx]=nco_var_dpl(var[idx]);
    (void)nco_xrf_var(var[idx],var_out[idx]);
    (void)nco_xrf_dmn(var_out[idx]);
  } /* end loop over idx */
  /* Extraction list no longer needed */
  xtr_lst=nco_nm_id_lst_free(xtr_lst,nbr_xtr);

  /* Divide variable lists into lists of fixed variables and variables to be processed */
  (void)nco_var_lst_dvd(var,var_out,nbr_xtr,CNV_CCM_CCSM_CF,nco_pck_plc_nil,nco_pck_map_nil,(dmn_sct **)NULL,0,&var_fix,&var_fix_out,&nbr_var_fix,&var_prc,&var_prc_out,&nbr_var_prc);

#ifdef ENABLE_MPI
  if(proc_id == 0){ /* MPI manager code */
#endif /* !ENABLE_MPI */
  /* Open output file */
  fl_out_tmp=nco_fl_out_open(fl_out,FORCE_APPEND,FORCE_OVERWRITE,FMT_64BIT,&out_id);

  /* Copy global attributes */
  (void)nco_att_cpy(in_id,out_id,NC_GLOBAL,NC_GLOBAL,True);
  
  /* Catenate time-stamped command line to "history" global attribute */
  if(HISTORY_APPEND) (void)nco_hst_att_cat(out_id,cmd_ln);

#ifdef ENABLE_MPI
  /* Initialize MPI task information */
  if(proc_nbr > 0 && HISTORY_APPEND) (void)nco_mpi_att_cat(out_id,proc_nbr);
#endif /* !ENABLE_MPI */

  /* Add input file list global attribute */
  if(FL_LST_IN_APPEND  && HISTORY_APPEND && FL_LST_IN_FROM_STDIN) (void)nco_fl_lst_att_cat(out_id,fl_lst_in,fl_nbr);

#ifdef ENABLE_MPI
  } /* proc_id != 0 */
#endif /* !ENABLE_MPI */

  /* ncecat-specific operations */
  if(True){

    /* Always construct new "record" dimension from scratch */
    rec_dmn=(dmn_sct *)nco_malloc(sizeof(dmn_sct));
    rec_dmn->nm=(char *)strdup("record");
    rec_dmn->id=-1;
    rec_dmn->nc_id=-1;
    rec_dmn->xrf=NULL;
    rec_dmn->val.vp=NULL;
    rec_dmn->is_crd_dmn=False;
    rec_dmn->is_rec_dmn=True;
    rec_dmn->sz=0L;
    rec_dmn->cnt=0L;
    rec_dmn->srt=0L;
    rec_dmn->end=rec_dmn->sz-1L;
      
    /* Change existing record dimension, if any, to regular dimension */
    for(idx=0;idx<nbr_dmn_xtr;idx++){
      /* Is any input dimension a record dimension? */
      if(dmn_out[idx]->is_rec_dmn){
	dmn_out[idx]->is_rec_dmn=False;
	break;
      } /* end if */
    } /* end loop over idx */

    /* Add record dimension to end of dimension list */
    nbr_dmn_xtr++;
    dmn_out=(dmn_sct **)nco_realloc(dmn_out,nbr_dmn_xtr*sizeof(dmn_sct **));
    dmn_out[nbr_dmn_xtr-1]=rec_dmn;

  } /* end if */

#ifdef ENABLE_MPI
  if(proc_id == 0){ /* MPI manager code */
#endif /* !ENABLE_MPI */
  /* Define dimensions in output file */
  (void)nco_dmn_dfn(fl_out,out_id,dmn_out,nbr_dmn_xtr);
#ifdef ENABLE_MPI
  } /* proc_id != 0 */
#endif /* !ENABLE_MPI */

  if(True){
    /* Prepend record dimension to beginning of all vectors for processed variables */
    for(idx=0;idx<nbr_var_prc;idx++){
      var_prc_out[idx]->nbr_dim++;
      var_prc_out[idx]->is_rec_var=True;
      var_prc_out[idx]->sz_rec=var_prc_out[idx]->sz;
      
      /* Allocate space to hold dimension IDs */
      var_prc_out[idx]->dim=(dmn_sct **)nco_realloc(var_prc_out[idx]->dim,var_prc_out[idx]->nbr_dim*sizeof(dmn_sct *));
      var_prc_out[idx]->dmn_id=(int *)nco_realloc(var_prc_out[idx]->dmn_id,var_prc_out[idx]->nbr_dim*sizeof(int));
      var_prc_out[idx]->cnt=(long *)nco_realloc(var_prc_out[idx]->cnt,var_prc_out[idx]->nbr_dim*sizeof(long));
      var_prc_out[idx]->srt=(long *)nco_realloc(var_prc_out[idx]->srt,var_prc_out[idx]->nbr_dim*sizeof(long));
      var_prc_out[idx]->end=(long *)nco_realloc(var_prc_out[idx]->end,var_prc_out[idx]->nbr_dim*sizeof(long));
      
      /* Move current array by one to make room for new record dimension info */
      (void)memmove((void *)(var_prc_out[idx]->dim+1),(void *)(var_prc_out[idx]->dim),(var_prc_out[idx]->nbr_dim-1)*sizeof(dmn_sct *));
      (void)memmove((void *)(var_prc_out[idx]->dmn_id+1),(void *)(var_prc_out[idx]->dmn_id),(var_prc_out[idx]->nbr_dim-1)*sizeof(int));
      (void)memmove((void *)(var_prc_out[idx]->cnt+1),(void *)(var_prc_out[idx]->cnt),(var_prc_out[idx]->nbr_dim-1)*sizeof(long));
      (void)memmove((void *)(var_prc_out[idx]->srt+1),(void *)(var_prc_out[idx]->srt),(var_prc_out[idx]->nbr_dim-1)*sizeof(long));
      (void)memmove((void *)(var_prc_out[idx]->end+1),(void *)(var_prc_out[idx]->end),(var_prc_out[idx]->nbr_dim-1)*sizeof(long));
      
      /* Insert value for new record dimension */
      var_prc_out[idx]->dim[0]=rec_dmn;
      var_prc_out[idx]->dmn_id[0]=rec_dmn->id;
      var_prc_out[idx]->cnt[0]=1L;
      var_prc_out[idx]->srt[0]=-1L;
      var_prc_out[idx]->end[0]=-1L;
		    
    } /* end loop over idx */
    
  } /* end if */

#ifdef ENABLE_MPI
  if(proc_id == 0){ /* MPI manager code */
#endif /* !ENABLE_MPI */
  /* Define variables in output file, copy their attributes */
  (void)nco_var_dfn(in_id,fl_out,out_id,var_out,nbr_xtr,(dmn_sct **)NULL,(int)0,nco_pck_plc_nil,nco_pck_map_nil);
#ifdef ENABLE_MPI
  } /* proc_id != 0 */
#endif /* !ENABLE_MPI */

  /* Zero start vectors for all output variables */
  (void)nco_var_srt_zero(var_out,nbr_xtr);

#ifdef ENABLE_MPI
  if(proc_id == 0){ /* proc_id != 0 */
#endif /* !ENABLE_MPI */

  /* Turn off default filling behavior to enhance efficiency */
  rcd=nco_set_fill(out_id,NC_NOFILL,&fll_md_old);
  
  /* Take output file out of define mode */
  (void)nco_enddef(out_id);
  
#ifdef ENABLE_MPI
  } /* proc_id != 0 */

  /* Manager obtains output filename and broadcasts to workers */
  if(proc_id == 0) fl_nm_lng=(int)strlen(fl_out_tmp);
  MPI_Bcast(&fl_nm_lng,1,MPI_INT,0,MPI_COMM_WORLD);
  if(proc_id != 0) fl_out_tmp=(char *)malloc((fl_nm_lng+1)*sizeof(char));
  MPI_Bcast(fl_out_tmp,fl_nm_lng+1,MPI_CHAR,0,MPI_COMM_WORLD);

  if(proc_id == 0){ /* MPI manager code */
    TOKEN_FREE=False;
#endif /* !ENABLE_MPI */
    /* Copy variable data for non-processed variables */
    (void)nco_var_val_cpy(in_id,out_id,var_fix,nbr_var_fix);
#ifdef ENABLE_MPI
    /* Close output file so workers can open it */
    nco_close(out_id);
    TOKEN_FREE=True;
  } /* proc_id != 0 */
#endif /* !ENABLE_MPI */

  /* Close first input netCDF file */
  (void)nco_close(in_id);
  
  /* Loop over input files */
  for(fl_idx=0;fl_idx<fl_nbr;fl_idx++){
#ifdef ENABLE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
#endif /* !ENABLE_MPI */
    /* Parse filename */
    if(fl_idx != 0) fl_in=nco_fl_nm_prs(fl_in,fl_idx,(int *)NULL,fl_lst_in,abb_arg_nbr,fl_lst_abb,fl_pth);
    if(dbg_lvl > 0) (void)fprintf(stderr,"\nInput file %d is %s; ",fl_idx,fl_in);
    /* Make sure file is on local system and is readable or die trying */
    if(fl_idx != 0) fl_in=nco_fl_mk_lcl(fl_in,fl_pth_lcl,&FILE_RETRIEVED_FROM_REMOTE_LOCATION);
    if(dbg_lvl > 0) (void)fprintf(stderr,"local file %s:\n",fl_in);
    rcd=nco_open(fl_in,NC_NOWRITE,&in_id);
    
    /* Perform various error-checks on input file */
    if(False) (void)nco_fl_cmp_err_chk();

#ifdef ENABLE_MPI
    if(proc_id == 0){ /* MPI manager code */
      /* Compensate for incrementing on each worker's first message */
      var_wrt_nbr=-proc_nbr+1;
      idx=0;
      /* While variables remain to be processed or written... */
      while(var_wrt_nbr < nbr_var_prc){
	/* Receive message from any worker */
	MPI_Recv(wrk_id_bfr,wrk_id_bfr_lng,MPI_INT,MPI_ANY_SOURCE,MPI_ANY_TAG,MPI_COMM_WORLD,&mpi_stt);
	/* Obtain MPI message type */
	msg_typ=mpi_stt.MPI_TAG;
	/* Get sender's proc_id */
	wrk_id=wrk_id_bfr[0];

	/* Allocate next variable, if any, to worker */
	if(msg_typ == WORK_REQUEST){
	  var_wrt_nbr++; /* [nbr] Number of variables written */
	  /* Worker closed output file before sending WORK_REQUEST */
	  TOKEN_FREE=True;

	  if(idx > nbr_var_prc-1){
	    /* Variable index = -1 indicates NO_MORE_WORK */
	    info_bfr[0]=NO_MORE_WORK; /* [idx] -1 */
	    info_bfr[1]=out_id; /* Output file ID */
	  }else{
	    /* Tell requesting worker to allocate space for next variable */
	    info_bfr[0]=idx; /* [idx] Variable to be processed */
	    info_bfr[1]=out_id; /* Output file ID */
	    info_bfr[2]=var_prc_out[idx]->id; /* [id] Variable ID in output file */
	    /* Point to next variable on list */
	    idx++;
	  } /* endif idx */
	  MPI_Send(info_bfr,info_bfr_lng,MPI_INT,wrk_id,WORK_ALLOC,MPI_COMM_WORLD);
	  /* msg_typ != WORK_REQUEST */
	}else if(msg_typ == TOKEN_REQUEST){
	  /* Allocate token if free, else ask worker to try later */
	  if(TOKEN_FREE){
	    TOKEN_FREE=False;
	    info_bfr[0]=1; /* Allow */
	  }else{
	    info_bfr[0]=0; /* Wait */
	  } /* !TOKEN_FREE */
	  MPI_Send(info_bfr,info_bfr_lng,MPI_INT,wrk_id,TOKEN_RESULT,MPI_COMM_WORLD);
	} /* msg_typ != TOKEN_REQUEST */
      } /* end while var_wrt_nbr < nbr_var_prc */
    }else{ /* proc_id != 0, end Manager code begin Worker code */
      wrk_id_bfr[0]=proc_id;
      while(1){ /* While work remains... */
	/* Send WORK_REQUEST */
	wrk_id_bfr[0]=proc_id;
	MPI_Send(wrk_id_bfr,wrk_id_bfr_lng,MPI_INT,mgr_id,WORK_REQUEST,MPI_COMM_WORLD);
	/* Receive WORK_ALLOC */
	MPI_Recv(info_bfr,info_bfr_lng,MPI_INT,0,WORK_ALLOC,MPI_COMM_WORLD,&mpi_stt);
	idx=info_bfr[0];
	out_id=info_bfr[1];
	if(idx == NO_MORE_WORK) break;
	else{
	  var_prc_out[idx]->id=info_bfr[2];
	  /* Process this variable same as UP code */
#endif /* !ENABLE_MPI */
#ifndef ENABLE_MPI
	  /* UP code main loop over variables */
	  for(idx=0;idx<nbr_var_prc;idx++){
#endif /* !ENABLE_MPI */
       /* Common code for UP and MPI */ /* fxm: requires C99 as is? */
	    if(dbg_lvl > 1) (void)fprintf(stderr,"%s, ",var_prc[idx]->nm);
	    if(dbg_lvl > 0) (void)fflush(stderr);
	    /* Variables may have different ID, missing_value, type, in each file */
	    (void)nco_var_mtd_refresh(in_id,var_prc[idx]);
	    /* Retrieve variable from disk into memory */
	    (void)nco_var_get(in_id,var_prc[idx]);
	    /* Size of record dimension is 1 in output file */
	    var_prc_out[idx]->cnt[0]=1L;
	    var_prc_out[idx]->srt[0]=idx_rec_out;
      
#ifdef ENABLE_MPI
	    /* Obtain token and prepare to write */
	    while(1){ /* Send TOKEN_REQUEST repeatedly until token obtained */
	      wrk_id_bfr[0]=proc_id;
	      MPI_Send(wrk_id_bfr,wrk_id_bfr_lng,MPI_INT,mgr_id,TOKEN_REQUEST,MPI_COMM_WORLD);
	      /* Receive TOKEN_RESULT (1,0)=(ALLOW,WAIT) */
	      MPI_Recv(info_bfr,info_bfr_lng,MPI_INT,mgr_id,TOKEN_RESULT,MPI_COMM_WORLD,&mpi_stt);
	      tkn_rsp=info_bfr[0];
	      /* Wait then re-send request */
	      if(tkn_rsp == TOKEN_WAIT) sleep(sleep_tm); else break;
	    } /* end while loop waiting for write token */

	    /* Worker has token---prepare to write */
	    if(tkn_rsp == TOKEN_ALLOC){
	      rcd=nco_open(fl_out_tmp,NC_WRITE,&out_id);
#endif /* !ENABLE_MPI */
	      /* Write variable into current record in output file */
	      if(var_prc[idx]->nbr_dim == 0){
		(void)nco_put_var1(out_id,var_prc_out[idx]->id,var_prc_out[idx]->srt,var_prc[idx]->val.vp,var_prc[idx]->type);
	      }else{ /* end if variable is a scalar */
		(void)nco_put_vara(out_id,var_prc_out[idx]->id,var_prc_out[idx]->srt,var_prc_out[idx]->cnt,var_prc[idx]->val.vp,var_prc[idx]->type);
	      } /* end if variable is array */

	  /* Free current input buffer */
	  var_prc[idx]->val.vp=nco_free(var_prc[idx]->val.vp);
#ifdef ENABLE_MPI
	      /* Close output file and increment written counter */
	      nco_close(out_id);
	      var_wrt_nbr++;
	    } /* endif TOKEN_ALLOC */
	  } /* end else !NO_MORE_WORK */
	} /* end while loop requesting work/token */
      } /* endif Worker */
#else /* !ENABLE_MPI */
    }  /* end for loop over idx */
#endif /* !ENABLE_MPI */
      
    idx_rec_out++; /* [idx] Index of current record in output file (0 is first, ...) */
    if(dbg_lvl > 1) (void)fprintf(stderr,"\n");
    
    /* Close input netCDF file */
    nco_close(in_id);

    /* Remove local copy of file */
    if(FILE_RETRIEVED_FROM_REMOTE_LOCATION && REMOVE_REMOTE_FILES_AFTER_PROCESSING) (void)nco_fl_rm(fl_in);
#ifdef ENABLE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
#endif /* !ENABLE_MPI */
  } /* end loop over fl_idx */
  
#ifdef ENABLE_MPI
    /* Manager moves output file (closed by workers) from temporary to permanent location */
    if(proc_id == 0) (void)nco_fl_mv(fl_out_tmp,fl_out);
#else /* !ENABLE_MPI */
    /* Close output file and move it from temporary to permanent location */
    (void)nco_fl_out_cls(fl_out,fl_out_tmp,out_id);
#endif /* end !ENABLE_MPI */
    
  /* ncecat-specific memory cleanup */

  /* NCO-generic clean-up */
  /* Free individual strings */
  if(cmd_ln != NULL) cmd_ln=(char *)nco_free(cmd_ln);
  if(fl_in != NULL) fl_in=(char *)nco_free(fl_in);
  if(fl_out != NULL) fl_out=(char *)nco_free(fl_out);
  if(fl_out_tmp != NULL) fl_out_tmp=(char *)nco_free(fl_out_tmp);
  if(fl_pth != NULL) fl_pth=(char *)nco_free(fl_pth);
  if(fl_pth_lcl != NULL) fl_pth_lcl=(char *)nco_free(fl_pth_lcl);
  /* Free lists of strings */
  if(fl_lst_in != NULL && fl_lst_abb == NULL) fl_lst_in=nco_sng_lst_free(fl_lst_in,fl_nbr); 
  if(fl_lst_in != NULL && fl_lst_abb != NULL) fl_lst_in=nco_sng_lst_free(fl_lst_in,1);
  if(fl_lst_abb != NULL) fl_lst_abb=nco_sng_lst_free(fl_lst_abb,abb_arg_nbr);
  if(var_lst_in_nbr > 0) var_lst_in=nco_sng_lst_free(var_lst_in,var_lst_in_nbr);
  /* Free limits */
  for(idx=0;idx<lmt_nbr;idx++) lmt_arg[idx]=(char *)nco_free(lmt_arg[idx]);
  if(lmt_nbr > 0) lmt=nco_lmt_lst_free(lmt,lmt_nbr);
  /* Free dimension lists */
  if(nbr_dmn_xtr > 0) dim=nco_dmn_lst_free(dim,nbr_dmn_xtr-1); /* NB: ncecat has one fewer input than output dimension */
  if(nbr_dmn_xtr > 0) dmn_out=nco_dmn_lst_free(dmn_out,nbr_dmn_xtr); 
  /* Free variable lists */
  if(nbr_xtr > 0) var=nco_var_lst_free(var,nbr_xtr);
  if(nbr_xtr > 0) var_out=nco_var_lst_free(var_out,nbr_xtr);
  var_prc=(var_sct **)nco_free(var_prc);
  var_prc_out=(var_sct **)nco_free(var_prc_out);
  var_fix=(var_sct **)nco_free(var_fix);
  var_fix_out=(var_sct **)nco_free(var_fix_out);
 
#ifdef ENABLE_MPI
  MPI_Finalize();
#endif /* !ENABLE_MPI */

  if(rcd != NC_NOERR) nco_err_exit(rcd,"main");
  nco_exit_gracefully();
  return EXIT_SUCCESS;
} /* end main() */
