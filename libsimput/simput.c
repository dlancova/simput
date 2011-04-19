#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <malloc.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "simput.h"


/////////////////////////////////////////////////////////////////
// Macros.
/////////////////////////////////////////////////////////////////


/** Common string length. */
#define SIMPUT_MAXSTR (1025)


#define SIMPUT_ERROR(msg) \
  fprintf(stderr, "Error in %s: %s!\n", __func__, msg)

#define CHECK_STATUS_RET(a,b) \
  if (EXIT_SUCCESS!=a) return(b)

#define CHECK_STATUS_VOID(a) \
  if (EXIT_SUCCESS!=a) return

#define CHECK_STATUS_BREAK(a) \
  if (EXIT_SUCCESS!=a) break;

#define CHECK_NULL_RET(a,status,msg,ret) \
  if (NULL==a) { \
    SIMPUT_ERROR(msg); \
    status=EXIT_FAILURE; \
    return(ret);\
  }

#define CHECK_NULL_VOID(a,status,msg) \
  if (NULL==a) { \
    SIMPUT_ERROR(msg); \
    status=EXIT_FAILURE; \
    return;\
  }

#define CHECK_NULL_BREAK(a,status,msg) \
  if (NULL==a) { \
    SIMPUT_ERROR(msg); \
    status=EXIT_FAILURE; \
    break;\
  }


/////////////////////////////////////////////////////////////////
// Structures.
/////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////
// Static Variables.
/////////////////////////////////////////////////////////////////


static struct ARF* static_arf=NULL;


/////////////////////////////////////////////////////////////////
// Function Declarations.
/////////////////////////////////////////////////////////////////


/** Check if the HDU referred to by filename is a binary table
    extension. */
static int check_if_btbl(const char* const filename, int* const status);

/** Read the TUNITn keyword from the header of the current FITS HDU,
    where n denotes the specfied column. */
static void read_unit(fitsfile* const fptr, const int column, 
		      char* unit, int* const status);

/** Convert a string into lower case letters. The string has to be
    terminated by a '\0' mark. */
static void strtolower(char* const string);

/** Determine the factor required to convert the specified unit into
    [rad]. If the conversion is not possible or implemented, the
    function return value is 0. */
static float unit_conversion_rad(const char* const unit);

/** Determine the factor required to convert the specified unit into
    [keV]. If the conversion is not possible or implemented, the
    function return value is 0. */
static float unit_conversion_keV(const char* const unit);

/** Determine the factor required to convert the specified unit into
    [erg/s/cm**2]. If the conversion is not possible or implemented,
    the function return value is 0. */
static float unit_conversion_ergpspcm2(const char* const unit);


/////////////////////////////////////////////////////////////////
// Function Definitions.
/////////////////////////////////////////////////////////////////


SimputSourceEntry* getSimputSourceEntry(int* const status)
{
  SimputSourceEntry* entry=(SimputSourceEntry*)malloc(sizeof(SimputSourceEntry));
  CHECK_NULL_RET(entry, *status, 
		 "memory allocation for SimputSourceEntry failed", entry);

  // Initialize elements.
  entry->src_id  =0;
  entry->src_name=NULL;
  entry->ra      =0.;
  entry->dec     =0.;
  entry->imgrota =0.;
  entry->imgscal =1.;
  entry->e_min   =0.;
  entry->e_max   =0.;
  entry->flux    =0.;
  entry->spectrum=NULL;
  entry->image   =NULL;
  entry->lightcur=NULL;
  
  return(entry);
}


SimputSourceEntry* getSimputSourceEntryV(const int src_id, 
					 const char* const src_name,
					 const double ra,
					 const double dec,
					 const float imgrota,
					 const float imgscal,
					 const float e_min,
					 const float e_max,
					 const float flux,
					 const char* const spectrum,
					 const char* const image,
					 const char* const lightcur,
					 int* const status)
{
  SimputSourceEntry* entry=getSimputSourceEntry(status);
  CHECK_STATUS_RET(*status, entry);

  // Initialize with the given values.
  entry->src_id = src_id;

  entry->src_name=(char*)malloc((strlen(src_name)+1)*sizeof(char));
  CHECK_NULL_RET(entry->src_name, *status,
		 "memory allocation for source name failed", entry);
  strcpy(entry->src_name, src_name);

  entry->ra      = ra;
  entry->dec     = dec;
  entry->imgrota = imgrota;
  entry->imgscal = imgscal;
  entry->e_min   = e_min;
  entry->e_max   = e_max;
  entry->flux    = flux;

  entry->spectrum=(char*)malloc((strlen(spectrum)+1)*sizeof(char));
  CHECK_NULL_RET(entry->spectrum, *status,
		 "memory allocation for source name failed", entry);
  strcpy(entry->spectrum, spectrum);

  entry->image  =(char*)malloc((strlen(image)+1)*sizeof(char));
  CHECK_NULL_RET(entry->image, *status,
		 "memory allocation for source name failed", entry);
  strcpy(entry->image, image);

  entry->lightcur=(char*)malloc((strlen(lightcur)+1)*sizeof(char));
  CHECK_NULL_RET(entry->lightcur, *status,
		 "memory allocation for source name failed", entry);
  strcpy(entry->lightcur, lightcur);
  

  return(entry);
}


void freeSimputSourceEntry(SimputSourceEntry** const entry)
{
  if (NULL!=*entry) {
    if (NULL!=(*entry)->src_name) {
      free((*entry)->src_name);
    }
    if (NULL!=(*entry)->spectrum) {
      free((*entry)->spectrum);
    }
    if (NULL!=(*entry)->image) {
      free((*entry)->image);
    }
    if (NULL!=(*entry)->lightcur) {
      free((*entry)->lightcur);
    }
    free(*entry);
    *entry=NULL;
  }
}


SimputSourceCatalog* getSimputSourceCatalog(int* const status)
{
  SimputSourceCatalog* catalog=(SimputSourceCatalog*)malloc(sizeof(SimputSourceCatalog));
  CHECK_NULL_RET(catalog, *status, 
		 "memory allocation for SimputSourceCatalog failed", catalog);

  // Initialize elements.
  catalog->nentries=0;
  catalog->entries =NULL;

  return(catalog);
}


void freeSimputSourceCatalog(SimputSourceCatalog** const catalog)
{
  if (NULL!=*catalog) {
    if ((*catalog)->nentries>0) {
      int ii;
      for (ii=0; ii<(*catalog)->nentries; ii++) {
	if (NULL!=(*catalog)->entries[ii]) {
	  freeSimputSourceEntry(&((*catalog)->entries[ii]));
	}
      }
      free((*catalog)->entries);
    }
    free(*catalog);
    *catalog=NULL;
  }
}


SimputSourceCatalog* loadSimputSourceCatalog(const char* const filename,
					     int* const status)
{
  SimputSourceCatalog* catalog = getSimputSourceCatalog(status);
  CHECK_STATUS_RET(*status, catalog);

  // Open the specified FITS file.
  fitsfile* fptr=NULL;
  fits_open_file(&fptr, filename, READONLY, status);
  CHECK_STATUS_RET(*status, catalog);

  // Move to the right extension.
  fits_movnam_hdu(fptr, BINARY_TBL, "SRC_CAT", 0, status);
  CHECK_STATUS_RET(*status, catalog);

  do { // Error handling loop.
    // Get the column names.
    int csrc_id=0, csrc_name=0, cra=0, cdec=0, cimgrota=0, cimgscal=0,
      ce_min=0, ce_max=0, cflux=0, cspectrum=0, cimage=0, clightcur=0;
    // Required columns:
    fits_get_colnum(fptr, CASEINSEN, "SRC_ID", &csrc_id, status);
    fits_get_colnum(fptr, CASEINSEN, "RA", &cra, status);
    fits_get_colnum(fptr, CASEINSEN, "DEC", &cdec, status);
    fits_get_colnum(fptr, CASEINSEN, "E_MIN", &ce_min, status);
    fits_get_colnum(fptr, CASEINSEN, "E_MAX", &ce_max, status);
    fits_get_colnum(fptr, CASEINSEN, "FLUX", &cflux, status);
    fits_get_colnum(fptr, CASEINSEN, "SPECTRUM", &cspectrum, status);
    fits_get_colnum(fptr, CASEINSEN, "IMAGE", &cimage, status);
    fits_get_colnum(fptr, CASEINSEN, "LIGHTCUR", &clightcur, status);
    CHECK_STATUS_BREAK(*status);
    // Optional columns:
    int opt_status=EXIT_SUCCESS;
    fits_write_errmark();
    fits_get_colnum(fptr, CASEINSEN, "SRC_NAME", &csrc_name, &opt_status);
    opt_status=EXIT_SUCCESS;
    fits_get_colnum(fptr, CASEINSEN, "IMGROTA", &cimgrota, &opt_status);
    opt_status=EXIT_SUCCESS;
    fits_get_colnum(fptr, CASEINSEN, "IMGSCAL", &cimgscal, &opt_status);
    opt_status=EXIT_SUCCESS;
    fits_clear_errmark();

    // Take care of the units. Determine conversion factors.
    char ura[SIMPUT_MAXSTR];
    read_unit(fptr, cra, ura, status);
    CHECK_STATUS_BREAK(*status);
    float fra = unit_conversion_rad(ura);
    if (0.==fra) {
      SIMPUT_ERROR("unknown units in RA column");
      *status=EXIT_FAILURE;
      break;
    }

    char udec[SIMPUT_MAXSTR];
    read_unit(fptr, cdec, udec, status);
    CHECK_STATUS_BREAK(*status);
    float fdec = unit_conversion_rad(udec);
    if (0.==fdec) {
      SIMPUT_ERROR("unknown units in DEC column");
      *status=EXIT_FAILURE;
      break;
    }

    float fimgrota=0;
    if (cimgrota>0) {
      char uimgrota[SIMPUT_MAXSTR];
      read_unit(fptr, cimgrota, uimgrota, status);
      CHECK_STATUS_BREAK(*status);
      fimgrota = unit_conversion_rad(uimgrota);
      if (0.==fimgrota) {
	SIMPUT_ERROR("unknown units in IMGROTA column");
	*status=EXIT_FAILURE;
	break;
      }
    }

    char ue_min[SIMPUT_MAXSTR];
    read_unit(fptr, ce_min, ue_min, status);
    CHECK_STATUS_BREAK(*status);
    float fe_min = unit_conversion_keV(ue_min);
    if (0.==fe_min) {
      SIMPUT_ERROR("unknown units in E_MIN column");
      *status=EXIT_FAILURE;
      break;
    }

    char ue_max[SIMPUT_MAXSTR];
    read_unit(fptr, ce_max, ue_max, status);
    CHECK_STATUS_BREAK(*status);
    float fe_max = unit_conversion_keV(ue_max);
    if (0.==fe_max) {
      SIMPUT_ERROR("unknown units in E_MAX column");
      *status=EXIT_FAILURE;
      break;
    }

    char uflux[SIMPUT_MAXSTR];
    read_unit(fptr, cflux, uflux, status);
    CHECK_STATUS_BREAK(*status);
    float fflux = unit_conversion_ergpspcm2(uflux);
    if (0.==fflux) {
      SIMPUT_ERROR("unknown units in FLUX column");
      *status=EXIT_FAILURE;
      break;
    }
    // END of determine unit conversion factors.

    // Determine the number of required entries.
    long nrows;
    fits_get_num_rows(fptr, &nrows, status);
    CHECK_STATUS_BREAK(*status);
    catalog->entries  = (SimputSourceEntry**)malloc(nrows*sizeof(SimputSourceEntry*));
    CHECK_NULL_BREAK(catalog->entries, *status, 
		     "memory allocation for catalog entries failed");
    catalog->nentries = (int)nrows;

    // Allocate memory for string buffers.
    char* src_name[1]={NULL};
    char* spectrum[1]={NULL};
    char* image[1]   ={NULL};
    char* lightcur[1]={NULL};
    src_name[0]=(char*)malloc(SIMPUT_MAXSTR*sizeof(char));
    CHECK_NULL_BREAK(src_name[0], *status, 
		     "memory allocation for string buffer failed");
    spectrum[0]=(char*)malloc(SIMPUT_MAXSTR*sizeof(char));
    CHECK_NULL_BREAK(spectrum[0], *status, 
		     "memory allocation for string buffer failed");
    image[0]=(char*)malloc(SIMPUT_MAXSTR*sizeof(char));
    CHECK_NULL_BREAK(image[0], *status, 
		     "memory allocation for string buffer failed");
    lightcur[0]=(char*)malloc(SIMPUT_MAXSTR*sizeof(char));
    CHECK_NULL_BREAK(lightcur[0], *status, 
		     "memory allocation for string buffer failed");

    // Loop over all rows in the table.
    long ii;
    for (ii=0; ii<nrows; ii++) {
      int src_id=0;
      double ra=0., dec=0.;
      float imgrota=0., imgscal=1.;
      float e_min=0., e_max=0., flux=0.;
      
      // Read the data from the table.
      int anynul=0;
      fits_read_col(fptr, TUINT, csrc_id, ii+1, 1, 1, 
		    &src_id, &src_id, &anynul, status);

      if (csrc_name>0) {
	fits_read_col_str(fptr, csrc_name, ii+1, 1, 1, 
			  "", src_name, &anynul, status);
      } else {
	strcpy(src_name[0], "");
      }

      fits_read_col(fptr, TDOUBLE, cra, ii+1, 1, 1, &ra, &ra, &anynul, status);
      ra *= fra; // Convert to [rad].
      fits_read_col(fptr, TDOUBLE, cdec, ii+1, 1, 1, &dec, &dec, &anynul, status);
      dec *= fdec; // Convert to [rad].

      if (cimgrota>0) {
	fits_read_col(fptr, TFLOAT, cimgrota, ii+1, 1, 1, 
		      &imgrota, &imgrota, &anynul, status);
	imgrota *= fimgrota; // Convert to [rad].
      }
      if (cimgscal>0) {
	fits_read_col(fptr, TFLOAT, cimgscal, ii+1, 1, 1, 
		      &imgscal, &imgscal, &anynul, status);
      }
      
      fits_read_col(fptr, TFLOAT, ce_min, ii+1, 1, 1, &e_min, &e_min, &anynul, status);
      e_min *= fe_min; // Convert to [keV].
      fits_read_col(fptr, TFLOAT, ce_max, ii+1, 1, 1, &e_max, &e_max, &anynul, status);
      e_max *= fe_max; // Convert to [keV].
      fits_read_col(fptr, TFLOAT, cflux, ii+1, 1, 1, &flux, &flux, &anynul, status);
      flux  *= fflux; // Convert to [erg/s/cm**2].

      fits_read_col(fptr, TSTRING, cspectrum, ii+1, 1, 1, 
		    "", spectrum, &anynul, status);
      fits_read_col(fptr, TSTRING, cimage, ii+1, 1, 1, 
		    "", image, &anynul, status);
      fits_read_col(fptr, TSTRING, clightcur, ii+1, 1, 1, 
		    "", lightcur, &anynul, status);

      CHECK_STATUS_BREAK(*status);

      // Add a new entry to the catalog.
      catalog->entries[ii] = 
	getSimputSourceEntryV(src_id, src_name[0], ra, dec, imgrota, imgscal, 
			      e_min, e_max, flux, spectrum[0], image[0],
			      lightcur[0], status);

      CHECK_STATUS_BREAK(*status);

    }
    // END of loop over all rows in the table.

    // Release allocated memory.
    if (NULL!=src_name[0]) free(src_name[0]);
    if (NULL!=spectrum[0]) free(spectrum[0]);
    if (NULL!=image[0])    free(image[0]);
    if (NULL!=lightcur[0]) free(lightcur[0]);

    CHECK_STATUS_BREAK(*status);

  } while(0); // END of error handling loop.
  
  // Close the file.
  fits_close_file(fptr, status);

  return(catalog);
}


void saveSimputSourceCatalog(const SimputSourceCatalog* const catalog,
			     const char* const filename,
			     int* const status)
{
  fitsfile* fptr=NULL;
  
  do { // Error handling loop.

    // Check if the file already exists.
    int exists;
    fits_file_exists(filename, &exists, status);
    CHECK_STATUS_BREAK(*status);
    if (1==exists) {
      // The file already exists.
      // Open the file and check, whether it contains a source catalog.
      fits_open_file(&fptr, filename, READWRITE, status);
      CHECK_STATUS_BREAK(*status);
      
      int status2=EXIT_SUCCESS;
      fits_write_errmark();
      fits_movnam_hdu(fptr, BINARY_TBL, "SRC_CAT", 0, &status2);
      if (BAD_HDU_NUM!=status2) {
	// The file alreay contains a source catalog.
	char msg[SIMPUT_MAXSTR];
	sprintf(msg, "the file '%s' already contains a source catalog", filename);
	SIMPUT_ERROR(msg);
	*status=EXIT_FAILURE;
	break;
      }
      fits_clear_errmark();

    } else {
      // The does not exist yet.
      // Create and open a new empty FITS file.
      fits_create_file(&fptr, filename, status);
      CHECK_STATUS_BREAK(*status);
    }


    // Create a binary table.
    const int csrc_id   = 1;
    const int csrc_name = 2;
    const int cra       = 3;
    const int cdec      = 4;
    const int cimgrota  = 5;
    const int cimgscal  = 6;
    const int ce_min    = 7;
    const int ce_max    = 8;
    const int cflux     = 9;
    const int cspectrum = 10;
    const int cimage    = 11;
    const int clightcur = 12;
    char *ttype[] = { "SRC_ID", "SRC_NAME", "RA", "DEC", "IMGROTA", "IMGSCAL", 
		      "E_MIN", "E_MAX", "FLUX", "SPECTRUM", "IMAGE", "LIGHTCUR" };
    char *tform[] = { "I", "1PA", "D", "D", "E", "E", 
		      "E", "E", "E", "1PA", "1PA", "1PA" };
    char *tunit[] = { "", "", "deg", "deg", "deg", "",  
		      "keV", "keV", "erg/s/cm**2", "", "", "" };
    // Provide option to use different units?
    fits_create_tbl(fptr, BINARY_TBL, 0, 12, ttype, tform, tunit, "SRC_CAT", status);
    CHECK_STATUS_BREAK(*status);

    // Write the necessary header keywords.
    fits_write_key(fptr, TSTRING, "HDUCLASS", "HEASARC", "", status);
    fits_write_key(fptr, TSTRING, "HDUCLAS1", "SIMPUT", "", status);
    fits_write_key(fptr, TSTRING, "HDUCLAS2", "SRC_CAT", "", status);
    fits_write_key(fptr, TSTRING, "HDUVERS", "1.0.0", "", status);
    fits_write_key(fptr, TSTRING, "RADESYS", "FK5", "", status);
    float equinox=2000.0;
    fits_update_key(fptr, TFLOAT, "EQUINOX", &equinox, "", status);
    CHECK_STATUS_BREAK(*status);

    // Write the data.
    fits_insert_rows(fptr, 0, catalog->nentries, status);
    CHECK_STATUS_BREAK(*status);
    int ii;
    for (ii=0; ii<catalog->nentries; ii++) {
      fits_write_col(fptr, TUINT, csrc_id, ii+1, 1, 1, 
		     &catalog->entries[ii]->src_id, status);
      fits_write_col(fptr, TSTRING, csrc_name, ii+1, 1, 1, 
		     &catalog->entries[ii]->src_name, status);
      double ra = catalog->entries[ii]->ra*180./M_PI;
      fits_write_col(fptr, TDOUBLE, cra, ii+1, 1, 1, &ra, status);
      double dec = catalog->entries[ii]->dec*180./M_PI;
      fits_write_col(fptr, TDOUBLE, cdec, ii+1, 1, 1, &dec, status);
      float imgrota = catalog->entries[ii]->imgrota*180./M_PI;
      fits_write_col(fptr, TFLOAT, cimgrota, ii+1, 1, 1, &imgrota, status);
      fits_write_col(fptr, TFLOAT, cimgscal, ii+1, 1, 1, 
		     &catalog->entries[ii]->imgscal, status);
      fits_write_col(fptr, TFLOAT, ce_min, ii+1, 1, 1, 
		     &catalog->entries[ii]->e_min, status);
      fits_write_col(fptr, TFLOAT, ce_max, ii+1, 1, 1, 
		     &catalog->entries[ii]->e_max, status);
      fits_write_col(fptr, TFLOAT, cflux, ii+1, 1, 1, 
		     &catalog->entries[ii]->flux, status);
      fits_write_col(fptr, TSTRING, cspectrum, ii+1, 1, 1, 
		     &catalog->entries[ii]->spectrum, status);
      fits_write_col(fptr, TSTRING, cimage, ii+1, 1, 1, 
		     &catalog->entries[ii]->image, status);
      fits_write_col(fptr, TSTRING, clightcur, ii+1, 1, 1, 
		     &catalog->entries[ii]->lightcur, status);
      CHECK_STATUS_BREAK(*status);
    }
    CHECK_STATUS_BREAK(*status);
    // END of loop over all entries in the catalog.

  } while(0); // END of error handling loop.

  // Close the file.
  fits_close_file(fptr, status);
  CHECK_STATUS_VOID(*status);
}


static int check_if_btbl(const char* const filename,
			 int* const status)
{
  // Check if this is a FITS image.
  fitsfile* fptr=NULL;
  int hdutype;

  do { // Beginning of error handling loop.

    fits_open_file(&fptr, filename, READONLY, status);
    CHECK_STATUS_BREAK(*status);
    
    fits_get_hdu_type(fptr, &hdutype, status);
    CHECK_STATUS_BREAK(*status);

  } while (0); // END of error handling loop.

  if (NULL!=fptr) fits_close_file(fptr, status);

  if (BINARY_TBL==hdutype) {
    return(1);
  } else {
    return(0);
  }
}


static void read_unit(fitsfile* const fptr, const int column, 
		      char* unit, int* const status)
{
  // Read the header keyword.
  char keyword[SIMPUT_MAXSTR], comment[SIMPUT_MAXSTR];
  sprintf(keyword, "TUNIT%d", column);
  fits_read_key(fptr, TSTRING, keyword, unit, comment, status);
  CHECK_STATUS_VOID(*status);
}


static void strtolower(char* const string)
{
  int ii=0;
  while (string[ii]!='\0') {
    string[ii] = tolower(string[ii]);
    ii++;
  };
}

static float unit_conversion_rad(const char* const unit)
{
  if (0==strcmp(unit, "rad")) {
    return(1.);
  } else if (0==strcmp(unit, "deg")) {
    return(M_PI/180.);
  } else if (0==strcmp(unit, "arcmin")) {
    return(M_PI/180./60.);
  } else if (0==strcmp(unit, "arcsec")) {
    return(M_PI/180./3600.);
  } else {
    // Unknown units.
    return(0.);
  }
}


static float unit_conversion_keV(const char* const unit)
{
  if (0==strcmp(unit, "keV")) {
    return(1.);
  } else if (0==strcmp(unit, "eV")) {
    return(0.001);
  } else {
    // Unknown units.
    return(0.);
  }
}


static float unit_conversion_ergpspcm2(const char* const unit)
{
  if (0==strcmp(unit, "erg/s/cm**2")) {
    return(1.);
  } else {
    // Unknown units.
    return(0.);
  }
}



void simputSetARF(struct ARF* const arf)
{
  static_arf = arf;
}


