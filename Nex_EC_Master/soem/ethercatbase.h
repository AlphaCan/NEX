/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

/** \file
 * \brief
 * Headerfile for ethercatbase.c
 */

#ifndef _ethercatbase_
#define _ethercatbase_

#ifdef __cplusplus
extern "C"
{
#endif

int Nexx__setupdatagram(Nexx__portt *port, void *frame, uint8 com, uint8 idx, uint16 ADP, uint16 ADO, uint16 length, void *data);
int Nexx__adddatagram(Nexx__portt *port, void *frame, uint8 com, uint8 idx, boolean more, uint16 ADP, uint16 ADO, uint16 length, void *data);
int Nexx__BWR(Nexx__portt *port, uint16 ADP,uint16 ADO,uint16 length,void *data,int timeout);
int Nexx__BRD(Nexx__portt *port, uint16 ADP,uint16 ADO,uint16 length,void *data,int timeout);
int Nexx__APRD(Nexx__portt *port, uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
int Nexx__ARMW(Nexx__portt *port, uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
int Nexx__FRMW(Nexx__portt *port, uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
uint16 Nexx__APRDw(Nexx__portt *port, uint16 ADP, uint16 ADO, int timeout);
int Nexx__FPRD(Nexx__portt *port, uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
uint16 Nexx__FPRDw(Nexx__portt *port, uint16 ADP, uint16 ADO, int timeout);
int Nexx__APWRw(Nexx__portt *port, uint16 ADP, uint16 ADO, uint16 data, int timeout);
int Nexx__APWR(Nexx__portt *port, uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
int Nexx__FPWRw(Nexx__portt *port, uint16 ADP, uint16 ADO, uint16 data, int timeout);
int Nexx__FPWR(Nexx__portt *port, uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
int Nexx__LRW(Nexx__portt *port, uint32 LogAdr, uint16 length, void *data, int timeout);
int Nexx__LRD(Nexx__portt *port, uint32 LogAdr, uint16 length, void *data, int timeout);
int Nexx__LWR(Nexx__portt *port, uint32 LogAdr, uint16 length, void *data, int timeout);
int Nexx__LRWDC(Nexx__portt *port, uint32 LogAdr, uint16 length, void *data, uint16 DCrs, int64 *DCtime, int timeout);


int Nex_setupdatagram(void *frame, uint8 com, uint8 idx, uint16 ADP, uint16 ADO, uint16 length, void *data);
int Nex_adddatagram(void *frame, uint8 com, uint8 idx, boolean more, uint16 ADP, uint16 ADO, uint16 length, void *data);
int Nex_BWR(uint16 ADP,uint16 ADO,uint16 length,void *data,int timeout);
int Nex_BRD(uint16 ADP,uint16 ADO,uint16 length,void *data,int timeout);
int Nex_APRD(uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
int Nex_ARMW(uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
int Nex_FRMW(uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
uint16 Nex_APRDw(uint16 ADP, uint16 ADO, int timeout);
int Nex_FPRD(uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
uint16 Nex_FPRDw(uint16 ADP, uint16 ADO, int timeout);
int Nex_APWRw(uint16 ADP, uint16 ADO, uint16 data, int timeout);
int Nex_APWR(uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
int Nex_FPWRw(uint16 ADP, uint16 ADO, uint16 data, int timeout);
int Nex_FPWR(uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
int Nex_LRW(uint32 LogAdr, uint16 length, void *data, int timeout);
int Nex_LRD(uint32 LogAdr, uint16 length, void *data, int timeout);
int Nex_LWR(uint32 LogAdr, uint16 length, void *data, int timeout);
int Nex_LRWDC(uint32 LogAdr, uint16 length, void *data, uint16 DCrs, int64 *DCtime, int timeout);


#ifdef __cplusplus
}
#endif

#endif
