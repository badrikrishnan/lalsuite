/*
*  Copyright (C) 2007 David Churches, B.S. Sathyaprakash, Duncan Brown, Drew Keppel
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
 * \author Sathyaprakash, B. S.
 * \file
 * \ingroup LALInspiral_h
 *
 * \brief The code \ref LALInspiralFrequency3.c calculates the frequency the
 * waveform from an inspiralling binary system as a function of time up to 3.5
 * post-Nowtonian order.
 *
 * ### Prototypes ###
 *
 * <tt>LALInspiralFrequency3()</tt>
 * <ul>
 * <li> \c frequency: Output containing the inspiral waveform.</li>
 * <li> \c td: Input containing PN expansion coefficients \f$F_k\f$ (cf. \ref table_flux "this table")
 * of frequency as a function of time.</li>
 * <li> \c ak: Input containing all PN expansion coefficients.</li>
 * </ul>
 *
 * ### Description ###
 *
 * This module computes the instantaneous frequency of an inspiral wave using
 * \f{equation}{
 * F(t) = F_N(\theta) \sum F_k \theta^k,
 * \f}
 * where the expansion coefficients \f$F_k,\f$ Newtonian value \f$F_N\f$ and the
 * time-variable \f$\theta\f$ are defined in \ref table_flux "this table".
 *
 * ### Algorithm ###
 *
 *
 * ### Uses ###
 *
 * None.
 *
 * ### Notes ###
 *
 * The frequency evolution defined by post-Newtonian expansion is not monotonic.
 * Indeed, the equations become highly inaccurate close to the last stable orbit (lso)
 * and breakdown at or slightly after lso, and the frequency begins to decrease at later times.
 * It turns out that the evolution is monotonic at least up to lso.
 *
 */


#include <math.h>
#include <lal/LALAtomicDatatypes.h>
#include <lal/LALInspiral.h>
#include <lal/XLALError.h>


REAL8
XLALInspiralFrequency3_0PN (
   REAL8       td,
   expnCoeffs *ak
   )
{
  REAL8 theta,theta3;
  REAL8 frequency;

  if (ak == NULL)
    XLAL_ERROR_REAL8(XLAL_EFAULT);

  theta = pow(td,-0.125);
  theta3 = theta*theta*theta;

  frequency = theta3*ak->ftaN;

  return frequency;
}



REAL8
XLALInspiralFrequency3_2PN (
   REAL8       td,
   expnCoeffs *ak
   )
{
  REAL8 theta,theta2,theta3;
  REAL8 frequency;

  if (ak == NULL)
    XLAL_ERROR_REAL8(XLAL_EFAULT);

  theta = pow(td,-0.125);
  theta2 = theta*theta;
  theta3 = theta2*theta;

  frequency = theta3*ak->ftaN * (1.
             + ak->fta2*theta2);

  return frequency;
}



REAL8
XLALInspiralFrequency3_3PN (
   REAL8       td,
   expnCoeffs *ak
   )
{
  REAL8 theta,theta2,theta3;
  REAL8 frequency;

  if (ak == NULL)
    XLAL_ERROR_REAL8(XLAL_EFAULT);

  theta = pow(td,-0.125);
  theta2 = theta*theta;
  theta3 = theta2*theta;

  frequency = theta3*ak->ftaN * (1.
             + ak->fta2*theta2
             + ak->fta3*theta3);

  return frequency;
}



REAL8
XLALInspiralFrequency3_4PN (
   REAL8       td,
   expnCoeffs *ak
   )
{
  REAL8 theta,theta2,theta3,theta4;
  REAL8 frequency;

  if (ak == NULL)
    XLAL_ERROR_REAL8(XLAL_EFAULT);

  theta = pow(td,-0.125);
  theta2 = theta*theta;
  theta3 = theta2*theta;
  theta4 = theta3*theta;

  frequency = theta3*ak->ftaN * (1.
             + ak->fta2*theta2
             + ak->fta3*theta3
             + ak->fta4*theta4);

  return frequency;
}



REAL8
XLALInspiralFrequency3_5PN (
   REAL8       td,
   expnCoeffs *ak
   )
{
  REAL8 theta,theta2,theta3,theta4,theta5;
  REAL8 frequency;

  if (ak == NULL)
    XLAL_ERROR_REAL8(XLAL_EFAULT);

  theta = pow(td,-0.125);
  theta2 = theta*theta;
  theta3 = theta2*theta;
  theta4 = theta3*theta;
  theta5 = theta4*theta;

  frequency = theta3*ak->ftaN * (1.
             + ak->fta2*theta2
             + ak->fta3*theta3
             + ak->fta4*theta4
             + ak->fta5*theta5);

  return frequency;
}



REAL8
XLALInspiralFrequency3_6PN (
   REAL8       td,
   expnCoeffs *ak
   )
{
  REAL8 theta,theta2,theta3,theta4,theta5,theta6;
  REAL8 frequency;

  if (ak == NULL)
    XLAL_ERROR_REAL8(XLAL_EFAULT);

  theta = pow(td,-0.125);
  theta2 = theta*theta;
  theta3 = theta2*theta;
  theta4 = theta3*theta;
  theta5 = theta4*theta;
  theta6 = theta5*theta;

  frequency = theta3*ak->ftaN * (1.
             + ak->fta2*theta2
             + ak->fta3*theta3
             + ak->fta4*theta4
             + ak->fta5*theta5
             + (ak->fta6 + ak->ftl6*log(td))*theta6);

  return frequency;
}



REAL8
XLALInspiralFrequency3_7PN (
   REAL8       td,
   expnCoeffs *ak
   )
{
  REAL8 theta,theta2,theta3,theta4,theta5,theta6,theta7;
  REAL8 frequency;

  if (ak == NULL)
    XLAL_ERROR_REAL8(XLAL_EFAULT);

  theta = pow(td,-0.125);
  theta2 = theta*theta;
  theta3 = theta2*theta;
  theta4 = theta3*theta;
  theta5 = theta4*theta;
  theta6 = theta5*theta;
  theta7 = theta6*theta;

  frequency = theta3*ak->ftaN * (1.
             + ak->fta2*theta2
             + ak->fta3*theta3
             + ak->fta4*theta4
             + ak->fta5*theta5
             + (ak->fta6 + ak->ftl6*log(td))*theta6
             + ak->fta7*theta7);

  return frequency;
}
