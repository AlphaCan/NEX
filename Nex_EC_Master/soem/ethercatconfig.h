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

#define Nex_NODEOFFSET      0x1000
#define Nex_TEMPNODE        0xffff


//int Nex_config_init(uint8 usetable);
int Nex_config_init(void);
int Nex_config_map(void *pIOmap);
int Nex_config_overlap_map(void *pIOmap);
int Nex_config_map_group(void *pIOmap, uint8 group);
int Nex_config_overlap_map_group(void *pIOmap, uint8 group);
int Nex_config(void *pIOmap);
int Nex_config_overlap(void *pIOmap);
int Nex_recover_slave(uint16 slave, int timeout);
int Nex_reconfig_slave(uint16 slave, int timeout);


//int Nexx__config_init(Nexx__contextt *context, uint8 usetable);
int Nexx__config_init(Nexx__contextt *context);
int Nexx__config_map_group(Nexx__contextt *context, void *pIOmap, uint8 group);
int Nexx__config_overlap_map_group(Nexx__contextt *context, void *pIOmap, uint8 group);
int Nexx__recover_slave(Nexx__contextt *context, uint16 slave, int timeout);
int Nexx__reconfig_slave(Nexx__contextt *context, uint16 slave, int timeout);

#ifdef __cplusplus
}
#endif

#endif
