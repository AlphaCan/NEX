/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

/** \file
 * \brief
 * Headerfile for ethercatfoe.c
 */

#ifndef _ethercatfoe_
#define _ethercatfoe_

#ifdef __cplusplus
extern "C"
{
#endif


int Nex_FOEdefinehook(void *hook);
int Nex_FOEread(uint16 slave, char *filename, uint32 password, int *psize, void *p, int timeout);
int Nex_FOEwrite(uint16 slave, char *filename, uint32 password, int psize, void *p, int timeout);


int Nexx__FOEdefinehook(Nexx__contextt *context, void *hook);
int Nexx__FOEread(Nexx__contextt *context, uint16 slave, char *filename, uint32 password, int *psize, void *p, int timeout);
int Nexx__FOEwrite(Nexx__contextt *context, uint16 slave, char *filename, uint32 password, int psize, void *p, int timeout);

#ifdef __cplusplus
}
#endif

#endif
