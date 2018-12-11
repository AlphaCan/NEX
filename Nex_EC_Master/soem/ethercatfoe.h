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

#ifdef NEX_VER1
int nex_FOEdefinehook(void *hook);
int nex_FOEread(uint16 slave, char *filename, uint32 password, int *psize, void *p, int timeout);
int nex_FOEwrite(uint16 slave, char *filename, uint32 password, int psize, void *p, int timeout);
#endif

int nexx_FOEdefinehook(nexx_contextt *context, void *hook);
int nexx_FOEread(nexx_contextt *context, uint16 slave, char *filename, uint32 password, int *psize, void *p, int timeout);
int nexx_FOEwrite(nexx_contextt *context, uint16 slave, char *filename, uint32 password, int psize, void *p, int timeout);

#ifdef __cplusplus
}
#endif

#endif
