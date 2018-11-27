/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

/** \file
 * \brief
 * Headerfile for ethercatdc.c
 */

#ifndef _Nex_ECATDC_H
#define _Nex_ECATDC_H

#ifdef __cplusplus
extern "C"
{
#endif


boolean Nex_configdc();
void Nex_dcsync0(uint16 slave, boolean act, uint32 CyclTime, int32 CyclShift);
void Nex_dcsync01(uint16 slave, boolean act, uint32 CyclTime0, uint32 CyclTime1, int32 CyclShift);


boolean Nexx__configdc(Nexx__contextt *context);
void Nexx__dcsync0(Nexx__contextt *context, uint16 slave, boolean act, uint32 CyclTime, int32 CyclShift);
void Nexx__dcsync01(Nexx__contextt *context, uint16 slave, boolean act, uint32 CyclTime0, uint32 CyclTime1, int32 CyclShift);

#ifdef __cplusplus
}
#endif

#endif /* _Nex_ECATDC_H */
