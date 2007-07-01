/*
*  Copyright (C) 2007 Bernd Machenschalk, Jolien Creighton, Kipp Cannon
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with with program; see the file COPYING. If not, write to the
*  Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
*  MA  02111-1307  USA
*/

#include <math.h>
#include <string.h>
#include <lal/FrequencySeries.h>
#include <lal/LALStdlib.h>
#include <lal/LALConstants.h>
#include <lal/AVFactories.h>
#include <lal/Sequence.h>
#include <lal/TimeFreqFFT.h>
#include <lal/Units.h>
#include <lal/Window.h>

#include <lal/LALRCSID.h>
NRCSID (AVERAGESPECTRUMC,"$Id$");


/*
 *
 * Compute a "modified periodogram," i.e., the power spectrum of a windowed
 * time series.
 *
 */
int XLALREAL4ModifiedPeriodogram(
    REAL4FrequencySeries        *periodogram,
    const REAL4TimeSeries       *tseries,
    const REAL4Window           *window,
    const REAL4FFTPlan          *plan
    )
{
  static const char *func = "XLALREAL4ModifiedPeriodogram";
  REAL4Sequence *work;
  REAL4 normfac;
  UINT4 k;
  int result;

  if ( ! periodogram || ! tseries || ! plan )
      XLAL_ERROR( func, XLAL_EFAULT );
  if ( ! periodogram->data || ! tseries->data )
      XLAL_ERROR( func, XLAL_EINVAL );
  if ( tseries->deltaT <= 0.0 )
      XLAL_ERROR( func, XLAL_EINVAL );
  if ( periodogram->data->length != tseries->data->length/2 + 1 )
      XLAL_ERROR( func, XLAL_EBADLEN );
  if ( window )
  {
    if ( ! window->data )
      XLAL_ERROR( func, XLAL_EINVAL );
    if ( window->sumofsquares <= 0.0 )
      XLAL_ERROR( func, XLAL_EINVAL );
    if ( window->data->length != tseries->data->length )
      XLAL_ERROR( func, XLAL_EBADLEN );
  }

  /* if the window has been specified, apply it to data */
  if ( window )
  {
    UINT4 j;

    /* make a working copy */
    work = XLALCutREAL4Sequence( tseries->data, 0, tseries->data->length );
    if ( ! work )
      XLAL_ERROR( func, XLAL_EFUNC );

    /* apply windowing to data */
    for ( j = 0; j < tseries->data->length; ++j )
      work->data[j] *= window->data->data[j];
  }
  else
    /* point to original data */
    work = tseries->data;

  /* compute the power spectrum of the (windowed) timeseries */
  /* CHECKME: are DC and Nyquist right? */
  result = XLALREAL4PowerSpectrum( periodogram->data, work, plan );
  /* destroy the workspace if it was created */
  if ( window )
    XLALDestroyREAL4Sequence( work );
  /* check for errors from the PowerSpectrum call */
  if ( result == XLAL_FAILURE )
    XLAL_ERROR( func, XLAL_EFUNC );

  /* normalize power spectrum to give correct units */
  /* CHECKME: is this the right factor? */
  if ( window )
    normfac = tseries->deltaT / window->sumofsquares;
  else
    normfac = tseries->deltaT / tseries->data->length;
  for ( k = 0; k < periodogram->data->length; ++k )
    periodogram->data->data[k] *= normfac;

  /* now set rest of metadata */
  periodogram->epoch  = tseries->epoch;
  periodogram->f0     = tseries->f0; /* FIXME: is this right? */
  periodogram->deltaF = 1.0 / ( tseries->data->length * tseries->deltaT );

  /* compute units */
  if ( ! XLALUnitSquare( &periodogram->sampleUnits, &tseries->sampleUnits ) )
    XLAL_ERROR( func, XLAL_EFUNC );
  if ( ! XLALUnitMultiply( &periodogram->sampleUnits,
                           &periodogram->sampleUnits, &lalSecondUnit ) )
    XLAL_ERROR( func, XLAL_EFUNC );

  return 0;
}


/* 
 *
 * Use Welch's method to compute the average power spectrum of a time series.
 *
 * See: Peter D. Welch "The Use of Fast Fourier Transform for the Estimation
 * of Power Spectra: A Method Based on Time Averaging Over Short, Modified
 * Periodograms"  IEEE Transactions on Audio and Electroacoustics,
 * Vol. AU-15, No. 2, June 1967.
 *
 */
int XLALREAL4AverageSpectrumWelch(
    REAL4FrequencySeries        *spectrum,
    const REAL4TimeSeries       *tseries,
    UINT4                        seglen,
    UINT4                        stride,
    const REAL4Window           *window,
    const REAL4FFTPlan          *plan
    )
{
  static const char *func = "XLALREAL4AverageSpectrumWelch";
  REAL4FrequencySeries *work; /* workspace */
  REAL4Sequence sequence; /* working copy of input time series data */
  REAL4TimeSeries tseriescopy; /* working copy of input time series */
  UINT4 numseg;
  UINT4 seg;
  UINT4 k;

  if ( ! spectrum || ! tseries || ! plan )
      XLAL_ERROR( func, XLAL_EFAULT );
  if ( ! spectrum->data || ! tseries->data )
      XLAL_ERROR( func, XLAL_EINVAL );
  if ( tseries->deltaT <= 0.0 )
      XLAL_ERROR( func, XLAL_EINVAL );

  /* construct local copy of time series */
  sequence = *tseries->data;
  tseriescopy = *tseries;
  tseriescopy.data = &sequence;
  tseriescopy.data->length = seglen;

  numseg = 1 + (tseries->data->length - seglen)/stride;

  /* consistency check for lengths: make sure that the segments cover the
   * data record completely */
  if ( (numseg - 1)*stride + seglen != tseries->data->length )
    XLAL_ERROR( func, XLAL_EBADLEN );
  if ( spectrum->data->length != seglen/2 + 1 )
    XLAL_ERROR( func, XLAL_EBADLEN );

  /* make sure window, if present, is appropriate */
  if ( window )
  {
    if ( ! window->data )
      XLAL_ERROR( func, XLAL_EINVAL );
    if ( window->sumofsquares <= 0.0 )
      XLAL_ERROR( func, XLAL_EINVAL );
    if ( window->data->length != seglen )
      XLAL_ERROR( func, XLAL_EBADLEN );
  }

  /* clear spectrum data */
  memset( spectrum->data->data, 0,
      spectrum->data->length * sizeof( *spectrum->data->data ) );

  /* create frequency series data workspace */
  work = XLALCutREAL4FrequencySeries( spectrum, 0, spectrum->data->length );
  if( ! work )
    XLAL_ERROR( func, XLAL_EFUNC );

  for ( seg = 0; seg < numseg; seg++, tseriescopy.data->data += stride )
  {
    /* compute the modified periodogram; clean up and exit on failure */
    if ( XLALREAL4ModifiedPeriodogram( work, &tseriescopy, window, plan ) == XLAL_FAILURE )
    {
      XLALDestroyREAL4FrequencySeries( work );
      XLAL_ERROR( func, XLAL_EFUNC );
    }

    /* add the periodogram to the running sum */
    for ( k = 0; k < spectrum->data->length; ++k )
      spectrum->data->data[k] += work->data->data[k];
  }

  /* set metadata */
  spectrum->epoch       = work->epoch;
  spectrum->f0          = work->f0;
  spectrum->deltaF      = work->deltaF;
  spectrum->sampleUnits = work->sampleUnits;

  /* divide spectrum data by the number of segments in average */
  for ( k = 0; k < spectrum->data->length; ++k )
    spectrum->data->data[k] /= numseg;

  /* clean up */
  XLALDestroyREAL4FrequencySeries( work );

  return 0;
}


/* 
 *
 * Median Method: use median average rather than mean.  Note: this will
 * cause a bias if the segments overlap, i.e., if the stride is less than
 * the segment length -- even though the median bias for Gaussian noise
 * is accounted for -- because the segments are not independent and their
 * correlation is non-zero.
 *
 */

/* compute the median bias */
REAL8 XLALMedianBias( UINT4 nn )
{
  const UINT4 nmax = 1000;
  REAL8 ans = 1;
  UINT4 n = (nn - 1)/2;
  UINT4 i;

  if ( nn >= nmax )
    return LAL_LN2;

  for ( i = 1; i <= n; ++i )
  {
    ans -= 1.0/(2*i);
    ans += 1.0/(2*i + 1);
  }

  return ans;
}

/* cleanup temporary workspace... ignore xlal errors */
static void median_cleanup( REAL4FrequencySeries *work, UINT4 n )
{
  int saveErrno = xlalErrno;
  UINT4 i;
  for ( i = 0; i < n; ++i )
    if ( work[i].data )
      XLALDestroyREAL4Vector( work[i].data );
  LALFree( work );
  xlalErrno = saveErrno;
  return;
}

/* comparison for floating point numbers */
static int compare_float( const void *p1, const void *p2 )
{
  REAL4 delta = *(const REAL4 *) p1 - *(const REAL4 *) p2;
  if(delta < 0.0)
    return -1;
  if(delta > 0.0)
    return +1;
  return 0;
}


/* here is the median method */
int XLALREAL4AverageSpectrumMedian(
    REAL4FrequencySeries        *spectrum,
    const REAL4TimeSeries       *tseries,
    UINT4                        seglen,
    UINT4                        stride,
    const REAL4Window           *window,
    const REAL4FFTPlan          *plan
    )
{
  static const char *func = "XLALREAL4AverageSpectrumMedian";
  REAL4FrequencySeries *work; /* array of frequency series */
  REAL4 *bin; /* array of bin values */
  REAL4 biasfac; /* median bias factor */
  REAL4 normfac; /* normalization factor */
  UINT4 reclen; /* length of entire data record */
  UINT4 numseg;
  UINT4 seg;
  UINT4 k;

  if ( ! spectrum || ! tseries || ! plan )
      XLAL_ERROR( func, XLAL_EFAULT );
  if ( ! spectrum->data || ! tseries->data )
      XLAL_ERROR( func, XLAL_EINVAL );
  if ( tseries->deltaT <= 0.0 )
      XLAL_ERROR( func, XLAL_EINVAL );

  reclen = tseries->data->length;
  numseg = 1 + (reclen - seglen)/stride;

  /* consistency check for lengths: make sure that the segments cover the
   * data record completely */
  if ( (numseg - 1)*stride + seglen != reclen )
    XLAL_ERROR( func, XLAL_EBADLEN );
  if ( spectrum->data->length != seglen/2 + 1 )
    XLAL_ERROR( func, XLAL_EBADLEN );

  /* make sure window, if present, is appropriate */
  if ( window )
  {
    if ( ! window->data )
      XLAL_ERROR( func, XLAL_EINVAL );
    if ( window->sumofsquares <= 0.0 )
      XLAL_ERROR( func, XLAL_EINVAL );
    if ( window->data->length != seglen )
      XLAL_ERROR( func, XLAL_EBADLEN );
  }

  /* create frequency series data workspaces */
  work = LALCalloc( numseg, sizeof( *work ) );
  if ( ! work )
    XLAL_ERROR( func, XLAL_ENOMEM );
  for ( seg = 0; seg < numseg; ++seg )
  {
    work[seg].data = XLALCreateREAL4Vector( spectrum->data->length );
    if ( ! work[seg].data )
    {
      median_cleanup( work, numseg ); /* cleanup */
      XLAL_ERROR( func, XLAL_EFUNC );
    }
  }

  for ( seg = 0; seg < numseg; ++seg )
  {
    REAL4Vector savevec; /* save the time series data vector */
    int code;

    /* save the time series data vector */
    savevec = *tseries->data;

    /* set the data vector to be appropriate for the even segment */
    tseries->data->length  = seglen;
    tseries->data->data   += seg * stride;

    /* compute the modified periodogram for the even segment */
    code = XLALREAL4ModifiedPeriodogram( work + seg, tseries, window, plan );
    
    /* restore the time series data vector to its original state */
    *tseries->data = savevec;

    /* now check for failure of the XLAL routine */
    if ( code == XLAL_FAILURE )
    {
      median_cleanup( work, numseg ); /* cleanup */
      XLAL_ERROR( func, XLAL_EFUNC );
    }
  }

  /* create array to hold a particular frequency bin data */
  bin = LALMalloc( numseg * sizeof( *bin ) );
  if ( ! bin )
  {
    median_cleanup( work, numseg ); /* cleanup */
    XLAL_ERROR( func, XLAL_ENOMEM );
  }

  /* compute median bias factor */
  biasfac = XLALMedianBias( numseg );

  /* normaliztion takes into account bias */
  normfac = 1.0 / biasfac;

  /* now loop over frequency bins and compute the median-mean */
  for ( k = 0; k < spectrum->data->length; ++k )
  {
    /* assign array of even segment values to bin array for this freq bin */
    for ( seg = 0; seg < numseg; ++seg )
      bin[seg] = work[seg].data->data[k];

    /* sort them and find median */
    qsort( bin, numseg, sizeof( *bin ), compare_float );
    if ( numseg % 2 ) /* odd number of evens */
      spectrum->data->data[k] = bin[numseg/2];
    else /* even number... take average */
      spectrum->data->data[k] = 0.5*(bin[numseg/2-1] + bin[numseg/2]);

    /* remove median bias */
    spectrum->data->data[k] *= normfac;
  }

  /* set metadata */
  spectrum->epoch       = work->epoch;
  spectrum->f0          = work->f0;
  spectrum->deltaF      = work->deltaF;
  spectrum->sampleUnits = work->sampleUnits;

  /* free the workspace data */
  LALFree( bin );
  median_cleanup( work, numseg );

  return 0;
}


/* 
 *
 * Median-Mean Method: divide overlapping segments into "even" and "odd"
 * segments; compute the bin-by-bin median of the "even" segments and the
 * "odd" segments, and then take the bin-by-bin average of these two median
 * averages.
 *
 */


/* cleanup temporary workspace... ignore xlal errors */
static void median_mean_cleanup( REAL4FrequencySeries *even,
    REAL4FrequencySeries *odd, UINT4 n )
{
  int saveErrno = xlalErrno;
  UINT4 i;
  for ( i = 0; i < n; ++i )
  {
    if ( even[i].data )
      XLALDestroyREAL4Vector( even[i].data );
    if ( odd[i].data )
      XLALDestroyREAL4Vector( odd[i].data );
  }
  LALFree( even );
  LALFree( odd );
  xlalErrno = saveErrno;
  return;
}

/* here is the median-mean-method */
int XLALREAL4AverageSpectrumMedianMean(
    REAL4FrequencySeries        *spectrum,
    const REAL4TimeSeries       *tseries,
    UINT4                        seglen,
    UINT4                        stride,
    const REAL4Window           *window,
    const REAL4FFTPlan          *plan
    )
{
  static const char *func = "XLALREAL4AverageSpectrumMedianMean";
  REAL4FrequencySeries *even; /* array of even frequency series */
  REAL4FrequencySeries *odd;  /* array of odd frequency series */
  REAL4 *bin; /* array of bin values */
  REAL4 biasfac; /* median bias factor */
  REAL4 normfac; /* normalization factor */
  UINT4 reclen; /* length of entire data record */
  UINT4 numseg;
  UINT4 halfnumseg;
  UINT4 seg;
  UINT4 k;

  if ( ! spectrum || ! tseries || ! plan )
      XLAL_ERROR( func, XLAL_EFAULT );
  if ( ! spectrum->data || ! tseries->data )
      XLAL_ERROR( func, XLAL_EINVAL );
  if ( tseries->deltaT <= 0.0 )
      XLAL_ERROR( func, XLAL_EINVAL );

  reclen = tseries->data->length;
  numseg = 1 + (reclen - seglen)/stride;

  /* consistency check for lengths: make sure that the segments cover the
   * data record completely */
  if ( (numseg - 1)*stride + seglen != reclen )
    XLAL_ERROR( func, XLAL_EBADLEN );
  if ( spectrum->data->length != seglen/2 + 1 )
    XLAL_ERROR( func, XLAL_EBADLEN );

  /* for median-mean to work, the number of segments must be even and
   * the stride must be greater-than or equal-to half of the seglen */
  halfnumseg = numseg/2;
  if ( numseg%2 || stride < seglen/2 )
    XLAL_ERROR( func, XLAL_EBADLEN );

  /* make sure window, if present, is appropriate */
  if ( window )
  {
    if ( ! window->data )
      XLAL_ERROR( func, XLAL_EINVAL );
    if ( window->sumofsquares <= 0.0 )
      XLAL_ERROR( func, XLAL_EINVAL );
    if ( window->data->length != seglen )
      XLAL_ERROR( func, XLAL_EBADLEN );
  }

  /* create frequency series data workspaces */
  even = LALCalloc( halfnumseg, sizeof( *even ) );
  if ( ! even )
    XLAL_ERROR( func, XLAL_ENOMEM );
  odd = LALCalloc( halfnumseg, sizeof( *odd ) );
  if ( ! odd )
    XLAL_ERROR( func, XLAL_ENOMEM );
  for ( seg = 0; seg < halfnumseg; ++seg )
  {
    even[seg].data = XLALCreateREAL4Vector( spectrum->data->length );
    odd[seg].data  = XLALCreateREAL4Vector( spectrum->data->length );
    if ( ! even[seg].data || ! odd[seg].data )
    {
      median_mean_cleanup( even, odd, halfnumseg ); /* cleanup */
      XLAL_ERROR( func, XLAL_EFUNC );
    }
  }

  for ( seg = 0; seg < halfnumseg; ++seg )
  {
    REAL4Vector savevec; /* save the time series data vector */
    int code;

    /* save the time series data vector */
    savevec = *tseries->data;

    /* set the data vector to be appropriate for the even segment */
    tseries->data->length  = seglen;
    tseries->data->data   += 2 * seg * stride;

    /* compute the modified periodogram for the even segment */
    code = XLALREAL4ModifiedPeriodogram( even + seg, tseries, window, plan );
    
    /* now check for failure of the XLAL routine */
    if ( code == XLAL_FAILURE )
    {
      *tseries->data = savevec;
      median_mean_cleanup( even, odd, halfnumseg ); /* cleanup */
      XLAL_ERROR( func, XLAL_EFUNC );
    }

    /* set the data vector to be appropriate for the odd segment */
    tseries->data->data += stride;

    /* compute the modified periodogram for the odd segment */
    code = XLALREAL4ModifiedPeriodogram( odd + seg, tseries, window, plan );

    /* now check for failure of the XLAL routine */
    if ( code == XLAL_FAILURE )
    {
      *tseries->data = savevec;
      median_mean_cleanup( even, odd, halfnumseg ); /* cleanup */
      XLAL_ERROR( func, XLAL_EFUNC );
    }

    /* restore the time series data vector to its original state */
    *tseries->data = savevec;
  }

  /* create array to hold a particular frequency bin data */
  bin = LALMalloc( halfnumseg * sizeof( *bin ) );
  if ( ! bin )
  {
    median_mean_cleanup( even, odd, halfnumseg ); /* cleanup */
    XLAL_ERROR( func, XLAL_ENOMEM );
  }

  /* compute median bias factor */
  biasfac = XLALMedianBias( halfnumseg );

  /* normaliztion takes into account bias and a factor of two from averaging
   * the even and the odd */
  normfac = 1.0 / ( 2.0 * biasfac );

  /* now loop over frequency bins and compute the median-mean */
  for ( k = 0; k < spectrum->data->length; ++k )
  {
    REAL4 evenmedian;
    REAL4 oddmedian;

    /* assign array of even segment values to bin array for this freq bin */
    for ( seg = 0; seg < halfnumseg; ++seg )
      bin[seg] = even[seg].data->data[k];

    /* sort them and find median */
    qsort( bin, halfnumseg, sizeof( *bin ), compare_float );
    if ( halfnumseg % 2 ) /* odd number of evens */
      evenmedian = bin[halfnumseg/2];
    else /* even number... take average */
      evenmedian = 0.5*(bin[halfnumseg/2-1] + bin[halfnumseg/2]);

    /* assign array of odd segment values to bin array for this freq bin */
    for ( seg = 0; seg < halfnumseg; ++seg )
      bin[seg] = odd[seg].data->data[k];

    /* sort them and find median */
    qsort( bin, halfnumseg, sizeof( *bin ), compare_float );
    if ( halfnumseg % 2 ) /* odd number of odds */
      oddmedian = bin[halfnumseg/2];
    else /* even number... take average */
      oddmedian = 0.5*(bin[halfnumseg/2-1] + bin[halfnumseg/2]);

    /* spectrum for this bin is the mean of the medians */
    spectrum->data->data[k] = normfac * (evenmedian + oddmedian);
  }

  /* set metadata */
  spectrum->epoch       = even->epoch;
  spectrum->f0          = even->f0;
  spectrum->deltaF      = even->deltaF;
  spectrum->sampleUnits = even->sampleUnits;

  /* free the workspace data */
  LALFree( bin );
  median_mean_cleanup( even, odd, halfnumseg );

  return 0;
}

int XLALREAL4SpectrumInvertTruncate(
    REAL4FrequencySeries        *spectrum,
    REAL4                        lowfreq,
    UINT4                        seglen,
    UINT4                        trunclen,
    REAL4FFTPlan                *fwdplan,
    REAL4FFTPlan                *revplan
    )
{
  static const char *func = "XLALREAL4SpectrumInvertTruncate";
  UINT4 cut;
  UINT4 k;

  if ( ! spectrum )
    XLAL_ERROR( func, XLAL_EFAULT );
  if ( ! spectrum->data )
    XLAL_ERROR( func, XLAL_EINVAL );
  if ( spectrum->deltaF <= 0.0 )
    XLAL_ERROR( func, XLAL_EINVAL );
  if ( lowfreq < 0 )
    XLAL_ERROR( func, XLAL_EINVAL );
  if ( ! seglen || spectrum->data->length != seglen/2 + 1 )
    XLAL_ERROR( func, XLAL_EBADLEN );
  if ( trunclen && ! revplan )
    XLAL_ERROR( func, XLAL_EFAULT );

  cut = lowfreq / spectrum->deltaF;
  if ( cut < 1 ) /* need to get rid of DC at least */
    cut = 1;

  if ( trunclen ) /* truncate while inverting */
  {
    COMPLEX8Vector *vtilde; /* frequency-domain vector workspace */
    REAL4Vector    *vector; /* time-domain vector workspace */
    REAL4           normfac;

    vector = XLALCreateREAL4Vector( seglen );
    vtilde = XLALCreateCOMPLEX8Vector( seglen / 2 + 1 );

    /* clear the low frequency components */
    memset( vtilde->data, 0, cut * sizeof( *vtilde->data ) );

    /* stick the root-inv-spectrum into the complex frequency-domain vector */
    for ( k = cut; k < vtilde->length - 1; ++k )
    {
      vtilde->data[k].re = 1.0 / sqrt( spectrum->data->data[k] );
      vtilde->data[k].im = 0.0; /* purely real */
    }

    /* no Nyquist */
    vtilde->data[vtilde->length - 1].re = 0.0;
    vtilde->data[vtilde->length - 1].im = 0.0;

    /* construct time-domain version of root-inv-spectrum */
    XLALREAL4ReverseFFT( vector, vtilde, revplan );

    /* now truncate it: zero time from trunclen/2 to length - trunclen/2 */
    memset( vector->data + trunclen/2, 0,
        (vector->length - trunclen) * sizeof( *vector->data ) );

    /* reconstruct spectrum */
    XLALREAL4PowerSpectrum( spectrum->data, vector, fwdplan );

    /* clear the low frequency components... again, and Nyquist too */
    memset( spectrum->data->data, 0, cut * sizeof( *spectrum->data->data ) );
    spectrum->data->data[spectrum->data->length - 1] = 0.0;

    /* rescale spectrum: 0.5 to undo factor of two from REAL4PowerSpectrum */
    /* seglen^2 for the reverse fft (squared because power spectrum) */
    /* Note: cast seglen to real so that seglen*seglen is a real rather */
    /* than a four-byte integer which might overflow... */
    normfac = 0.5 / ( ((REAL4)seglen) * ((REAL4)seglen) );
    for ( k = cut; k < spectrum->data->length - 1; ++k )
      spectrum->data->data[k] *= normfac;

    /* cleanup workspace */
    XLALDestroyCOMPLEX8Vector( vtilde );
    XLALDestroyREAL4Vector( vector );
  }
  else /* otherwise just invert the spectrum */
  {
    /* clear the low frequency components */
    memset( spectrum->data->data, 0, cut * sizeof( *spectrum->data->data ) );

    /* invert the high frequency (non-Nyquist) components */
    for ( k = cut; k < spectrum->data->length - 1; ++k )
      spectrum->data->data[k] = 1.0 / spectrum->data->data[k];

    /* zero Nyquist */
    spectrum->data->data[spectrum->data->length - 1] = 0.0;
  }

  /* now correct the units */
  XLALUnitRaiseINT2( &spectrum->sampleUnits, &spectrum->sampleUnits, -1 );

  return 0;
}


/*
 *
 * Normalize a COMPLEX8 frequency series to a REAL4 average PSD.  If the
 * frequency series is the Fourier transform of (coloured) Gaussian random
 * noise, and the PSD is of the same noise, and both have been computed
 * according to the LAL technical specifications (LIGO-T010095-00-Z), then
 * the output frequency series' bins will be complex Gaussian random
 * variables with mean squares of 1 (the real and imaginary components,
 * individually, have variances of 1/2).  Fourier transforms computed by
 * XLALREAL4ForwardFFT(), and PSDs computed by
 * XLALREAL4AverageSpectrumMedian() and friends conform to the LAL
 * technical specifications.
 *
 * PSDs computed from high- or low-passed data can include 0s at one or the
 * other end of the spectrum, and these would normally result in
 * divide-by-zero errors.  This routine avoids PSD divide-by-zero errors by
 * permitting zeroes in the PSD outside of the band given by fmin <= f <
 * fmax.  The fmin and fmax parameters set the frequency band ``of
 * interest''.  Divide-by-zero errors within the band of interest are
 * reported, while divide-by-zero errors outside the band of interest are
 * ignored and the output frequency series zeroed in the affected bins.
 *
 * The input PSD is allowed to span a larger frequency band than the input
 * frequency series, but the frequency resolutions of the two must be the
 * same.  The frequency series is modified in place.  The return value is
 * the input frequency series pointer on success, or NULL on error.  When
 * an error occurs, the contents of the input frequency series are
 * undefined.
 */


COMPLEX8FrequencySeries *XLALWhitenCOMPLEX8FrequencySeries(COMPLEX8FrequencySeries *fseries, const REAL4FrequencySeries *psd, REAL8 fmin, REAL8 fmax)
{
  static const char func[] = "XLALWhitenCOMPLEX8FrequencySeries";
  COMPLEX8 *fdata = fseries->data->data;
  REAL4 *pdata = psd->data->data;
  REAL4 factor;
  unsigned i;	/* fseries index */
  unsigned j;	/* psd index */

  if((psd->deltaF != fseries->deltaF) ||
     (fseries->f0 < psd->f0))
    /* resolution mismatch, or PSD does not span fseries at low end */
    XLAL_ERROR_NULL(func, XLAL_EINVAL);

  j = (fseries->f0 - psd->f0) / psd->deltaF;
  if(j * psd->deltaF + psd->f0 != fseries->f0)
    /* fseries does not start on an integer PSD sample */
    XLAL_ERROR_NULL(func, XLAL_EINVAL);
  if(j + fseries->data->length > psd->data->length)
    /* PSD does not span fseries at high end */
    XLAL_ERROR_NULL(func, XLAL_EINVAL);

  for(i = 0; i < fseries->data->length; i++, j++)
  {
    if(pdata[j] == 0)
    {
      /* PSD has a 0 in it */
      REAL8 f = fseries->f0 + i * fseries->deltaF;
      if((fmin <= f) && (f < fmax))
        /* f is in band:  error */
        XLAL_ERROR_NULL(func, XLAL_EFPDIV0);
      else
        /* f is out of band:  ignore, zero the output */
        factor = 0;
    }
    else
      factor = sqrt(2 * fseries->deltaF / pdata[j]);
    fdata[i].re *= factor;
    fdata[i].im *= factor;
  }

  return fseries;
}


/*
 * Double-precision version of XLALWhitenCOMPLEX8FrequencySeries().
 */


COMPLEX16FrequencySeries *XLALWhitenCOMPLEX16FrequencySeries(COMPLEX16FrequencySeries *fseries, const REAL8FrequencySeries *psd, REAL8 fmin, REAL8 fmax)
{
  static const char func[] = "XLALWhitenCOMPLEX16FrequencySeries";
  COMPLEX16 *fdata = fseries->data->data;
  REAL8 *pdata = psd->data->data;
  REAL8 factor;
  unsigned i;	/* fseries index */
  unsigned j;	/* psd index */

  if((psd->deltaF != fseries->deltaF) ||
     (fseries->f0 < psd->f0))
    /* resolution mismatch, or PSD does not span fseries at low end */
    XLAL_ERROR_NULL(func, XLAL_EINVAL);

  j = (fseries->f0 - psd->f0) / psd->deltaF;
  if(j * psd->deltaF + psd->f0 != fseries->f0)
    /* fseries does not start on an integer PSD sample */
    XLAL_ERROR_NULL(func, XLAL_EINVAL);
  if(j + fseries->data->length > psd->data->length)
    /* PSD does not span fseries at high end */
    XLAL_ERROR_NULL(func, XLAL_EINVAL);

  for(i = 0; i < fseries->data->length; i++, j++)
  {
    if(pdata[j] == 0)
    {
      /* PSD has a 0 in it */
      REAL8 f = fseries->f0 + i * fseries->deltaF;
      if((fmin <= f) && (f < fmax))
        /* f is in band:  error */
        XLAL_ERROR_NULL(func, XLAL_EFPDIV0);
      else
        /* f is out of band:  ignore, zero the output */
        factor = 0;
    }
    else
      factor = sqrt(2 * fseries->deltaF / pdata[j]);
    fdata[i].re *= factor;
    fdata[i].im *= factor;
  }

  return fseries;
}

