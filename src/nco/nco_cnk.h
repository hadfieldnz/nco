/* $Header: /data/zender/nco_20150216/nco/src/nco/nco_cnk.h,v 1.31 2014-01-06 07:36:08 pvicente Exp $ */

/* Purpose: Description (definition) of chunking functions */

/* Copyright (C) 1995--2014 Charlie Zender
   License: GNU General Public License (GPL) Version 3
   See http://www.gnu.org/copyleft/gpl.html for full license text */

/* Usage:
   #include "nco_cnk.h" *//* Chunking */

#ifndef NCO_CNK_H
#define NCO_CNK_H

/* Standard header files */
#include <math.h> /* sin cos cos sin 3.14159 */
#include <stdio.h> /* stderr, FILE, NULL, printf */

/* 3rd party vendors */
#include <netcdf.h> /* netCDF definitions and C library */
#include "nco_netcdf.h" /* NCO wrappers for netCDF C library */

/* Personal headers */
#include "nco.h" /* netCDF Operator (NCO) definitions */
#include "nco_fl_utl.h" /* File manipulation */
#include "nco_grp_utl.h" /* Group utilities */
#include "nco_lst_utl.h" /* List utilities */
#include "nco_mmr.h" /* Memory management */

/* Chunking policies: 
   Handle each chunking policies in nco_cnk_sz_set() */
enum nco_cnk_plc{ /* [enm] Chunking policy */
  nco_cnk_plc_nil, /* 0 [enm] Do not think about chunking */
  nco_cnk_plc_all, /* 1 [enm] Chunk all variables */
  nco_cnk_plc_g2d, /* 2 [enm] Chunk variables >= two dimensions */
  nco_cnk_plc_g3d, /* 3 [enm] Chunk variables >= three dimensions */
  nco_cnk_plc_xpl, /* 4 [enm] Chunk variables with explicitly chunked dimensions */
  nco_cnk_plc_xst, /* 5 [enm] Chunk variables that are chunked in input */
  nco_cnk_plc_uck /* 6 [enm] Unchunk all chunked variables */
}; /* end nco_cnk_plc enum */

/* Chunking type maps: */
enum nco_cnk_map{ /* [enm] Chunking conversion map */
  nco_cnk_map_nil, /* 0 [enm] Do not chunk anything, storage is unchanged */
  nco_cnk_map_dmn, /* 1 [enm] Chunksize equals dimension size */
  nco_cnk_map_rd1, /* 2 [enm] Chunksize equals dimension size except record dimension has size one */
  nco_cnk_map_scl, /* 3 [enm] Chunksize equals scalar size specified */
  nco_cnk_map_prd, /* 4 [enm] Chunksize product is scalar size specified */
  nco_cnk_map_lfp, /* 5 [enm] Chunksizes of lefter dimensions flexes to matches scalar size specified */
  nco_cnk_map_xst /* 6 [enm] Chunksize equals chunk-sizes in input file */
}; /* end nco_cnk_map enum */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void 
nco_dfl_case_cnk_map_err(void); /* [fnc] Print error and exit for illegal switch(cnk_map) case */

void 
nco_dfl_case_cnk_plc_err(void); /* [fnc] Print error and exit for illegal switch(nco_cnk_plc) case */

const char * /* O [sng] Chunking map string */
nco_cnk_map_sng_get /* [fnc] Convert chunking map enum to string */
(const int nco_cnk_map); /* I [enm] Chunking map */

const char * /* O [sng] Chunking policy string */
nco_cnk_plc_sng_get /* [fnc] Convert chunking policy enum to string */
(const int nco_cnk_plc); /* I [enm] Chunking policy */

int /* [rcd] [enm] Return code */
nco_cnk_ini /* [fnc] Create structure with all chunking information */
(const char * const fl_out, /* I [sng] Output filename */
 CST_X_PTR_CST_PTR_CST_Y(char,cnk_arg), /* I [sng] List of user-specified chunksizes */
 const int cnk_nbr, /* I [nbr] Number of chunksizes specified */
 const int cnk_map, /* I [enm] Chunking map */
 const int cnk_plc, /* I [enm] Chunking policy */
 const size_t cnk_sz_byt, /* I [B] Chunk size in bytes */
 const size_t cnk_sz_scl, /* I [nbr] Chunk size scalar */
 cnk_sct * const cnk); /* O [sct] Chunking structure */

cnk_dmn_sct ** /* O [sct] Structure list with user-specified per-dimension chunking information */
nco_cnk_prs /* [fnc] Create chunking structures with name and chunksize elements */
(const int cnk_nbr, /* I [nbr] Number of chunksizes specified */
 CST_X_PTR_CST_PTR_CST_Y(char,cnk_arg)); /* I [sng] List of user-specified chunksizes */

cnk_dmn_sct ** /* O [sct] Pointer to free'd structure list */
nco_cnk_lst_free /* [fnc] Free memory associated with chunking structure list */
(cnk_dmn_sct **cnk_lst, /* I/O [sct] Chunking structure list to free */
 const int cnk_nbr); /* I [nbr] Number of chunking structures in list */

cnk_dmn_sct * /* O [sct] Pointer to free'd chunking structure */
nco_cnk_dmn_free /* [fnc] Free all memory associated with chunking structure */
(cnk_dmn_sct *cnk_dmn); /* I/O [sct] Chunking structure to free */

void
nco_cnk_sz_set /* [fnc] Set chunksize parameters */
(const int nc_id, /* I [id] netCDF file ID */
 CST_X_PTR_CST_PTR_CST_Y(lmt_msa_sct,lmt_all_lst), /* I [sct] Hyperslab limits */
 const int lmt_all_lst_nbr, /* I [nbr] Number of hyperslab limits */
 int * const cnk_map_ptr, /* I/O [enm] Chunking map */
 int * const cnk_plc_ptr, /* I/O [enm] Chunking policy */
 const size_t cnk_sz_scl, /* I [nbr] Chunk size scalar */
 CST_X_PTR_CST_PTR_CST_Y(cnk_dmn_sct,cnk_dmn), /* I [sct] Chunking information */
 const int cnk_nbr); /* I [nbr] Number of dimensions with user-specified chunking */

void
nco_cnk_sz_set_trv /* [fnc] Set chunksize parameters (GTT version of nco_cnk_sz_set() ) */
(const int grp_id_in, /* I [id] netCDF group ID in input file */
 const int grp_id_out, /* I [id] netCDF group ID in output file */
 const cnk_sct * const cnk, /* I [sct] Chunking structure */
 const trv_sct * const var_trv); /* I [sct] Variable Object */

int /* O [enm] Chunking map */
nco_cnk_map_get /* [fnc] Convert user-specified chunking map to key */
(const char *nco_cnk_map_sng); /* [sng] User-specified chunking map */

int /* O [enm] Chunking policy */
nco_cnk_plc_get /* [fnc] Convert user-specified chunking policy to key */
(const char *nco_cnk_plc_sng); /* [sng] User-specified chunking policy */

nco_bool /* O [flg] Variable is chunked on disk */
nco_cnk_dsk_inq /* [fnc] Check whether variable is chunked on disk */
(const int nc_id, /* I [idx] netCDF file ID */
 const int var_id); /* I [id] Variable ID */

#if 0 
size_t * /* O [nbr] Chunksize array for variable */
nco_cnk_sz_get /* [fnc] Determine chunksize array */
(const int nc_id, /* I [id] netCDF file ID */
 const char * const var_nm, /* I [sng] Variable name */
 const int cnk_map, /* I [enm] Chunking map */
 const int cnk_plc, /* I [enm] Chunking policy */
 const size_t cnk_sz_scl, /* I [nbr] Chunk size scalar */
 CST_X_PTR_CST_PTR_CST_Y(cnk_dmn_sct,cnk_dmn), /* I [sct] Chunking information */
 const int cnk_nbr); /* I [nbr] Number of dimensions with user-specified chunking */

nco_bool /* O [flg] NCO will attempt to chunk variable */
nco_is_chunkable /* [fnc] Will NCO attempt to chunk variable? */
(const nc_type nc_typ_in); /* I [enm] Type of input variable */
#endif /* endif 0 */

#ifdef __cplusplus
} /* end extern "C" */
#endif /* __cplusplus */

#endif /* NCO_CNK_H */
