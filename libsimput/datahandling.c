#include "common.h"


/** Random number generator. */
static double(*static_rndgen)(int* const)=NULL;


void setSimputARF(SimputCtlg* const cat, struct ARF* const arf)
{
  cat->arf=arf;
}


/** Use the C rand() function to determine a random number between 0
    and 1. */
static double getCRand(int* const status) 
{
  double r=(double)rand()/((double)RAND_MAX+1.0);
  assert(r<1.0);
  return(r);

  // Status variable is not needed.
  (void)(*status);
}


void setSimputRndGen(double(*rndgen)(int* const))
{
  static_rndgen=rndgen;
}


/** Determine a random number between 0 and 1 with the specified
    random number generator. */
static inline double getRndNum(int* const status)
{
  // Check if a random number generator has been set.
  if (NULL==static_rndgen) {
    // If not use the C rand() generator as default.
    SIMPUT_WARNING("use C rand() as default since no random number generator "
		   "is specified");

    // Initialize random seed.
    srand(time(NULL));

    setSimputRndGen(getCRand);
  }

  // Obtain a random number.
  return(static_rndgen(status));
}


SimputSrc* getSimputSrc(SimputCtlg* const cf,
			const long row,
			int* const status)
{
  // Maximum number of sources in the cache.
  const long maxsrcs=1000000; 

  // Check if the source catalog contains a source buffer.
  if (NULL==cf->srcbuff) {
    cf->srcbuff=newSimputSrcBuffer(status);
    CHECK_STATUS_RET(*status, NULL);
  }

  // Convert the void* pointer to the source buffer into the right
  // format.
  struct SimputSrcBuffer* sb=(struct SimputSrcBuffer*)cf->srcbuff;

  // Allocate memory for the cache.
  if (NULL==sb->rowmap) {
    sb->rowmap=(long*)malloc(cf->nentries*sizeof(long));
    CHECK_NULL_RET(sb->rowmap, *status, 
		   "memory allocation for row map failed", NULL);
    long jj;
    for (jj=0; jj<cf->nentries; jj++) {
      sb->rowmap[jj]=-1;
    }
  }

  // Check if the cache already exists or whether this routine is 
  // called for the first time.
  if (NULL==sb->srcs) {
    sb->srcs=(SimputSrc**)malloc(maxsrcs*sizeof(SimputSrc*));
    CHECK_NULL_RET(sb->srcs, *status,
		   "memory allocation for source cache failed", NULL);
  }
  if (NULL==sb->rownums) {
    sb->rownums=(long*)malloc(maxsrcs*sizeof(long));
    CHECK_NULL_RET(sb->rownums, *status,
		   "memory allocation for array of row numbers failed", NULL);
  }

  // Check if the specified source index (row number) is valid.
  if ((row<=0) || (row>cf->nentries)) {
    SIMPUT_ERROR("invalid row number");
    return(NULL);
  }

  // Check if the requested row is already available in the cache.
  if (sb->rowmap[row-1]>0) {
    return(sb->srcs[sb->rowmap[row-1]]);
  }

  // The requested source is not contained in the cache.
  // Therefore we must load it from the FITS file.
  
  // Check if the cache is already full.
  if (sb->nsrcs<maxsrcs) {
    sb->csrc = sb->nsrcs;
    sb->nsrcs++;
  } else {
    sb->csrc++;
    if (sb->csrc>=maxsrcs) {
      sb->csrc=0;
    }
    // Destroy the source that is currently stored at this place 
    // in the cache.
    freeSimputSrc(&(sb->srcs[sb->csrc]));
    sb->rowmap[sb->rownums[sb->csrc]-1] = -1;
    sb->rownums[sb->csrc] = 0;
  }

  // Load the source from the FITS file.
  sb->srcs[sb->csrc]=loadSimputSrc(cf, row, status);
  CHECK_STATUS_RET(*status, sb->srcs[sb->csrc]);
  sb->rownums[sb->csrc]=row;
  sb->rowmap[row-1]=sb->csrc;

  return(sb->srcs[sb->csrc]);
}


static void getSrcTimeRef(SimputCtlg* const cat,
			  const SimputSrc* const src,
			  char* const timeref)
{
  // Initialize with an empty string.
  strcpy(timeref, "");

  // Determine the reference to the timing extension.
  if (NULL==src->timing) {
    strcpy(timeref, "");
  } else if ((0==strlen(src->timing)) || 
	     (0==strcmp(src->timing, "NULL")) ||
	     (0==strcmp(src->timing, " "))) {
    strcpy(timeref, "");
  } else {
    if ('['==src->timing[0]) {
      strcpy(timeref, cat->filepath);
      strcat(timeref, cat->filename);
    } else {
      if ('/'!=src->timing[0]) {
	strcpy(timeref, cat->filepath);
      } else {
	strcpy(timeref, "");
      }
    }
    strcat(timeref, src->timing);
  }
}


static void gauss_rndgen(double* const x, double* const y, int* const status)
{
  double sqrt_2rho=sqrt(-log(getRndNum(status))*2.);
  CHECK_STATUS_VOID(*status);
  double phi=getRndNum(status)*2.*M_PI;
  CHECK_STATUS_VOID(*status);

  *x=sqrt_2rho * cos(phi);
  *y=sqrt_2rho * sin(phi);
}


static SimputPSD* getSimputPSD(SimputCtlg* const cat,
			       char* const filename,
			       int* const status)
{
  const int maxpsds=200;

  // Check if the source catalog contains a PSD buffer.
  if (NULL==cat->psdbuff) {
    cat->psdbuff=newSimputPSDBuffer(status);
    CHECK_STATUS_RET(*status, NULL);
  }

  // Convert the void* pointer to the PSD buffer into the right
  // format.
  struct SimputPSDBuffer* sb=(struct SimputPSDBuffer*)cat->psdbuff;

  // In case there are no PSDs available at all, allocate 
  // memory for the array (storage for PSDs).
  if (NULL==sb->psds) {
    sb->psds=(SimputPSD**)malloc(maxpsds*sizeof(SimputPSD*));
    CHECK_NULL_RET(sb->psds, *status, 
		   "memory allocation for PSD buffer failed", NULL);
  }

  // Search if the requested PSD is available in the storage.
  long ii;
  for (ii=0; ii<sb->npsds; ii++) {
    // Check if the PSD is equivalent to the requested one.
    if (0==strcmp(sb->psds[ii]->fileref, filename)) {
      // If yes, return the PSD.
      return(sb->psds[ii]);
    }
  }

  // The requested PSD is not contained in the storage.
  // Therefore we must load it from the specified location.
  if (sb->npsds>=maxpsds) {
    SIMPUT_ERROR("too many PSDs in the internal storage");
    *status=EXIT_FAILURE;
    return(NULL);
  }

  // Load the PSD from the file.
  sb->psds[sb->npsds]=loadSimputPSD(filename, status);
  CHECK_STATUS_RET(*status, sb->psds[sb->npsds]);
  sb->npsds++;

  return(sb->psds[sb->npsds-1]);
}


static inline double getLCTime(const SimputLC* const lc, 
			       const long kk, 
			       const long long nperiods,
			       const double mjdref)
{
  if (NULL!=lc->time) {
    // Non-periodic light curve.
    return(lc->time[kk] + lc->timezero + (lc->mjdref-mjdref)*24.*3600.);
  } else {
    // Periodic light curve.
    double phase=lc->phase[kk] - lc->phase0 + nperiods;
    return(phase*lc->period 
	   +lc->timezero + (lc->mjdref-mjdref)*24.*3600.);
  }
}


/** Determine the index of the bin of the light curve that corresponds
    to the specified time. */
static inline long getLCBin(const SimputLC* const lc, 
			    const double time, 
			    const double mjdref,
			    long long* nperiods, 
			    int* const status)
{
  // Check if the value of MJDREF is negative. This indicates
  // that the spectrum referred to in the first bin of the light curve
  // is requested in order to calculate the source count rate.
  if (mjdref<0.) {
    return(0);
  }

  // Check if the light curve is periodic or not.
  if (NULL!=lc->time) {
    // Non-periodic light curve.

    // Check if the requested time is within the covered interval.
    if ((time<getLCTime(lc, 0, 0, mjdref)) || 
	(time>=getLCTime(lc, lc->nentries-1, 0, mjdref))) {
      char msg[SIMPUT_MAXSTR];
      sprintf(msg, "requested time (%lf MJD) is outside the "
	      "interval covered by the light curve '%s' (%lf to %lf MJD)",
	      time/24./3600. + mjdref,
	      lc->fileref,
	      getLCTime(lc, 0, 0, 0.)/24./3600., 
	      getLCTime(lc, lc->nentries-1, 0, 0.)/24./3600.);
      SIMPUT_ERROR(msg);
      *status=EXIT_FAILURE;
      return(0);
    }
    
    *nperiods=0;

  } else {
    // Periodic light curve.
    double dt=time-getLCTime(lc, 0, 0, mjdref);
    double phase=lc->phase0 + dt/lc->period;
    (*nperiods)=(long long)phase;
    if (phase<0.) {
      (*nperiods)--;
    }     
  }

  // Determine the respective index kk of the light curve (using
  // binary search).
  long lower=0, upper=lc->nentries-2, mid;
  while (upper>lower) {
    mid=(lower+upper)/2;
    if (getLCTime(lc, mid+1, *nperiods, mjdref) < time) {
      lower=mid+1;
    } else {
      upper=mid;
    }
  }
  
  return(lower);
}


static SimputLC* getSimputLC(SimputCtlg* const cat, 
			     const SimputSrc* const src,
			     char* const filename, 
			     const double prevtime, 
			     const double mjdref, 
			     int* const status)
{
  SimputLC* lc=NULL;

  // Check if the SimputLC is contained in the internal cache.
  const int maxlcs=1000;

  // Check if the source catalog contains a light curve buffer.
  if (NULL==cat->lcbuff) {
    cat->lcbuff=newSimputLCBuffer(status);
    CHECK_STATUS_RET(*status, NULL);
  }

  // Convert the void* pointer to the light curve buffer into 
  // the right format.
  struct SimputLCBuffer* lb=(struct SimputLCBuffer*)cat->lcbuff;

  // In case there are no light curves available at all, allocate 
  // memory for the array (storage for images).
  if (NULL==lb->lcs) {
    lb->lcs=(SimputLC**)malloc(maxlcs*sizeof(SimputLC*));
    CHECK_NULL_RET(lb->lcs, *status, 
		   "memory allocation for light curves failed", NULL);
  }

  // Search if the requested light curve is available in the storage.
  long ii;
  for (ii=0; ii<lb->nlcs; ii++) {
    // Check if the light curve is equivalent to the requested one.
    if (0==strcmp(lb->lcs[ii]->fileref, filename)) {
      // For a light curve created from a PSD, we also have to check,
      // whether this is the source associated to this light curve.
      if (lb->lcs[ii]->src_id>0) {
	// Check if the SRC_IDs agree.
	if (lb->lcs[ii]->src_id==src->src_id) {
	  // We have a light curve which has been produced from a PSD.
	  // Check if the requested time is covered by the light curve.
	  if (prevtime<getLCTime(lb->lcs[ii], lb->lcs[ii]->nentries-1, 
				 0, mjdref)) {
	    return(lb->lcs[ii]);
	  }
	  // If not, we have to produce a new light curve from the PSD.
	}
      } else {
	// This light curve is loaded from a file and can be re-used
	// for different sources.
	return(lb->lcs[ii]);
      }
    }
  }


  // If the LC is not contained in the cache, load it either from 
  // a file or create it from a SimputPSD.
  int timetype=getExtType(cat, filename, status);
  CHECK_STATUS_RET(*status, lc);

  if (EXTTYPE_LC==timetype) {
    // Load directly from file.
    lc=loadSimputLC(filename, status);
    CHECK_STATUS_RET(*status, lc);

  } else {
    // Create the SimputLC from a SimputPSD.
    
    // Buffer for Fourier transform.
    float* power=NULL;
    double *fftw_in=NULL, *fftw_out=NULL;

    // Length of the PSD for the FFT, which is obtained by 
    // interpolation of the input PSD.
    const long psdlen=100000000;

    do { // Error handling loop.
  
      SimputPSD* psd=getSimputPSD(cat, filename, status);
      CHECK_STATUS_BREAK(*status);
      
      // Get an empty SimputLC data structure.
      lc=newSimputLC(status);
      CHECK_STATUS_BREAK(*status);
      
      // Set the MJDREF.
      lc->mjdref=mjdref;

      // Allocate memory for the light curve.
      lc->nentries=2*psdlen;
      lc->time    =(double*)malloc(lc->nentries*sizeof(double));
      CHECK_NULL_BREAK(lc->time, *status, 
		       "memory allocation for K&R light curve failed");
      lc->flux    =(float*)malloc(lc->nentries*sizeof(float));
      CHECK_NULL_BREAK(lc->flux, *status, 
		       "memory allocation for K&R light curve failed");

      // Set the time bins of the light curve.
      lc->timezero=prevtime;
      long ii;
      for (ii=0; ii<lc->nentries; ii++) {
	lc->time[ii] = ii*1./(2.*psd->frequency[psd->nentries-1]);
      }

      // Interpolate the PSD to a uniform frequency grid.
      // The PSD is given in Miyamoto normalization. In order to get the RMS
      // right, we have to multiply each bin with df (delta frequency).
      power=(float*)malloc(psdlen*sizeof(float));
      CHECK_NULL_BREAK(power, *status, 
		       "memory allocation for PSD buffer failed");
      long jj=0;
      float delta_f=psd->frequency[psd->nentries-1]/psdlen;
      for (ii=0; ii<psdlen; ii++) {
	float frequency = (ii+1)*delta_f;
	while((frequency>psd->frequency[jj]) &&
	      (jj<psd->nentries-1)) {
	  jj++;
	}
	if (jj==0) {
	  power[ii]=0.;
	  /* frequency/psd->frequency[jj]* 
	     psd->power[jj]* 
	     delta_f;*/
	} else {
	  power[ii]=
	    (psd->power[jj-1] + 
	     (frequency-psd->frequency[jj-1])/
	     (psd->frequency[jj]-psd->frequency[jj-1])*
	     (psd->power[jj]-psd->power[jj-1])) *
	    delta_f;
	}
      }
    
      // Allocate the data structures required by the fftw routines.
      fftw_in =(double*)fftw_malloc(sizeof(double)*lc->nentries);
      CHECK_NULL_BREAK(fftw_in, *status, "memory allocation for fftw "
		       "data structure (fftw_in) failed");
      fftw_out=(double*)fftw_malloc(sizeof(double)*lc->nentries);
      CHECK_NULL_BREAK(fftw_out, *status, "memory allocation for fftw "
		       "data structure (fftw_out) failed");

      // Apply the algorithm introduced by Timmer & Koenig (1995).
      double randr, randi;
      lc->fluxscal=1.; // Set Fluxscal to 1.
      gauss_rndgen(&randr, &randi, status);
      CHECK_STATUS_RET(*status, lc);
      fftw_in[0]     =1.;
      fftw_in[psdlen]=randi*sqrt(power[psdlen-1]);
      for (ii=1; ii<psdlen; ii++) {
	gauss_rndgen(&randr, &randi, status);
	CHECK_STATUS_RET(*status, lc);
	REAL(fftw_in, ii)              =randr*0.5*sqrt(power[ii-1]);
	IMAG(fftw_in, ii, lc->nentries)=randi*0.5*sqrt(power[ii-1]);
      }

      // Perform the inverse Fourier transformation.
      fftw_plan iplan=fftw_plan_r2r_1d(lc->nentries, fftw_in, fftw_out, 
				       FFTW_HC2R, FFTW_ESTIMATE);
      fftw_execute(iplan);
      fftw_destroy_plan(iplan);
      
      // Determine the normalized rates from the FFT.
      for (ii=0; ii<lc->nentries; ii++) {
	lc->flux[ii] = (float)fftw_out[ii] /* *requ_rms/act_rms */;

	// Avoid negative fluxes (no physical meaning):
	if (lc->flux[ii]<0.) { 
	  lc->flux[ii]=0.; 
	}
      }

      // This light curve has been generated for this
      // particular source.
      lc->src_id=src->src_id;

      // Store the file reference to the timing extension for later comparisons.
      lc->fileref=
	(char*)malloc((strlen(filename)+1)*sizeof(char));
      CHECK_NULL_RET(lc->fileref, *status, 
		 "memory allocation for file reference failed", 
		 lc);
      strcpy(lc->fileref, filename);

    } while(0); // END of error handling loop.
    
    // Release allocated memory.
    if (NULL!=power) free(power);
    if (NULL!=fftw_in) fftw_free(fftw_in);
    if (NULL!=fftw_out) fftw_free(fftw_out);
    
  }

  // Check if there is still space left in the internal cache.
  if (lb->nlcs<maxlcs) {
    lb->clc=lb->nlcs;
    lb->nlcs++;
  } else {
    lb->clc++;
    if (lb->clc>=maxlcs) {
      lb->clc=0;
    }
    // Release the SimputLC that is currently stored at this place 
    // in the cache.
    freeSimputLC(&(lb->lcs[lb->clc]));
  }

  // Store the SimputLC in the internal cache.
  lb->lcs[lb->clc]=lc;

  return(lb->lcs[lb->clc]);
}


void getSimputSrcSpecRef(SimputCtlg* const cat,
			 const SimputSrc* const src,
			 const double prevtime,
			 const double mjdref,
			 char* const specref,
			 int* const status)
{
  // Initialize with an empty string.
  strcpy(specref, "");

  // Determine the timing extension.
  char timeref[SIMPUT_MAXSTR];
  getSrcTimeRef(cat, src, timeref);
  CHECK_STATUS_VOID(*status);

  int timetype=getExtType(cat, timeref, status);
  CHECK_STATUS_VOID(*status);

  if (EXTTYPE_LC==timetype) {
    // Get the respective light curve.
    SimputLC* lc=getSimputLC(cat, src, timeref, prevtime, mjdref, status);
    CHECK_STATUS_VOID(*status);

    // Check if there is a spectrum column in the light curve.
    if (NULL!=lc->spectrum) {
      // Determine the current light curve bin.
      long long nperiods;
      long bin=getLCBin(lc, prevtime, mjdref, &nperiods, status);
      CHECK_STATUS_VOID(*status);

      // Check if this is a valid spectrum is defined.
      if ((0==strcmp(lc->spectrum[bin], "NULL")) || 
	  (0==strcmp(lc->spectrum[bin], " ")) ||
	  (0==strlen(lc->spectrum[bin]))) {
	SIMPUT_ERROR("in the current implementation light curves "
		     "must not contain blank entries in a given "
		     "spectrum column");
	*status=EXIT_FAILURE;
	return;
      }

      // Copy the reference to the spectrum.
      strcpy(specref, lc->spectrum[bin]);

      // Determine the relative location with respect to the 
      // location of the light curve.
      if ('['==specref[0]) {
	char* firstbrack=strchr(timeref, '[');
	strcpy(firstbrack, specref);
	strcpy(specref, timeref);
      } else if (('/'!=specref[0])&&(NULL!=strchr(timeref, '/'))) {
	char* lastslash=strrchr(timeref, '/');
	lastslash++;
	strcpy(lastslash, specref);
	strcpy(specref, timeref);	
      }

      return;
    }
  }

  // If no light curve extension with a spectrum column is given, 
  // determine the spectrum reference directly from the source 
  // description.
  if (NULL!=src->spectrum) {   
    // Check if this is a valid spectrum reference.
    if ((0==strcmp(src->spectrum, "NULL")) || 
	(0==strcmp(src->spectrum, " ")) ||
	(0==strlen(specref))) {
      return;
    }

    strcpy(specref, src->spectrum);
    
    // Set path and file name if missing.
    if ('['==specref[0]) {
      char buffer[SIMPUT_MAXSTR];
      strcpy(buffer, cat->filepath);
      strcat(buffer, cat->filename);
      strcat(buffer, specref);
      strcpy(specref, buffer);
    } else {
      if ('/'!=specref[0]) {
	char buffer[SIMPUT_MAXSTR];
	strcpy(buffer, cat->filepath);
	strcat(buffer, specref);
	strcpy(specref, buffer);
	
      }
    }
  }
}


static void getSrcImagRef(SimputCtlg* const cat,
			  const SimputSrc* const src,
			  const double prevtime,
			  const double mjdref,
			  char* const imagref,
			  int* const status)
{
  // Initialize with an empty string.
  strcpy(imagref, "");

  // Determine the timing extension.
  char timeref[SIMPUT_MAXSTR];
  getSrcTimeRef(cat, src, timeref);
  CHECK_STATUS_VOID(*status);

  int timetype=getExtType(cat, timeref, status);
  CHECK_STATUS_VOID(*status);

  if (EXTTYPE_LC==timetype) {
    // Get the respective light curve.
    SimputLC* lc=getSimputLC(cat, src, timeref, prevtime, mjdref, status);
    CHECK_STATUS_VOID(*status);

    // Check if there is an image column in the light curve.
    if (NULL!=lc->image) {
      // Determine the current light curve bin.
      long long nperiods;
      long bin=getLCBin(lc, prevtime, mjdref, &nperiods, status);
      CHECK_STATUS_VOID(*status);

      // Copy the reference to the image.
      strcpy(imagref, lc->image[bin]);
      
      // Check if an image is defined in this light curve bin.
      if (0==strlen(imagref)) {
	SIMPUT_ERROR("in the current implementation light curves "
		     "must not contain blank entries in a given "
		     "image column");
	*status=EXIT_FAILURE;
	return;
      }

      // Determine the relative location with respect to the 
      // location of the light curve.
      if ('['==imagref[0]) {
	char* firstbrack=strchr(timeref, '[');
	strcpy(firstbrack, imagref);
	strcpy(imagref, timeref);
      } else if (('/'!=imagref[0])&&(NULL!=strchr(timeref, '/'))) {
	char* lastslash=strrchr(timeref, '/');
	lastslash++;
	strcpy(lastslash, imagref);
	strcpy(imagref, timeref);
      }
    }
  }

  // If no light curve extension with an image column is given, 
  // determine the spectrum reference directly from the source 
  // description.
  if (0==strlen(imagref)) {
    if (NULL==src->image) {
      strcpy(imagref, "");
    } else {
      strcpy(imagref, src->image);
    }
  }

  // Check if this is a valid image reference.
  if ((0==strcmp(src->image, "NULL")) || (0==strcmp(src->image, " "))) {
    strcpy(imagref, "");
  }
  if (0==strlen(imagref)) {
    return;
  }

  // Set path and file name if missing.
  if ('['==imagref[0]) {
    char buffer[SIMPUT_MAXSTR];
    strcpy(buffer, cat->filepath);
    strcat(buffer, cat->filename);
    strcat(buffer, imagref);
    strcpy(imagref, buffer);
  } else {
    if ('/'!=imagref[0]) {
      char buffer[SIMPUT_MAXSTR];
      strcpy(buffer, cat->filepath);
      strcat(buffer, imagref);
      strcpy(imagref, buffer);
    }
  }
}


static SimputMIdpSpec* getSimputMIdpSpec(SimputCtlg* const cat,
					 const char* const filename,
					 int* const status)
{
  // Check if the source catalog contains an MIdpSpec buffer.
  if (NULL==cat->midpspecbuff) {
    cat->midpspecbuff=newSimputMIdpSpecBuffer(status);
    CHECK_STATUS_RET(*status, NULL);
  }

  // Convert the void* pointer to the spectrum buffer into the right
  // format.
  struct SimputMIdpSpecBuffer* sb=
    (struct SimputMIdpSpecBuffer*)cat->midpspecbuff;

  // In case there are no spectra available at all, allocate 
  // memory for the array (storage for spectra).
  if (NULL==sb->spectra) {
    sb->spectra=
      (SimputMIdpSpec**)malloc(MAXMIDPSPEC*sizeof(SimputMIdpSpec*));
    CHECK_NULL_RET(sb->spectra, *status, 
		   "memory allocation for spectra failed", NULL);
  }

  // Search if the required spectrum is available in the storage.
  long ii;
  for (ii=0; ii<sb->nspectra; ii++) {
    // Check if the spectrum is equivalent to the required one.
    if (0==strcmp(sb->spectra[ii]->fileref, filename)) {
      // If yes, return the spectrum.
      return(sb->spectra[ii]);
    }
  }

  // The required spectrum is not contained in the storage.
  // Therefore it must be loaded from the specified location.

  // Check if there is still space left in the spectral storage buffer.
  if (sb->nspectra<MAXMIDPSPEC) {
    sb->cspectrum=sb->nspectra;
    sb->nspectra++;
  } else {
    sb->cspectrum++;
    if (sb->cspectrum>=MAXMIDPSPEC) {
      sb->cspectrum=0;
    }
    // Release the spectrum that is currently stored at this place in the
    // storage buffer.
    freeSimputMIdpSpec(&(sb->spectra[sb->cspectrum]));
  }

  // Load the mission-independent spectrum.
  sb->spectra[sb->cspectrum]=loadSimputMIdpSpec(filename, status);
  CHECK_STATUS_RET(*status, sb->spectra[sb->cspectrum]);

  return(sb->spectra[sb->cspectrum]);
}


SimputMIdpSpec* getSimputSrcMIdpSpec(SimputCtlg* const cat,
				     const SimputSrc* const src,
				     const double prevtime,
				     const double mjdref,
				     int* const status)
{
  char specref[SIMPUT_MAXSTR];
  getSimputSrcSpecRef(cat, src, prevtime, mjdref, specref, status);
  CHECK_STATUS_RET(*status, 0);

  int spectype=getExtType(cat, specref, status);
  CHECK_STATUS_RET(*status, 0);

  if (EXTTYPE_MIDPSPEC==spectype) {
    // Determine the spectrum.
    SimputMIdpSpec* spec=getSimputMIdpSpec(cat, specref, status);
    CHECK_STATUS_RET(*status, 0);

    return(spec);

  } else {
    SIMPUT_ERROR("source does not refer to a mission-independent spectrum");
    *status=EXIT_FAILURE;
    return(NULL);
  }
}


static inline void getMIdpSpecEbounds(const SimputMIdpSpec* const spec,
				      const long idx,
				      float* const emin, 
				      float* const emax)
{
  // Determine the lower boundary.
  if (idx>0) {
    *emin=0.5*(spec->energy[idx]+spec->energy[idx-1]);
  } else {
    *emin=spec->energy[idx];
  }

  // Determine the upper boundary.
  if (idx<spec->nentries-1) {
    *emax=0.5*(spec->energy[idx+1]+spec->energy[idx]);
  } else {
    *emax=spec->energy[idx];
  }
}


/** Convolve the given mission-independent spectrum with the
    instrument ARF. The product of this process is the spectral
    probability distribution binned to the energy grid of the ARF. */
static SimputSpec* convSimputMIdpSpecWithARF(SimputCtlg* const cat, 
					     SimputMIdpSpec* const midpspec, 
					     int* const status)
{
  SimputSpec* spec=NULL;

  // Check if the ARF is defined.
  CHECK_NULL_RET(cat->arf, *status, "instrument ARF undefined", spec);

  // Allocate memory.
  spec=newSimputSpec(status);
  CHECK_STATUS_RET(*status, spec);
  spec->distribution=
    (double*)malloc(cat->arf->NumberEnergyBins*sizeof(double));
  CHECK_NULL_RET(spec->distribution, *status,
		 "memory allocation for spectral distribution failed", spec);

  // Loop over all bins of the ARF.
  long ii, jj=0;
  int warning_printed=0; // Flag whether warning has been printed.
  for (ii=0; ii<cat->arf->NumberEnergyBins; ii++) {
    // Initialize with 0.
    spec->distribution[ii]=0.;

    // Lower boundary of the current bin.
    float lo=cat->arf->LowEnergy[ii];

    // Loop over all spectral points within the ARF bin.
    int finished=0;
    do {
      // Determine the next spectral point.
      float spec_emin=0., spec_emax=0.;
      for ( ; jj<midpspec->nentries; jj++) {
	getMIdpSpecEbounds(midpspec, jj, &spec_emin, &spec_emax);
	if (spec_emax>lo) break;
      }
      
      // Check special cases.
      if ((0==jj) && (spec_emin>cat->arf->LowEnergy[ii])) {
	if (0==warning_printed) {
	  char msg[SIMPUT_MAXSTR];
	  sprintf(msg, "the spectrum '%s' does not cover the "
		  "full energy range of the ARF", midpspec->fileref);
	  SIMPUT_WARNING(msg);
	  warning_printed=1;
	}
	if (spec_emin>cat->arf->HighEnergy[ii]) break;

      } else if (jj==midpspec->nentries) {
	if (0==warning_printed) {
	  char msg[SIMPUT_MAXSTR];
	  sprintf(msg, "the spectrum '%s' does not cover the "
		  "full energy range of the ARF", midpspec->fileref);
	  SIMPUT_WARNING(msg);
	  warning_printed=1;
	}
	break;
      }
      
      // Upper boundary of the current bin.
      float hi;
      if (spec_emax<=cat->arf->HighEnergy[ii]) {
	hi=spec_emax;
      } else {
	hi=cat->arf->HighEnergy[ii];
	finished=1;
      }

      // Add to the spectral probability density.
      spec->distribution[ii]+=
	(hi-lo)*cat->arf->EffArea[ii]*midpspec->pflux[jj];
      
      // Increase the lower boundary.
      lo=hi;

    } while (0==finished);

    // Create the spectral distribution function 
    // normalized to the total photon number [photons]. 
    if (ii>0) {
      spec->distribution[ii]+=spec->distribution[ii-1];
    }
  } // Loop over all ARF bins.


  // Copy the file reference to the spectrum for later comparisons.
  spec->fileref=
    (char*)malloc((strlen(midpspec->fileref)+1)*sizeof(char));
  CHECK_NULL_RET(spec->fileref, *status, 
		 "memory allocation for file reference failed", 
		 spec);
  strcpy(spec->fileref, midpspec->fileref);

  return(spec);
}


static SimputSpec* getSimputSpec(SimputCtlg* const cat,
				 const char* const filename,
				 int* const status)
{
  // Maximum number of spectra in the cache.
  const int maxspec=30000; 

  // Check if the source catalog contains a spectrum buffer.
  if (NULL==cat->specbuff) {
    cat->specbuff=newSimputSpecBuffer(status);
    CHECK_STATUS_RET(*status, NULL);
  }

  // Convert the void* pointer to the spectrum buffer into the right
  // format.
  struct SimputSpecBuffer* sb=
    (struct SimputSpecBuffer*)cat->specbuff;

  // In case there are no spectra available at all, allocate 
  // memory for the array (storage for spectra).
  if (NULL==sb->spectra) {
    sb->spectra=
      (SimputSpec**)malloc(maxspec*sizeof(SimputSpec*));
    CHECK_NULL_RET(sb->spectra, *status, 
		   "memory allocation for spectra failed", NULL);
  }

  // Search if the required spectrum is available in the storage.
  long ii;
  for (ii=0; ii<sb->nspectra; ii++) {
    // Check if the spectrum is equivalent to the required one.
    if (0==strcmp(sb->spectra[ii]->fileref, filename)) {
      // If yes, return the spectrum.
      return(sb->spectra[ii]);
    }
  }

  // The required spectrum is not contained in the storage.
  // Therefore we must determine it from the referred mission-
  // independent spectrum.

  // Check if there is still space left in the spectral storage buffer.
  if (sb->nspectra<maxspec) {
    sb->cspectrum=sb->nspectra;
    sb->nspectra++;
  } else {
    sb->cspectrum++;
    if (sb->cspectrum>=maxspec) {
      sb->cspectrum=0;
    }
    // Release the spectrum that is currently stored at this place in the
    // storage buffer.
    freeSimputSpec(&(sb->spectra[sb->cspectrum]));
  }

  // Obtain the mission-independent spectrum.
  SimputMIdpSpec* midpspec=getSimputMIdpSpec(cat, filename, status);
  CHECK_STATUS_RET(*status, sb->spectra[sb->cspectrum]);

  // Convolve it with the ARF.
  sb->spectra[sb->cspectrum]=
    convSimputMIdpSpecWithARF(cat, midpspec, status);
  CHECK_STATUS_RET(*status, sb->spectra[sb->cspectrum]);

  return(sb->spectra[sb->cspectrum]);
}


static inline double rndexp(const double avgdist, int* const status)
{
  assert(avgdist>0.);

  double rand;
  do {
    rand=getRndNum(status);
    CHECK_STATUS_RET(*status, 0.);
    assert(rand>=0.);
  } while (rand==0.);

  return(-log(rand)*avgdist);
}


/** Determine the time corresponding to a particular light curve bin
    [s]. The function takes into account, whether the light curve is
    periodic or not. For periodic light curves the specified number of
    periods is added to the time value. For non-periodic light curves
    the nperiod parameter is neglected. The returned value includes
    the MJDREF and TIMEZERO contributions. */
static inline double getKRLCTime(const SimputKRLC* const lc, 
				 const long kk, 
				 const long long nperiods,
				 const double mjdref)
{
  if (NULL!=lc->time) {
    // Non-periodic light curve.
    return(lc->time[kk] + lc->timezero + (lc->mjdref-mjdref)*24.*3600.);
  } else {
    // Periodic light curve.
    double phase=lc->phase[kk] - lc->phase0 + nperiods;
    return(phase*lc->period 
	   +lc->timezero + (lc->mjdref-mjdref)*24.*3600.);
  }
}


/** Determine the index of the bin of the K&R light curve that
    corresponds to the specified time. */
static inline long getKRLCBin(const SimputKRLC* const lc, 
			      const double time, 
			      const double mjdref,
			      long long* nperiods, 
			      int* const status)
{
  // Check if the light curve is periodic or not.
  if (NULL!=lc->time) {
    // Non-periodic light curve.

    // Check if the requested time is within the covered interval.
    if ((time<getKRLCTime(lc, 0, 0, mjdref)) || 
	(time>=getKRLCTime(lc, lc->nentries-1, 0, mjdref))) {
      char msg[SIMPUT_MAXSTR];
      sprintf(msg, "requested time (%lf MJD) is outside the "
	      "interval covered by the light curve '%s' (%lf to %lf MJD)",
	      time/24./3600. + mjdref,
	      lc->fileref,
	      getKRLCTime(lc, 0, 0, 0.)/24./3600., 
	      getKRLCTime(lc, lc->nentries-1, 0, 0.)/24./3600.);
      SIMPUT_ERROR(msg);
      *status=EXIT_FAILURE;
      return(0);
    }
    
    *nperiods=0;

  } else {
    // Periodic light curve.
    double dt=time-getKRLCTime(lc, 0, 0, mjdref);
    double phase=lc->phase0 + dt/lc->period;
    (*nperiods)=(long long)phase;
    if (phase<0.) {
      (*nperiods)--;
    }     
  }

  // Determine the respective index kk of the light curve (using
  // binary search).
  long lower=0, upper=lc->nentries-2, mid;
  while (upper>lower) {
    mid=(lower+upper)/2;
    if (getKRLCTime(lc, mid+1, *nperiods, mjdref) < time) {
      lower=mid+1;
    } else {
      upper=mid;
    }
  }
  
  return(lower);
}


/** Return the requested Klein & Roberts light curve. Keeps a certain
    number of them in an internal storage. If the requested Klein &
    Roberts light curve is not located in the internal storage, it is
    obtained from the light curve/PSD referred to by the timing
    reference of the source. If the the source does not refer to a
    timing extension (i.e. it is a source with constant brightness)
    the function return value is NULL. */
static SimputKRLC* getSimputKRLC(SimputCtlg* const cat, 
				 const SimputSrc* const src,
				 char* const timeref,
				 const double time, 
				 const double mjdref,
				 int* const status)
{
  // Maximum number of Klein & Roberts light curves in the
  // internal storage.
  const int maxkrlcs=10; 

  // Check if the source catalog contains a Klein & Robert 
  // light curve buffer.
  if (NULL==cat->krlcbuff) {
    cat->krlcbuff=newSimputKRLCBuffer(status);
    CHECK_STATUS_RET(*status, NULL);
  }

  // Convert the void* pointer to the Klein & Roberts light curve 
  // buffer into the right format.
  struct SimputKRLCBuffer* sb=(struct SimputKRLCBuffer*)cat->krlcbuff;

  // In case there are no Klein & Roberts light curves available at all, 
  // allocate memory for the array (storage for light curves).
  if (NULL==sb->krlcs) {
    sb->krlcs=(SimputKRLC**)malloc(maxkrlcs*sizeof(SimputKRLC*));
    CHECK_NULL_RET(sb->krlcs, *status, 
		   "memory allocation for Klein & Roberts light curve "
		   "cache failed", NULL);
  }

  // Check if the requested K&R light curve is available in the storage.
  long ii;
  for (ii=0; ii<sb->nkrlcs; ii++) {
    // Check if the light curve is equivalent to the requested one.
    if (0==strcmp(sb->krlcs[ii]->fileref, timeref)) {
      // For a K&R light curve created from a PSD, we also have to check,
      // whether this is the source associated to this light curve.
      if (sb->krlcs[ii]->src_id>0) {
	// Check if the SRC_IDs agree.
	if (sb->krlcs[ii]->src_id==src->src_id) {
	  // We have a K&R light curve which has been produced from a PSD.
	  // Check if the requested time is covered by the K&R light curve.
	  if (time<getKRLCTime(sb->krlcs[ii], sb->krlcs[ii]->nentries-1, 
			       0, mjdref)) {
	    return(sb->krlcs[ii]);
	  }
	  // If not, we have to produce a new K&R light curve from the PSD.
	}
      } else {
	// This K&R light curve is loaded from a file and can be re-used
	// for different sources.
	return(sb->krlcs[ii]);
      }
    }
  }

  // The requested K&R light curve is not contained in the storage.
  // Therefore we must load it from the specified reference.

  // Check if there is still space left in the light curve storage.
  if (sb->nkrlcs<maxkrlcs) {
    sb->ckrlc = sb->nkrlcs;
    sb->nkrlcs++;
  } else {
    sb->ckrlc++;
    if (sb->ckrlc>=maxkrlcs) {
      sb->ckrlc=0;
    }
    // Release the light curve that is currently stored at this place
    // in the cache.
    freeSimputKRLC(&(sb->krlcs[sb->ckrlc]));
  }

  // Obtain the K&R light curve from the fundamental data structure.

  // Get the underlying light curve.
  SimputLC* lc=getSimputLC(cat, src, timeref, time, mjdref, status);
  CHECK_STATUS_RET(*status, NULL);

  // Memory allocation.
  SimputKRLC* krlc=newSimputKRLC(status);
  CHECK_STATUS_RET(*status, krlc);

  krlc->nentries=lc->nentries;

  if (NULL!=lc->time) {
    krlc->time=(double*)malloc(krlc->nentries*sizeof(double));
    CHECK_NULL_RET(krlc->time, *status, 
		   "memory allocation for K&R light curve failed", krlc);
  } else {
    krlc->phase=(double*)malloc(krlc->nentries*sizeof(double));
    CHECK_NULL_RET(krlc->phase, *status, 
		   "memory allocation for K&R light curve failed", krlc);
  }    
  krlc->a=(double*)malloc(krlc->nentries*sizeof(double));
  CHECK_NULL_RET(krlc->a, *status, 
		 "memory allocation for K&R light curve failed", krlc);
  krlc->b=(double*)malloc(krlc->nentries*sizeof(double));
  CHECK_NULL_RET(krlc->b, *status, 
		 "memory allocation for K&R light curve failed", krlc);

  // Copy values.
  krlc->mjdref  =lc->mjdref;
  krlc->timezero=lc->timezero;
  krlc->phase0  =lc->phase0;
  krlc->period  =lc->period;
  for (ii=0; ii<krlc->nentries; ii++) {
    if (NULL!=lc->time) {
      krlc->time[ii]=lc->time[ii];
    } else {
      krlc->phase[ii]=lc->phase[ii];
    }
  }

  // Determine the auxiliary values for the K&R light curve 
  // (including FLUXSCAL).
  for (ii=0; ii<krlc->nentries-1; ii++) {
    double dt;
    if (NULL!=krlc->time) {
      // Non-periodic light curve.
      dt=krlc->time[ii+1]-krlc->time[ii];
    } else {
      // Periodic light curve.
      dt=(krlc->phase[ii+1]-krlc->phase[ii])*krlc->period;
    }
    krlc->a[ii] = (lc->flux[ii+1]-lc->flux[ii])	
      /dt /lc->fluxscal;
    krlc->b[ii] = lc->flux[ii]/lc->fluxscal; 
  }
  krlc->a[krlc->nentries-1]=0.;
  krlc->b[krlc->nentries-1]=lc->flux[lc->nentries-1]/lc->fluxscal;

  // Determine the extension type of the referred HDU.
  int timetype=getExtType(cat, timeref, status);
  CHECK_STATUS_RET(*status, NULL);
  if (EXTTYPE_PSD==timetype) {
    // The new KRLC is assigned to this particular source and 
    // cannot be re-used for others.
    krlc->src_id=src->src_id;
  }

  // Store the file reference to the timing extension for 
  // later comparisons.
  krlc->fileref= 
    (char*)malloc((strlen(timeref)+1)*sizeof(char));
  CHECK_NULL_RET(krlc->fileref, *status, 
		 "memory allocation for file reference failed", 
		 krlc);
  strcpy(krlc->fileref, timeref);
   
  // Store the K&R lc in the cache.
  sb->krlcs[sb->ckrlc]=krlc;

  return(sb->krlcs[sb->ckrlc]);
}


/** Return the requested image. Keeps a certain number of images in an
    internal storage. If the requested image is not located in the
    internal storage, it is loaded from the reference given in the
    source catalog. */
static SimputImg* getSimputImg(SimputCtlg* const cat,
			       char* const filename,
			       int* const status)
{
  const int maximgs=200;

  // Check if the source catalog contains an image buffer.
  if (NULL==cat->imgbuff) {
    cat->imgbuff=newSimputImgBuffer(status);
    CHECK_STATUS_RET(*status, NULL);
  }

  // Convert the void* pointer to the image buffer into the right
  // format.
  struct SimputImgBuffer* sb=(struct SimputImgBuffer*)cat->imgbuff;

  // In case there are no images available at all, allocate 
  // memory for the array (storage for images).
  if (NULL==sb->imgs) {
    sb->imgs=(SimputImg**)malloc(maximgs*sizeof(SimputImg*));
    CHECK_NULL_RET(sb->imgs, *status, 
		   "memory allocation for images failed", NULL);
  }

  // Search if the requested image is available in the storage.
  long ii;
  for (ii=0; ii<sb->nimgs; ii++) {
    // Check if the image is equivalent to the requested one.
    if (0==strcmp(sb->imgs[ii]->fileref, filename)) {
      // If yes, return the image.
      return(sb->imgs[ii]);
    }
  }

  // The requested image is not contained in the storage.
  // Therefore we must load it from the specified location.
  if (sb->nimgs>=maximgs) {
    SIMPUT_ERROR("too many images in the internal storage");
    *status=EXIT_FAILURE;
    return(NULL);
  }

  // Load the image from the file.
  sb->imgs[sb->nimgs]=loadSimputImg(filename, status);
  CHECK_STATUS_RET(*status, sb->imgs[sb->nimgs]);
  sb->nimgs++;
  
  return(sb->imgs[sb->nimgs-1]);
}


static void p2s(struct wcsprm* const wcs,
		const double px, 
		const double py,
		double* sx, 
		double* sy,
		int* const status)
{
  double pixcrd[2]={ px, py };
  double imgcrd[2], world[2];
  double phi, theta;
  
  // If CUNIT is set to 'degree', change this to 'deg'.
  // Otherwise the WCSlib will not work properly.
  if (0==strcmp(wcs->cunit[0], "degree  ")) {
    strcpy(wcs->cunit[0], "deg");
  }
  if (0==strcmp(wcs->cunit[1], "degree  ")) {
    strcpy(wcs->cunit[1], "deg");
  }

  // Perform the transform using WCSlib.
  int retval=wcsp2s(wcs, 1, 2, pixcrd, imgcrd, &phi, &theta, world, status);
  CHECK_STATUS_VOID(*status);
  if (0!=retval) {
    *status=EXIT_FAILURE;
    SIMPUT_ERROR("WCS transformation failed");
    return;
  }
  
  // Convert to [rad].
  *sx=world[0]*M_PI/180.;
  *sy=world[1]*M_PI/180.;
}


/** Return the requested photon lists. Keeps a certain number of
    photon lists in an internal storage. If the requested photon list
    is not located in the internal storage, it is obtained from the
    reference given in the source catalog. */
static SimputPhList* getSimputPhList(SimputCtlg* const cat,
				     char* const filename,
				     int* const status)
{
  const int maxphls=200;

  // Check if the source catalog contains a photon list buffer.
  if (NULL==cat->phlistbuff) {
    cat->phlistbuff=newSimputPhListBuffer(status);
    CHECK_STATUS_RET(*status, NULL);
  }

  // Convert the void* pointer to the photon list buffer into the right
  // format.
  struct SimputPhListBuffer* pb=(struct SimputPhListBuffer*)cat->phlistbuff;

  // In case there are no photon lists available at all, allocate 
  // memory for the array (storage for photon lists).
  if (NULL==pb->phls) {
    pb->phls=(SimputPhList**)malloc(maxphls*sizeof(SimputPhList*));
    CHECK_NULL_RET(pb->phls, *status, 
		   "memory allocation for photon lists failed", NULL);
  }

  // Search if the requested photon list is available in the storage.
  long ii;
  for (ii=0; ii<pb->nphls; ii++) {
    // Check if the photon list is equivalent to the requested one.
    if (0==strcmp(pb->phls[ii]->fileref, filename)) {
      // If yes, return the photon list.
      return(pb->phls[ii]);
    }
  }

  // The requested photon list is not contained in the storage.
  // Therefore we must open it from the specified location.
  if (pb->nphls>=maxphls) {
    SIMPUT_ERROR("too many photon lists in the internal storage");
    *status=EXIT_FAILURE;
    return(NULL);
  }

  // Open the photon list from the file.
  pb->phls[pb->nphls]=openSimputPhList(filename, READONLY, status);
  CHECK_STATUS_RET(*status, pb->phls[pb->nphls]);
  pb->nphls++;
   
  return(pb->phls[pb->nphls-1]);
}


float getSimputMIdpSpecBandFlux(SimputMIdpSpec* const spec,
				const float emin, 
				const float emax)
{
  // Return value.
  float flux=0.;

  long ii;
  for (ii=0; ii<spec->nentries; ii++) {
    float binmin, binmax;
    getMIdpSpecEbounds(spec, ii, &binmin, &binmax);
    if ((emin<binmax) && (emax>binmin)) {
      float min=MAX(binmin, emin);
      float max=MIN(binmax, emax);
      assert(max>min);
      flux+=(max-min)*spec->pflux[ii]*spec->energy[ii];
    }
  }

  // Convert units of 'flux' from [keV/cm^2]->[erg/cm^2].
  flux*=keV2erg;

  return(flux);
}


float getSimputPhotonRate(SimputCtlg* const cat,
			  SimputSrc* const src,
			  const double prevtime, 
			  const double mjdref,
			  int* const status)
{
  // Check if the photon rate has already been determined before.
  if (NULL==src->phrate) {
    // Obtain the spectrum.
    char specref[SIMPUT_MAXSTR];
    getSimputSrcSpecRef(cat, src, prevtime, mjdref, specref, status);
    CHECK_STATUS_RET(*status, 0.);

    int spectype=getExtType(cat, specref, status);
    CHECK_STATUS_RET(*status, 0.);

    // Check if the ARF is defined.
    if (NULL==cat->arf) {
      *status=EXIT_FAILURE;
      SIMPUT_ERROR("ARF not found");
      return(0.);
    }

    if (EXTTYPE_MIDPSPEC==spectype) {
      SimputMIdpSpec* midpspec=getSimputMIdpSpec(cat, specref, status);
      CHECK_STATUS_RET(*status, 0.);

      // Flux in the reference energy band.
      float refband_flux=
	getSimputMIdpSpecBandFlux(midpspec, src->e_min, src->e_max);

      SimputSpec* spec=getSimputSpec(cat, specref, status);
      CHECK_STATUS_RET(*status, 0.);
      
      // Store the determined photon rate in the source data structure
      // for later use.
      // Allocate memory.
      src->phrate=(float*)malloc(sizeof(float));
      CHECK_NULL_RET(src->phrate, *status,
		     "memory allocation for photon rate buffer failed", 0.);
      *(src->phrate)=
	src->eflux / refband_flux * 
	(float)(spec->distribution[cat->arf->NumberEnergyBins-1]);
      
    } else if (EXTTYPE_PHLIST==spectype) {

      // Get the photon list.
      SimputPhList* phl=getSimputPhList(cat, specref, status);
      CHECK_STATUS_RET(*status, 0.);
      
      // Determine the flux in the reference energy band
      // and the reference number of photons after weighing
      // with the instrument ARF.
      double refband_flux=0.; // [erg]
      double refnumber=0.; // [photons*cm^2]
      const long buffsize=10000;
      long ii;
      for (ii=0; ii*buffsize<phl->nphs; ii++) {
	// Read a block of photons.
	int anynul=0;
	long nphs=MIN(buffsize, phl->nphs-(ii*buffsize));
	float buffer[buffsize];
	fits_read_col(phl->fptr, TFLOAT, phl->cenergy, ii*buffsize+1, 
		      1, nphs, NULL, buffer, &anynul, status);
	if (EXIT_SUCCESS!=*status) {
	  SIMPUT_ERROR("failed reading unit of energy column in photon list");
	  return(0.);
	}

	// Determine the illuminated energy in the
	// reference energy band.
	long jj;
	for (jj=0; jj<nphs; jj++) {
	  float energy=buffer[jj]*phl->fenergy;
	  if ((energy>=src->e_min)&&(energy<=src->e_max)) {
	    refband_flux+=energy*keV2erg;
	  }

	  // Determine the ARF value for this energy.
	  long upper=cat->arf->NumberEnergyBins-1, lower=0, mid;
	  while (upper>lower) {
	    mid=(lower+upper)/2;
	    if (cat->arf->HighEnergy[mid]<energy) {
	      lower=mid+1;
	    } else {
	      upper=mid;
	    }
	  }
	  refnumber+=cat->arf->EffArea[lower];	  
	}
	// END of loop over all photons in the buffer.
      }

      // Store the determined photon rate in the source data structure
      // for later use.
      // Allocate memory.
      src->phrate=(float*)malloc(sizeof(float));
      CHECK_NULL_RET(src->phrate, *status,
		     "memory allocation for photon rate buffer failed", 0.);
      *(src->phrate)=src->eflux / refband_flux * refnumber;

    } else {
      SIMPUT_ERROR("could not find valid spectrum extension");
      *status=EXIT_FAILURE;
      return(0.);
    }
  }
  
  return(*(src->phrate));
}


int getSimputPhotonTime(SimputCtlg* const cat,
			SimputSrc* const src,
			double prevtime,
			const double mjdref,
			double* const nexttime,
			int* const status)
{
  // Determine the time of the next photon.
  // Determine the reference to the timing extension.
  char timeref[SIMPUT_MAXSTR];
  getSrcTimeRef(cat, src, timeref);
  CHECK_STATUS_RET(*status, 0);

  // Determine the average photon rate.
  float avgrate=getSimputPhotonRate(cat, src, prevtime, mjdref, status);
  CHECK_STATUS_RET(*status, 0);
  
  // Check if the rate is 0.
  if (0.==avgrate) {
    return(1);
  }
  assert(avgrate>0.);
    
  // Check if a timing extension has been specified.
  if (0==strlen(timeref)) {
    // The source has a constant brightness.
    *nexttime=prevtime+rndexp((double)1./avgrate, status);
    CHECK_STATUS_RET(*status, 0);
    return(0);

  } else {
    // The source has a time-variable brightness.
    int timetype=getExtType(cat, timeref, status);
    CHECK_STATUS_RET(*status, 0);

    // Check if the timing reference points to a photon list.
    if (EXTTYPE_PHLIST==timetype) {
      *status=EXIT_FAILURE;
      SIMPUT_ERROR("photon lists are currently not supported "
		   "for timing extensions");
      return(0);
    }

    // Get the light curve.
    SimputKRLC* lc=getSimputKRLC(cat, src, timeref, prevtime, mjdref, status);
    CHECK_STATUS_RET(*status, 0);

    // Determine the photon time according to the light curve.
    // The light curve is a piece-wise constant function, so the
    // general algorithm proposed by Klein & Roberts has to 
    // be applied.
    // Step 1 in the algorithm.
    double u=getRndNum(status);
    CHECK_STATUS_RET(*status, 0);

    // Determine the respective index kk of the light curve.
    long long nperiods=0;
    long kk=getKRLCBin(lc, prevtime, mjdref, &nperiods, status);
    CHECK_STATUS_RET(*status, 0);

    while ((kk<lc->nentries-1)||(lc->src_id>0)) {

      // If the end of the light curve is reached, check if it has
      // been produced from a PSD. In that case one can create new one.
      if ((kk>=lc->nentries-1)&&(lc->src_id>0)) {
	lc=getSimputKRLC(cat, src, timeref, prevtime, mjdref, status);
	CHECK_STATUS_RET(*status, 0);
	kk=getKRLCBin(lc, prevtime, mjdref, &nperiods, status);
	CHECK_STATUS_RET(*status, 0);
      }

      // Determine the relative time within the kk-th interval 
      // (i.e., t=0 lies at the beginning of the kk-th interval).
      double t        =prevtime-(getKRLCTime(lc, kk, nperiods, mjdref));
      double stepwidth=
	getKRLCTime(lc, kk+1, nperiods, mjdref)-
	getKRLCTime(lc, kk  , nperiods, mjdref);

      // Step 2 in the algorithm.
      double uk=1.-exp((-lc->a[kk]/2.*(pow(stepwidth,2.)-pow(t,2.))
			-lc->b[kk]*(stepwidth-t))*avgrate);
      // Step 3 in the algorithm.
      if (u<=uk) {
	if (fabs(lc->a[kk]*stepwidth)>fabs(lc->b[kk]*1.e-6)) { 
	  // Instead of checking if a_kk = 0., check, whether its product 
	  // with the interval length is a very small number in comparison 
	  // to b_kk. If a_kk * stepwidth is much smaller than b_kk, the 
	  // rate in the interval can be assumed to be approximately constant.
	  *nexttime=
	    getKRLCTime(lc, kk, nperiods, mjdref) +
	    (-lc->b[kk]+sqrt(pow(lc->b[kk],2.) + pow(lc->a[kk]*t,2.) + 
			     2.*lc->a[kk]*lc->b[kk]*t - 
			     2.*lc->a[kk]*log(1.-u)/avgrate)
	     )/lc->a[kk];
	  return(0);
	  
	} else { // a_kk == 0
	  *nexttime=prevtime-log(1.-u)/(lc->b[kk]*avgrate);
	  return(0);
	}

      } else {
	// Step 4 (u > u_k).
	u=(u-uk)/(1-uk);
	kk++;
	if ((kk>=lc->nentries-1)&&(NULL!=lc->phase)) {
	  kk=0;
	  nperiods++;
	}
	prevtime=getKRLCTime(lc, kk, nperiods, mjdref);
      }
    }
    // END of while (kk < lc->nentries).
    
    // The range of the light curve has been exceeded.
    // So the routine has failed to determine a photon time.
    return(1);
  }
}


void getSimputPhFromPhList(const SimputCtlg* const cat,
			   SimputPhList* const phl, 
			   float* const energy, 
			   double* const ra, 
			   double* const dec, 
			   int* const status)
{
  // Determine the maximum value of the instrument ARF.
  if (0.==phl->refarea) {
    long kk;
    for (kk=0; kk<cat->arf->NumberEnergyBins; kk++) {
      if (cat->arf->EffArea[kk]>phl->refarea) {
	phl->refarea=cat->arf->EffArea[kk];
      }
    }
  }
  
  while(1) {
    // Determine a random photon within the list.
    long ii=(long)(getRndNum(status)*phl->nphs);
    CHECK_STATUS_VOID(*status);

    // Read the photon energy.
    int anynul=0;
    fits_read_col(phl->fptr, TFLOAT, phl->cenergy, ii+1, 1, 1, 
		  NULL, energy, &anynul, status);
    if (EXIT_SUCCESS!=*status) {
      SIMPUT_ERROR("failed reading photon energy from photon list");
      return;
    }
    *energy *=phl->fenergy;

    // Determine the ARF value for the photon energy.
    long upper=cat->arf->NumberEnergyBins-1, lower=0, mid;
    while (upper>lower) {
      mid=(lower+upper)/2;
      if (cat->arf->HighEnergy[mid]<*energy) {
	lower=mid+1;
      } else {
	upper=mid;
      }
    }
    
    // Randomly determine according to the effective area
    // of the instrument, whether this photon is seen or not.
    double r=getRndNum(status);
    CHECK_STATUS_VOID(*status);
    if (r<cat->arf->EffArea[lower]/phl->refarea) {
      // Read the position of the photon.
      fits_read_col(phl->fptr, TDOUBLE, phl->cra, ii+1, 1, 1, 
		    NULL, ra, &anynul, status);      
      if (EXIT_SUCCESS!=*status) {
	SIMPUT_ERROR("failed reading right ascension from photon list");
	return;
      }
      *ra *=phl->fra;

      fits_read_col(phl->fptr, TDOUBLE, phl->cdec, ii+1, 1, 1, 
		    NULL, dec, &anynul, status);
      if (EXIT_SUCCESS!=*status) {
	SIMPUT_ERROR("failed reading declination from photon list");
	return;
      }
      *dec *=phl->fdec;

      return;
    }
  }
}


void getSimputPhotonEnergyCoord(SimputCtlg* const cat,
				SimputSrc* const src,
				double currtime,
				const double mjdref,
				float* const energy,
				double* const ra,
				double* const dec,
				int* const status)
{
  // Determine the references to the spectrum and 
  // image for the updated photon arrival time.
  char specref[SIMPUT_MAXSTR];
  getSimputSrcSpecRef(cat, src, currtime, mjdref, specref, status);
  CHECK_STATUS_VOID(*status);
  char imagref[SIMPUT_MAXSTR];
  getSrcImagRef(cat, src, currtime, mjdref, imagref, status);
  CHECK_STATUS_VOID(*status);

  
  // Determine the extension type of the spectrum and the image
  // reference.
  int spectype=getExtType(cat, specref, status);
  CHECK_STATUS_VOID(*status);
  int imagtype=getExtType(cat, imagref, status);
  CHECK_STATUS_VOID(*status);


  // If the spectrum or the image reference point to a photon list,
  // determine simultaneously the energy and spatial information.
  SimputPhList* phl=NULL;
  if (EXTTYPE_PHLIST==spectype) {
    phl=getSimputPhList(cat, specref, status);
    CHECK_STATUS_VOID(*status);
  } else if (EXTTYPE_PHLIST==imagtype) {
    phl=getSimputPhList(cat, imagref, status);
    CHECK_STATUS_VOID(*status);
  }
  if (NULL!=phl) {
    float b_energy;
    double b_ra, b_dec;
    getSimputPhFromPhList(cat, phl, &b_energy, &b_ra, &b_dec, status);
    CHECK_STATUS_VOID(*status);

    if (EXTTYPE_PHLIST==spectype) {
      *energy=b_energy;
    }
    if (EXTTYPE_PHLIST==imagtype) {
      *ra =b_ra;
      *dec=b_dec;
    }
  }


  // ---
  // If the spectrum does NOT refer to a photon list, determine the 
  // photon energy from a spectrum.
  if (EXTTYPE_MIDPSPEC==spectype) {
    // Determine the spectrum.
    SimputSpec* spec=getSimputSpec(cat, specref, status);
    CHECK_STATUS_VOID(*status);

    // Get a random number in the interval [0,1].
    double rnd=getRndNum(status);
    CHECK_STATUS_VOID(*status);
    assert(rnd>=0.);
    assert(rnd<=1.);

    // Multiply with the total photon rate (i.e. the spectrum
    // does not have to be normalized).
    rnd*=spec->distribution[cat->arf->NumberEnergyBins-1];

    // Determine the corresponding point in the spectral 
    // distribution (using binary search).
    long upper=cat->arf->NumberEnergyBins-1, lower=0, mid;
    while (upper>lower) {
      mid=(lower+upper)/2;
      if (spec->distribution[mid]<rnd) {
	lower=mid+1;
      } else {
	upper=mid;
      }
    }

    // Return the corresponding photon energy.
    *energy=
      cat->arf->LowEnergy[lower] + 
      getRndNum(status)*
      (cat->arf->HighEnergy[lower]-cat->arf->LowEnergy[lower]);
    CHECK_STATUS_VOID(*status);
  }
  // END of determine the photon energy.
  // --- 


  // ---
  // If the image does NOT refer to a photon list, determine the
  // spatial information.

  // Point-like sources.
  if (EXTTYPE_NONE==imagtype) {
    *ra =src->ra;
    *dec=src->dec;
  }

  // Spatially extended sources.
  else if (EXTTYPE_IMAGE==imagtype) {
    // Determine the photon direction from an image.
    struct wcsprm wcs={ .flag=-1 };

    do { // Error handling loop.

      // Determine the image.
      SimputImg* img=getSimputImg(cat, imagref, status);
      CHECK_STATUS_BREAK(*status);

      // Perform a binary search in 2 dimensions.
      double rnd=getRndNum(status)*img->dist[img->naxis1-1][img->naxis2-1];
      CHECK_STATUS_BREAK(*status);

      // Perform a binary search to obtain the x-coordinate.
      long high=img->naxis1-1;
      long xl  =0;
      long mid;
      long ymax=img->naxis2-1;
      while (high > xl) {
	mid=(xl+high)/2;
	if (img->dist[mid][ymax] < rnd) {
	  xl=mid+1;
	} else {
	  high=mid;
	}
      }

      // Search for the y coordinate.
      high=img->naxis2-1;
      long yl=0;
      while (high > yl) {
	mid=(yl+high)/2;
	if (img->dist[xl][mid] < rnd) {
	  yl=mid+1;
	} else {
	  high=mid;
	}
      }
      // Now xl and yl have pixel positions [long pixel coordinates].

      // Create a temporary wcsprm data structure, which can be modified
      // to fit this particular source. The wcsprm data structure contained 
      // in the image should not be modified, since it is used for all 
      // sources including the image.
      wcscopy(1, img->wcs, &wcs);

      // Set the position to the origin and assign the correct scaling.
      // TODO: This assumes that the image WCS is equivalent to the 
      // coordinate system used in the catalog!!
      wcs.crval[0] = src->ra *180./M_PI; // Units (CUNITn) must be [deg]!
      wcs.crval[1] = src->dec*180./M_PI;
      wcs.cdelt[0]*= 1./src->imgscal;
      wcs.cdelt[1]*= 1./src->imgscal;
      wcs.flag=0;

      // Check that CUNIT is set to "deg". Otherwise there will be a conflict
      // between CRVAL [deg] and CDELT [different unit]. 
      // TODO This is not required by the standard.
      if (((0!=strcmp(wcs.cunit[0], "deg     ")) && 
	   (0!=strcmp(wcs.cunit[0], "degree  "))) || 
	  ((0!=strcmp(wcs.cunit[1], "deg     ")) &&
	   (0!=strcmp(wcs.cunit[1], "degree  ")))) {
	*status=EXIT_FAILURE;
	char msg[SIMPUT_MAXSTR];
	sprintf(msg, "units of image coordinates are '%s' and '%s' "
		"(must be 'deg')", wcs.cunit[0], wcs.cunit[1]);
	SIMPUT_ERROR(msg);
	break;
      }

      // Determine floating point pixel positions shifted by 0.5 in 
      // order to match the FITS conventions and with a randomization
      // over the pixels.
      double xd=(double)xl + 0.5 + getRndNum(status);
      CHECK_STATUS_BREAK(*status);
      double yd=(double)yl + 0.5 + getRndNum(status);
      CHECK_STATUS_BREAK(*status);

      // Rotate the image (pixel coordinates) by IMGROTA around the 
      // reference point.
      double xdrot= 
	(xd-wcs.crpix[0])*cos(src->imgrota) + 
	(yd-wcs.crpix[1])*sin(src->imgrota) + wcs.crpix[0];
      double ydrot=
	-(xd-wcs.crpix[0])*sin(src->imgrota) + 
	 (yd-wcs.crpix[1])*cos(src->imgrota) + wcs.crpix[1];
      
      // Convert the long-valued pixel coordinates to double values,
      // including a randomization over the pixel and transform from 
      // pixel coordinates to RA and DEC ([rad]) using the  WCS information.
      p2s(&wcs, xdrot, ydrot, ra, dec, status);
      CHECK_STATUS_BREAK(*status);

      // Determine the RA in the interval from [0:2pi).
      while(*ra>=2.*M_PI) {
	*ra-=2.*M_PI;
      }
      while(*ra<0.) {
	*ra+=2.*M_PI;
      }
      
    } while(0); // END of error handling loop.

    // Release memory.
    wcsfree(&wcs);
  }
  // END of determine the photon direction.
  // ---

  return;
}


int getSimputPhoton(SimputCtlg* const cat,
		    SimputSrc* const src,
		    double prevtime,
		    const double mjdref,
		    double* const nexttime,
		    float* const energy,
		    double* const ra,
		    double* const dec,
		    int* const status)
{
  // Determine the time of the next photon.
  int failed=getSimputPhotonTime(cat, src, prevtime, mjdref, nexttime, status);
  CHECK_STATUS_RET(*status, failed);
  if (failed>0) return(failed);

  // Determine the energy and the direction of origin of the photon.
  getSimputPhotonEnergyCoord(cat, src, *nexttime, mjdref,
			     energy, ra, dec, status);
  CHECK_STATUS_RET(*status, 0);

  return(0);
}


float getSimputSrcExt(SimputCtlg* const cat,
		      const SimputSrc* const src,
		      const double prevtime,
		      const double mjdref,
		      int* const status)
{
  // Return value [rad].
  float extension=0.;

  struct wcsprm wcs={ .flag=-1 };

  do { // Error handling loop.

    // Get the source image for this particular source.
    char imagref[SIMPUT_MAXSTR];
    getSrcImagRef(cat, src, prevtime, mjdref, imagref, status);
    CHECK_STATUS_BREAK(*status);
    int imagtype=getExtType(cat, imagref, status);
    CHECK_STATUS_RET(*status, 0);

    // Check if it is a point-like or an extended source.
    if (EXTTYPE_NONE==imagtype) {
      // Point-like source.
      extension=0.;
      break;
      
    } else if (EXTTYPE_IMAGE==imagtype) {
      // Extended source => determine the maximum extension.
      double maxext=0.;

      SimputImg* img=getSimputImg(cat, imagref, status);
      CHECK_STATUS_BREAK(*status);

      // Copy the wcsprm structure and change the size 
      // according to IMGSCAL.
      wcscopy(1, img->wcs, &wcs);

      // Change the scale of the image according to the source specific
      // IMGSCAL property.
      wcs.crval[0] = 0.; 
      wcs.crval[1] = 0.;
      wcs.cdelt[0]*= 1./src->imgscal;
      wcs.cdelt[1]*= 1./src->imgscal;
      wcs.flag = 0;

      // Check lower left corner.
      double px=0.5;
      double py=0.5;
      double sx, sy;
      p2s(&wcs, px, py, &sx, &sy, status);
      CHECK_STATUS_BREAK(*status);
      // TODO This has not been tested extensively, yet.
      while(sx>M_PI) {
	sx-=2.0*M_PI;
      }
      double ext=sqrt(pow(sx,2.)+pow(sy,2.));
      if (ext>maxext) {
	maxext=ext;
      }

      // Check lower right corner.
      px = img->naxis1*1. + 0.5;
      py = 0.5;
      p2s(&wcs, px, py, &sx, &sy, status);
      CHECK_STATUS_BREAK(*status);
      while(sx>M_PI) {
	sx-=2.0*M_PI;
      }
      ext = sqrt(pow(sx,2.)+pow(sy,2.));
      if (ext>maxext) {
	maxext = ext;
      }
      
      // Check upper left corner.
      px=0.5;
      py=img->naxis2*1. + 0.5;
      p2s(&wcs, px, py, &sx, &sy, status);
      CHECK_STATUS_BREAK(*status);
      while(sx>M_PI) {
	sx-=2.0*M_PI;
      }
      ext=sqrt(pow(sx,2.)+pow(sy,2.));
      if (ext>maxext) {
	maxext=ext;
      }
      
      // Check upper right corner.
      px=img->naxis1*1. + 0.5;
      py=img->naxis2*1. + 0.5;
      p2s(&wcs, px, py, &sx, &sy, status);
      CHECK_STATUS_BREAK(*status);
      while(sx>M_PI) {
	sx-=2.0*M_PI;
      }
      ext=sqrt(pow(sx,2.)+pow(sy,2.));
      if (ext>maxext) {
	maxext=ext;
      }
      
      extension=(float)maxext;

    } else if (EXTTYPE_PHLIST==imagtype) {
      // Get the photon list.
      SimputPhList* phl=getSimputPhList(cat, imagref, status);
      CHECK_STATUS_BREAK(*status);
      
      // Determine the maximum extension by going through
      // the whole list of photons.
      double maxext=0.;
      const long buffsize=10000;
      double rabuffer[buffsize];
      double decbuffer[buffsize];
      long ii;
      for (ii=0; ii*buffsize<phl->nphs; ii++) {
	// Read a block of photons.
	int anynul=0;
	long nphs=MIN(buffsize, phl->nphs-(ii*buffsize));
	fits_read_col(phl->fptr, TDOUBLE, phl->cra, ii*buffsize+1, 
		      1, nphs, NULL, rabuffer, &anynul, status);
	if (EXIT_SUCCESS!=*status) {
	  SIMPUT_ERROR("failed reading right ascension from photon list");
	  return(0.);
	}
	fits_read_col(phl->fptr, TDOUBLE, phl->cdec, ii*buffsize+1, 
		      1, nphs, NULL, decbuffer, &anynul, status);
	if (EXIT_SUCCESS!=*status) {
	  SIMPUT_ERROR("failed reading declination from photon list");
	  return(0.);
	}

	// Determine the maximum extension.
	long jj;
	for (jj=0; jj<nphs; jj++) {
	  double ext=
	    sqrt(pow(rabuffer[jj],2.0)+pow(decbuffer[jj],2.0))*M_PI/180.;
	  if (ext>maxext) {
	    maxext=ext;
	  }
	}
      }

      extension=(float)maxext;
    }
  } while(0); // END of error handling loop.

  // Release memory.
  wcsfree(&wcs);

  return(extension);
}

