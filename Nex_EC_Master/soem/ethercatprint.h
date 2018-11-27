/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

/** \file
 * \brief
 * Headerfile for ethercatprint.c
 */

#ifndef _ethercatprint_
#define _ethercatprint_

#ifdef __cplusplus
extern "C"
{
#endif

char* Nex_sdoerror2string( uint32 sdoerrorcode);
char* Nex_ALstatuscode2string( uint16 ALstatuscode);
char* Nex_soeerror2string( uint16 errorcode);
char* Nexx__elist2string(Nexx__contextt *context);


char* Nex_elist2string(void);


#ifdef __cplusplus
}
#endif

#endif
