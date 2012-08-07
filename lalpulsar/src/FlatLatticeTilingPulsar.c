/*
 *  Copyright (C) 2007, 2008 Karl Wette
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

/**
 * \author Karl Wette
 * \file
 * \brief Pulsar-specific routines for FlatLatticeTiling
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <gsl/gsl_permutation.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_eigen.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_nan.h>

#include <lal/FlatLatticeTilingPulsar.h>
#include <lal/LALStdlib.h>
#include <lal/LALMalloc.h>
#include <lal/LALConstants.h>
#include <lal/XLALError.h>
#include <lal/Factorial.h>

/**
 * Set a flat lattice tiling to the spindown Fstat metric
 * (so no sky position tiling). Components are in the order
 * \f$\alpha,\delta,\omega_0,\omega_1,\omega_2,...\f$
 * and will be converted on output to
 * \f$\alpha,\delta,f_0,f_1,f_2,...\f$
 * using the conversions
 * \f$f_k = \omega_k \frac{(k+1)!}{{2\pi T^{k+1}}}\f$
 */
int XLALSetFlatLatticeTilingSpindownFstatMetric(
  FlatLatticeTiling *tiling, /**< Tiling structure */
  double max_mismatch,        /**< Maximum mismatch */
  double Tspan                /**< Time span of the search */
  )
{

  const size_t n = XLALFlatLatticeTilingDimension(tiling);

  /* Check input */
  XLAL_CHECK(Tspan > 0.0, XLAL_EINVAL);

  /* Allocate memory */
  gsl_matrix* norm_metric = gsl_matrix_alloc(n, n);
  XLAL_CHECK(norm_metric != NULL, XLAL_ENOMEM);
  gsl_vector* norm_to_real = gsl_vector_alloc(n);
  XLAL_CHECK(norm_to_real != NULL, XLAL_ENOMEM);
  gsl_matrix_set_identity(norm_metric);
  gsl_vector_set_all(norm_to_real, 1.0);

  /* Calculate metric and conversion factors */
  for (size_t i = 0; i < n - 2; ++i) {

    for (size_t j = 0; j < n - 2; ++j) {
      gsl_matrix_set(norm_metric, i + 2, j + 2, (1.0 * (i + 1) * (j + 1)) / ((i + 2) * (j + 2) * (i + j + 3)));
    }

    gsl_vector_set(norm_to_real, i + 2, 1.0 * LAL_FACT[i + 1] / (LAL_TWOPI * pow(Tspan, i + 1)));

  }

  /* Set the metric of the flat lattice tiling */
  XLAL_CHECK(XLALSetFlatLatticeTilingMetric(tiling, norm_metric, max_mismatch, norm_to_real) == XLAL_SUCCESS, XLAL_EFAILED);

  /* Cleanup */
  FREE_GSL_MATRIX(norm_metric);
  FREE_GSL_VECTOR(norm_to_real);

  return XLAL_SUCCESS;

}

/**
 * Set a flat lattice tiling to a parameter space defined by
 * the age and possible braking index range of an object
 */
static void AgeBraking1stSpindownBound(void *data, size_t dimension, gsl_vector *point, double *lower, double *upper)
{

  /* Set lower and upper bound */
  double x = gsl_vector_get(point, dimension-1);
  *lower = x * gsl_vector_get((gsl_vector*)data, 0);
  *upper = x * gsl_vector_get((gsl_vector*)data, 1);

}
static void AgeBraking1stSpindownFree(void *data)
{

  /* Cleanup */
  gsl_vector_free((gsl_vector*)data);

}
static void AgeBraking2ndSpindownBound(void *data, size_t dimension, gsl_vector *point, double *lower, double *upper)
{

  /* Set lower and upper bound */
  double x = gsl_vector_get(point, dimension-1);
  x *= x;
  x /= gsl_vector_get(point, dimension-2);
  *lower = x * gsl_vector_get((gsl_vector*)data, 0);
  *upper = x * gsl_vector_get((gsl_vector*)data, 1);

}
static void AgeBraking2ndSpindownFree(void *data)
{

  /* Cleanup */
  gsl_vector_free((gsl_vector*)data);

}
int XLALAddFlatLatticeTilingAgeBrakingIndexBounds(
  FlatLatticeTiling *tiling, /**< Tiling structure */
  double freq,                /**< Starting frequency */
  double freq_band,           /**< Frequency band */
  double age,                 /**< Spindown age */
  double min_braking,         /**< Minimum braking index */
  double max_braking,         /**< Maximum braking index */
  size_t offset               /**< Number of dimensions offset between first dimension and frequency */
  )
{

  /* Check tiling dimension */
  XLAL_CHECK(XLALFlatLatticeTilingDimension(tiling) >= 3, XLAL_EINVAL);

  /* Allocate memory */
  gsl_vector* f1dot_data = gsl_vector_alloc(2);
  XLAL_CHECK(f1dot_data != NULL, XLAL_ENOMEM);
  gsl_vector* f2dot_data = gsl_vector_alloc(2);
  XLAL_CHECK(f2dot_data != NULL, XLAL_ENOMEM);

  /* Set bound dimensions */
  int64_t bound = ((int64_t)1) << offset;

  /* Set frequency bounds */
  XLAL_CHECK(XLALAddFlatLatticeTilingConstantBound(tiling, offset, freq, freq + freq_band) == XLAL_SUCCESS, XLAL_EFAILED);

  /* Set first spindown bounds */
  bound = bound << 1;
  gsl_vector_set(f1dot_data, 0, -1.0 / ((min_braking - 1.0) * age));
  gsl_vector_set(f1dot_data, 1, -1.0 / ((max_braking - 1.0) * age));
  XLAL_CHECK(XLALAddFlatLatticeTilingBound(tiling, bound, AgeBraking1stSpindownBound,
                                           (void*)f1dot_data, AgeBraking1stSpindownFree) == XLAL_SUCCESS, XLAL_EFAILED);

  /* Set second spindown bounds */
  bound = bound << 1;
  gsl_vector_set(f2dot_data, 0, min_braking);
  gsl_vector_set(f2dot_data, 1, max_braking);
  XLAL_CHECK(XLALAddFlatLatticeTilingBound(tiling, bound, AgeBraking2ndSpindownBound,
                                           (void*)f2dot_data, AgeBraking2ndSpindownFree) == XLAL_SUCCESS, XLAL_EFAILED);

  return XLAL_SUCCESS;

}
