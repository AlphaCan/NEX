/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

/** \file
 * \brief
 * Headerfile for ethercatconfig.c
 */

#ifndef _ethercatconfig_
#define _ethercatconfig_

#ifdef __cplusplus
extern "C"
{
#endif

#define NEX_NODEOFFSET      0x1000
#define NEX_TEMPNODE        0xffff

#ifdef NEX_VER1
int nex_config_init();
int nex_config_map(void *pIOmap);
int nex_config_overlap_map(void *pIOmap);
int nex_config_map_group(void *pIOmap, uint8 group);
int nex_config_overlap_map_group(void *pIOmap, uint8 group);
int nex_config(void *pIOmap);
int nex_config_overlap(void *pIOmap);
int nex_recover_slave(uint16 slave, int timeout);
int nex_reconfig_slave(uint16 slave, int timeout);
#endif

int nexx_config_init(nexx_contextt *context);
int nexx_config_map_group(nexx_contextt *context, void *pIOmap, uint8 group);
int nexx_config_overlap_map_group(nexx_contextt *context, void *pIOmap, uint8 group);
int nexx_recover_slave(nexx_contextt *context, uint16 slave, int timeout);
int nexx_reconfig_slave(nexx_contextt *context, uint16 slave, int timeout);

#ifdef __cplusplus
}
#endif

#endif
