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

int nexx_setupdatagram(nexx_portt *port, void *frame, uint8 com, uint8 idx, uint16 ADP, uint16 ADO, uint16 length, void *data);
int nexx_adddatagram(nexx_portt *port, void *frame, uint8 com, uint8 idx, boolean more, uint16 ADP, uint16 ADO, uint16 length, void *data);
int nexx_BWR(nexx_portt *port, uint16 ADP,uint16 ADO,uint16 length,void *data,int timeout);
int nexx_BRD(nexx_portt *port, uint16 ADP,uint16 ADO,uint16 length,void *data,int timeout);
int nexx_APRD(nexx_portt *port, uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
int nexx_ARMW(nexx_portt *port, uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
int nexx_FRMW(nexx_portt *port, uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
uint16 nexx_APRDw(nexx_portt *port, uint16 ADP, uint16 ADO, int timeout);
int nexx_FPRD(nexx_portt *port, uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
uint16 nexx_FPRDw(nexx_portt *port, uint16 ADP, uint16 ADO, int timeout);
int nexx_APWRw(nexx_portt *port, uint16 ADP, uint16 ADO, uint16 data, int timeout);
int nexx_APWR(nexx_portt *port, uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
int nexx_FPWRw(nexx_portt *port, uint16 ADP, uint16 ADO, uint16 data, int timeout);
int nexx_FPWR(nexx_portt *port, uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
int nexx_LRW(nexx_portt *port, uint32 LogAdr, uint16 length, void *data, int timeout);
int nexx_LRD(nexx_portt *port, uint32 LogAdr, uint16 length, void *data, int timeout);
int nexx_LWR(nexx_portt *port, uint32 LogAdr, uint16 length, void *data, int timeout);
int nexx_LRWDC(nexx_portt *port, uint32 LogAdr, uint16 length, void *data, uint16 DCrs, int64 *DCtime, int timeout);

#ifdef NEX_VER1
int nex_setupdatagram(void *frame, uint8 com, uint8 idx, uint16 ADP, uint16 ADO, uint16 length, void *data);
int nex_adddatagram(void *frame, uint8 com, uint8 idx, boolean more, uint16 ADP, uint16 ADO, uint16 length, void *data);
int nex_BWR(uint16 ADP,uint16 ADO,uint16 length,void *data,int timeout);
int nex_BRD(uint16 ADP,uint16 ADO,uint16 length,void *data,int timeout);
int nex_APRD(uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
int nex_ARMW(uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
int nex_FRMW(uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
uint16 nex_APRDw(uint16 ADP, uint16 ADO, int timeout);
int nex_FPRD(uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
uint16 nex_FPRDw(uint16 ADP, uint16 ADO, int timeout);
int nex_APWRw(uint16 ADP, uint16 ADO, uint16 data, int timeout);
int nex_APWR(uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
int nex_FPWRw(uint16 ADP, uint16 ADO, uint16 data, int timeout);
int nex_FPWR(uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout);
int nex_LRW(uint32 LogAdr, uint16 length, void *data, int timeout);
int nex_LRD(uint32 LogAdr, uint16 length, void *data, int timeout);
int nex_LWR(uint32 LogAdr, uint16 length, void *data, int timeout);
int nex_LRWDC(uint32 LogAdr, uint16 length, void *data, uint16 DCrs, int64 *DCtime, int timeout);
#endif

PACKED_BEGIN
typedef struct PACKED
{
	uint16 KeyWords;
	int32 TargetPosition;
	uint16 TouchProbeControlWords;
}drivercontrol_t;
PACKED_END

PACKED_BEGIN
typedef struct PACKED
{
	uint16 ErrorNumber;
	uint16 StatusWords;
	uint8  OperationMode;
	int32 ActualPosition;
	uint16 TouchProbeStatusWords;
	uint32 TouchProbeData1;
	uint32 IOStatus;

}driverstatus_t;
PACKED_END


#ifdef __cplusplus
}
#endif

#endif
