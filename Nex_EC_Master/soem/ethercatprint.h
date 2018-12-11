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

char* nex_sdoerror2string( uint32 sdoerrorcode);
char* nex_ALstatuscode2string( uint16 ALstatuscode);
char* nex_soeerror2string( uint16 errorcode);
char* nexx_elist2string(nexx_contextt *context);

#ifdef NEX_VER1
char* nex_elist2string(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
