/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

/** \file
 * \brief
 * Headerfile for ethercatdc.c
 */

#ifndef _NEX_ECATDC_H
#define _NEX_ECATDC_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef NEX_VER1
boolean nex_configdc();
void nex_dcsync0(uint16 slave, boolean act, uint32 CyclTime, int32 CyclShift);
void nex_dcsync01(uint16 slave, boolean act, uint32 CyclTime0, uint32 CyclTime1, int32 CyclShift);
#endif

boolean nexx_configdc(nexx_contextt *context);
void nexx_dcsync0(nexx_contextt *context, uint16 slave, boolean act, uint32 CyclTime, int32 CyclShift);
void nexx_dcsync01(nexx_contextt *context, uint16 slave, boolean act, uint32 CyclTime0, uint32 CyclTime1, int32 CyclShift);

#ifdef __cplusplus
}
#endif

#endif /* _NEX_ECATDC_H */
