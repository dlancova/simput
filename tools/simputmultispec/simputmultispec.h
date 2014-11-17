/*
   This file is part of SIMPUT.

   SIMPUT is free software: you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   any later version.

   SIMPUT is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   For a copy of the GNU General Public License see
   <http://www.gnu.org/licenses/>.


   Copyright 2007-2014 Christian Schmid, FAU
*/

#ifndef SIMPUTMULTISPEC_H
#define SIMPUTMULTISPEC_H 1

#include "ape/ape_trad.h"

#include "simput.h"
#include "common.h"


#define TOOLSUB simputmultispec_main
#include "headas_main.c"

#define CHECK_MALLOC_RET_NULL(a) \
  if (NULL==a) { \
    SIMPUT_ERROR("memory allocation failed"); \
    return NULL;\
  }

#define CHECK_MALLOC_VOID(a) \
  if (NULL==a) { \
    SIMPUT_ERROR("memory allocation failed"); \
    return;\
  }


struct param_input{
	double minPar;
	double maxPar;
	int num_values;
	int logScale;
	char* param_files;
	char* param_names;

};

typedef struct{
	int num_param;

	// values for each parameter
	int num_pvals;
	char *par_names;
	double *pvals;
} par_info;

// is this number reasonable??
static const int maxStrLenCat = 64;
static char* extnameSpec = "SPECTRUM";
static int extver = 1;

struct Parameters {
  /** File name of the output SIMPUT file. */
  char Simput[SIMPUT_MAXSTR];

  /* Source position [deg]. */
  float RA;
  float Dec;

  /* Source flux [erg/s/cm^2]. If the source flux is not specified
      (value=0.0), it is set according to the assigned spectrum. */
  float srcFlux;

/*
  * ID of the source.
  int Src_ID;
  * Name of the source.
  char Src_Name[SIMPUT_MAXSTR];





*/

  /* Lower and upper boundary of the generated spectrum [keV].*/
  float Elow;
  float Eup;
  float Estep;

  /* Reference energy band [keV].*/
  float Emin;
  float Emax;

  /** File name of the input ISIS parameter file containing a spectral
       model	. */
   char ISISFile[SIMPUT_MAXSTR];


   /* File name for optional preperation script (f. e. to load additional
    models). */
   char ISISPrep[SIMPUT_MAXSTR];

   /* File name of the input Xspec spectral model. */
   char XSPECFile[SIMPUT_MAXSTR];


   // Param1
  char Param1File[SIMPUT_MAXSTR];
  char Param1Name[SIMPUT_MAXSTR];

  // Param2
 char Param2File[SIMPUT_MAXSTR];
 char Param2Name[SIMPUT_MAXSTR];

 int Param1num_values;
 int Param2num_values;

 char Param1logScale;
 char Param2logScale;

  /** File name of the input FITS image. */
  char ImageFile[SIMPUT_MAXSTR];

  int chatter;
  char clobber;
  char history;
};

struct node{

	// number of the parameter
	int param_num;

	// value index of the parameter (pvals[ind])
	int pind;

	// pointer to the parameter structure
	par_info *par;

	// image for the certain parameter combination
	double **img;

	// pointer to the next array of elements (i.e., param_num+1!)
	struct node **next;

};
typedef struct node param_node;

struct li{

	// number of the parameter
	int num_param;

	// index array of the parameter values
	int *pval_ar;

	// image for the certain parameter combination
	double **img;

	// pointer to the next array of elements (i.e., param_num+1!)
	struct li *next;

};
typedef struct li img_list;

int simputmultispec_getpar(struct Parameters* const par);

void query_simput_parameter_file_name(char *name, char *field, int *status);
void query_simput_parameter_string(char *name, char *field, int *status);
void query_simput_parameter_int(char *name, int *field, int *status);
void query_simput_parameter_float(char *name, float *field, int *status);
void query_simput_parameter_bool(char *name, char *field, int *status);

#endif /* SIMPUTMULTISPEC_H */

