/** 
 * 
 * 实现EtherCAT最基本的功能
 *
 *在以太网帧中设置数据报。
 * EtherCAT数据报原语，广播，自动增量，配置和
 *逻辑寻址数据传输。 所有基本传输都是阻塞的，因此请等待帧返回主服务器或超时。
 */

#include <stdio.h>
#include <string.h>
#include "oshw.h"
#include "osal.h"
#include "ethercattype.h"
#include "ethercatbase.h"

/** 将数据写进数据报
 *
 * @param[out] datagramdata   数据部分
 * @param[in]  com            命令
 * @param[in]  length         写入的数据长度
 * @param[in]  data           要写到数据报中的数据
 */
static void Nexx__writedatagramdata(void *datagramdata, Nex_cmdtype com, uint16 length, const void * data)
{
   if (length > 0)
   {
      switch (com)
      {
		  /*读取数据的话，需要将数据报清空，以免产生错误数据*/
         case Nex_CMD_NOP:          
         case Nex_CMD_APRD:       
         case Nex_CMD_FPRD:
         case Nex_CMD_BRD:
         case Nex_CMD_LRD:       
            memset(datagramdata, 0, length);
            break;
         default:
            memcpy(datagramdata, data, length);
            break;
      }
   }
}

/** 创建一个标准的EtherCAT数据报文
 *
 * @param[in] port         port结构体
 * @param[out] frame       数据报
 * @param[in]  com         命令
 * @param[in]  idx         数据发送与接收的索引，避免丢数据
 * @param[in]  ADP         寄存器地址
 * @param[in]  ADO         地址偏移
 * @param[in]  length      除去EtherCAT报头外的数据长度
 * @param[in]  data        准备写到报文中数据
 * @return always 0
 */
int Nexx__setupdatagram(Nexx__portt *port, void *frame, uint8 com, uint8 idx, uint16 ADP, uint16 ADO, uint16 length, void *data)
{
   Nex_comt *datagramP;
   uint8 *frameP;

   frameP = frame;
   datagramP = (Nex_comt*)&frameP[ETH_HEADERSIZE];
   datagramP->elength = htoes(Nex_ECATTYPE + Nex_HEADERSIZE + length);
   datagramP->command = com;
   datagramP->index = idx;
   datagramP->ADP = htoes(ADP);
   datagramP->ADO = htoes(ADO);
   datagramP->dlength = htoes(length);
   Nexx__writedatagramdata(&frameP[ETH_HEADERSIZE + Nex_HEADERSIZE], com, length, data);
   /* set WKC to zero */
   frameP[ETH_HEADERSIZE + Nex_HEADERSIZE + length] = 0x00;
   frameP[ETH_HEADERSIZE + Nex_HEADERSIZE + length + 1] = 0x00;
   /* set size of frame in buffer array */
   port->txbuflength[idx] = ETH_HEADERSIZE + Nex_HEADERSIZE + Nex_WKCSIZE + length;

   return 0;
}

/** Add EtherCAT datagram to a standard ethernet frame with existing datagram(s).
 *
 * @param[in] port        = port context struct
 * @param[out] frame      = framebuffer
 * @param[in]  com        = command
 * @param[in]  idx        = index used for TX and RX buffers
 * @param[in]  more       = TRUE if still more datagrams to follow
 * @param[in]  ADP        = Address Position
 * @param[in]  ADO        = Address Offset
 * @param[in]  length     = length of datagram excluding EtherCAT header
 * @param[in]  data       = databuffer to be copied in datagram
 * @return Offset to data in rx frame, usefull to retrieve data after RX.
 */
int Nexx__adddatagram(Nexx__portt *port, void *frame, uint8 com, uint8 idx, boolean more, uint16 ADP, uint16 ADO, uint16 length, void *data)
{
   Nex_comt *datagramP;
   uint8 *frameP;
   uint16 prevlength;

   frameP = frame;
   /* copy previous frame size */
   prevlength = port->txbuflength[idx];
   datagramP = (Nex_comt*)&frameP[ETH_HEADERSIZE];
   /* add new datagram to ethernet frame size */
   datagramP->elength = htoes( etohs(datagramP->elength) + Nex_HEADERSIZE + length );
   /* add "datagram follows" flag to previous subframe dlength */
   datagramP->dlength = htoes( etohs(datagramP->dlength) | Nex_DATAGRAMFOLLOWS );
   /* set new EtherCAT header position */
   datagramP = (Nex_comt*)&frameP[prevlength - Nex_ELENGTHSIZE];
   datagramP->command = com;
   datagramP->index = idx;
   datagramP->ADP = htoes(ADP);
   datagramP->ADO = htoes(ADO);
   if (more)
   {
      /* this is not the last datagram to add */
      datagramP->dlength = htoes(length | Nex_DATAGRAMFOLLOWS);
   }
   else
   {
      /* this is the last datagram in the frame */
      datagramP->dlength = htoes(length);
   }
   Nexx__writedatagramdata(&frameP[prevlength + Nex_HEADERSIZE - Nex_ELENGTHSIZE], com, length, data);
   /* set WKC to zero */
   frameP[prevlength + Nex_HEADERSIZE - Nex_ELENGTHSIZE + length] = 0x00;
   frameP[prevlength + Nex_HEADERSIZE - Nex_ELENGTHSIZE + length + 1] = 0x00;
   /* set size of frame in buffer array */
   port->txbuflength[idx] = prevlength + Nex_HEADERSIZE - Nex_ELENGTHSIZE + Nex_WKCSIZE + length;

   /* return offset to data in rx frame
      14 bytes smaller than tx frame due to stripping of ethernet header */
   return prevlength + Nex_HEADERSIZE - Nex_ELENGTHSIZE - ETH_HEADERSIZE;
}

/** BRW "broadcast write" primitive. Blocking.
 *
 * @param[in] port        = port context struct
 * @param[in] ADP         = Address Position, normally 0
 * @param[in] ADO         = Address Offset, slave memory address
 * @param[in] length      = length of databuffer
 * @param[in] data        = databuffer to be written to slaves
 * @param[in] timeout     = timeout in us, standard is Nex_TIMEOUTRET
 * @return Workcounter or Nex_NOFRAME
 */
int Nexx__BWR (Nexx__portt *port, uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout)
{
   uint8 idx;
   int wkc;

   /* get fresh index */
   idx = Nexx__getindex (port);
   /* setup datagram */
   Nexx__setupdatagram (port, &(port->txbuf[idx]), Nex_CMD_BWR, idx, ADP, ADO, length, data);
   /* send data and wait for answer */
   wkc = Nexx__srconfirm (port, idx, timeout);
   /* clear buffer status */
   Nexx__setbufstat (port, idx, Nex_BUF_EMPTY);

   return wkc;
}

/** BRD "broadcast read" primitive. Blocking.
 *
 * @param[in] port        = port context struct
 * @param[in]  ADP        = Address Position, normally 0
 * @param[in]  ADO        = Address Offset, slave memory address
 * @param[in]  length     = length of databuffer
 * @param[out] data       = databuffer to put slave data in
 * @param[in]  timeout    = timeout in us, standard is Nex_TIMEOUTRET
 * @return Workcounter or Nex_NOFRAME
 */
int Nexx__BRD(Nexx__portt *port, uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout)
{
   uint8 idx;
   int wkc;

   /* get fresh index */
   idx = Nexx__getindex(port);
   /* setup datagram */
   Nexx__setupdatagram(port, &(port->txbuf[idx]), Nex_CMD_BRD, idx, ADP, ADO, length, data);
   /* send data and wait for answer */
   wkc = Nexx__srconfirm (port, idx, timeout);
   if (wkc > 0)
   {
      /* copy datagram to data buffer */
      memcpy(data, &(port->rxbuf[idx][Nex_HEADERSIZE]), length);
   }
   /* clear buffer status */
   Nexx__setbufstat(port, idx, Nex_BUF_EMPTY);

   return wkc;
}

/** APRD "auto increment address read" primitive. Blocking.
 *
 * @param[in] port        = port context struct
 * @param[in]  ADP        = Address Position, each slave ++, slave that has 0 excecutes
 * @param[in]  ADO        = Address Offset, slave memory address
 * @param[in]  length     = length of databuffer
 * @param[out] data       = databuffer to put slave data in
 * @param[in]  timeout    = timeout in us, standard is Nex_TIMEOUTRET
 * @return Workcounter or Nex_NOFRAME
 */
int Nexx__APRD(Nexx__portt *port, uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout)
{
   int wkc;
   uint8 idx;

   idx = Nexx__getindex(port);
   Nexx__setupdatagram(port, &(port->txbuf[idx]), Nex_CMD_APRD, idx, ADP, ADO, length, data);
   wkc = Nexx__srconfirm(port, idx, timeout);
   if (wkc > 0)
   {
      memcpy(data, &(port->rxbuf[idx][Nex_HEADERSIZE]), length);
   }
   Nexx__setbufstat(port, idx, Nex_BUF_EMPTY);

   return wkc;
}

/** APRMW "auto increment address read, multiple write" primitive. Blocking.
 *
 * @param[in] port        = port context struct
 * @param[in]  ADP        = Address Position, each slave ++, slave that has 0 reads,
 *                          following slaves write.
 * @param[in]  ADO        = Address Offset, slave memory address
 * @param[in]  length     = length of databuffer
 * @param[out] data       = databuffer to put slave data in
 * @param[in]  timeout    = timeout in us, standard is Nex_TIMEOUTRET
 * @return Workcounter or Nex_NOFRAME
 */
int Nexx__ARMW(Nexx__portt *port, uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout)
{
   int wkc;
   uint8 idx;

   idx = Nexx__getindex(port);
   Nexx__setupdatagram(port, &(port->txbuf[idx]), Nex_CMD_ARMW, idx, ADP, ADO, length, data);
   wkc = Nexx__srconfirm(port, idx, timeout);
   if (wkc > 0)
   {
      memcpy(data, &(port->rxbuf[idx][Nex_HEADERSIZE]), length);
   }
   Nexx__setbufstat(port, idx, Nex_BUF_EMPTY);

   return wkc;
}

/** FPRMW "configured address read, multiple write" primitive. Blocking.
 *
 * @param[in] port        = port context struct
 * @param[in]  ADP        = Address Position, slave that has address reads,
 *                          following slaves write.
 * @param[in]  ADO        = Address Offset, slave memory address
 * @param[in]  length     = length of databuffer
 * @param[out] data       = databuffer to put slave data in
 * @param[in]  timeout    = timeout in us, standard is Nex_TIMEOUTRET
 * @return Workcounter or Nex_NOFRAME
 */
int Nexx__FRMW(Nexx__portt *port, uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout)
{
   int wkc;
   uint8 idx;

   idx = Nexx__getindex(port);
   Nexx__setupdatagram(port, &(port->txbuf[idx]), Nex_CMD_FRMW, idx, ADP, ADO, length, data);
   wkc = Nexx__srconfirm(port, idx, timeout);
   if (wkc > 0)
   {
      memcpy(data, &(port->rxbuf[idx][Nex_HEADERSIZE]), length);
   }
   Nexx__setbufstat(port, idx, Nex_BUF_EMPTY);

   return wkc;
}

/** APRDw "auto increment address read" word return primitive. Blocking.
 *
 * @param[in] port        = port context struct
 * @param[in] ADP         = Address Position, each slave ++, slave that has 0 reads.
 * @param[in] ADO         = Address Offset, slave memory address
 * @param[in] timeout     = timeout in us, standard is Nex_TIMEOUTRET
 * @return word data from slave
 */
uint16 Nexx__APRDw(Nexx__portt *port, uint16 ADP, uint16 ADO, int timeout)
{
   uint16 w;

   w = 0;
   Nexx__APRD(port, ADP, ADO, sizeof(w), &w, timeout);

   return w;
}

/** FPRD "configured address read" primitive. Blocking.
 *
 * @param[in] port        = port context struct
 * @param[in]  ADP        = Address Position, slave that has address reads.
 * @param[in]  ADO        = Address Offset, slave memory address
 * @param[in]  length     = length of databuffer
 * @param[out] data       = databuffer to put slave data in
 * @param[in]  timeout    = timeout in us, standard is Nex_TIMEOUTRET
 * @return Workcounter or Nex_NOFRAME
 */
int Nexx__FPRD(Nexx__portt *port, uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout)
{
   int wkc;
   uint8 idx;

   idx = Nexx__getindex(port);
   Nexx__setupdatagram(port, &(port->txbuf[idx]), Nex_CMD_FPRD, idx, ADP, ADO, length, data);
   wkc = Nexx__srconfirm(port, idx, timeout);
   if (wkc > 0)
   {
      memcpy(data, &(port->rxbuf[idx][Nex_HEADERSIZE]), length);
   }
   Nexx__setbufstat(port, idx, Nex_BUF_EMPTY);

   return wkc;
}

/** FPRDw "configured address read" word return primitive. Blocking.
 *
 * @param[in] port        = port context struct
 * @param[in] ADP         = Address Position, slave that has address reads.
 * @param[in] ADO         = Address Offset, slave memory address
 * @param[in] timeout     = timeout in us, standard is Nex_TIMEOUTRET
 * @return word data from slave
 */
uint16 Nexx__FPRDw(Nexx__portt *port, uint16 ADP, uint16 ADO, int timeout)
{
   uint16 w;

   w = 0;
   Nexx__FPRD(port, ADP, ADO, sizeof(w), &w, timeout);
   return w;
}

/** APWR "auto increment address write" primitive. Blocking.
 *
 * @param[in] port        = port context struct
 * @param[in] ADP         = Address Position, each slave ++, slave that has 0 writes.
 * @param[in] ADO         = Address Offset, slave memory address
 * @param[in] length      = length of databuffer
 * @param[in] data        = databuffer to write to slave.
 * @param[in] timeout     = timeout in us, standard is Nex_TIMEOUTRET
 * @return Workcounter or Nex_NOFRAME
 */
int Nexx__APWR(Nexx__portt *port, uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout)
{
   uint8 idx;
   int wkc;

   idx = Nexx__getindex(port);
   Nexx__setupdatagram(port, &(port->txbuf[idx]), Nex_CMD_APWR, idx, ADP, ADO, length, data);
   wkc = Nexx__srconfirm(port, idx, timeout);
   Nexx__setbufstat(port, idx, Nex_BUF_EMPTY);

   return wkc;
}

/** APWRw "auto increment address write" word primitive. Blocking.
 *
 * @param[in] port        = port context struct
 * @param[in] ADP         = Address Position, each slave ++, slave that has 0 writes.
 * @param[in] ADO         = Address Offset, slave memory address
 * @param[in] data        = word data to write to slave.
 * @param[in] timeout     = timeout in us, standard is Nex_TIMEOUTRET
 * @return Workcounter or Nex_NOFRAME
 */
int Nexx__APWRw(Nexx__portt *port, uint16 ADP, uint16 ADO, uint16 data, int timeout)
{
   return Nexx__APWR(port, ADP, ADO, sizeof(data), &data, timeout);
}

/** FPWR "configured address write" primitive. Blocking.
 *
 * @param[in] port        = port context struct
 * @param[in] ADP         = Address Position, slave that has address writes.
 * @param[in] ADO         = Address Offset, slave memory address
 * @param[in] length      = length of databuffer
 * @param[in] data        = databuffer to write to slave.
 * @param[in] timeout     = timeout in us, standard is Nex_TIMEOUTRET
 * @return Workcounter or Nex_NOFRAME
 */
int Nexx__FPWR(Nexx__portt *port, uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout)
{
   int wkc;
   uint8 idx;

   idx = Nexx__getindex(port);
   Nexx__setupdatagram(port, &(port->txbuf[idx]), Nex_CMD_FPWR, idx, ADP, ADO, length, data);
   wkc = Nexx__srconfirm(port, idx, timeout);
   Nexx__setbufstat(port, idx, Nex_BUF_EMPTY);

   return wkc;
}

/** FPWR "configured address write" primitive. Blocking.
 *
 * @param[in] port        = port context struct
 * @param[in] ADP         = Address Position, slave that has address writes.
 * @param[in] ADO         = Address Offset, slave memory address
 * @param[in] data        = word to write to slave.
 * @param[in] timeout     = timeout in us, standard is Nex_TIMEOUTRET
 * @return Workcounter or Nex_NOFRAME
 */
int Nexx__FPWRw(Nexx__portt *port, uint16 ADP, uint16 ADO, uint16 data, int timeout)
{
   return Nexx__FPWR(port, ADP, ADO, sizeof(data), &data, timeout);
}

/** LRW "logical memory read / write" primitive. Blocking.
 *
 * @param[in] port        = port context struct
 * @param[in]     LogAdr  = Logical memory address
 * @param[in]     length  = length of databuffer
 * @param[in,out] data    = databuffer to write to and read from slave.
 * @param[in]     timeout = timeout in us, standard is Nex_TIMEOUTRET
 * @return Workcounter or Nex_NOFRAME
 */
int Nexx__LRW(Nexx__portt *port, uint32 LogAdr, uint16 length, void *data, int timeout)
{
   uint8 idx;
   int wkc;

   idx = Nexx__getindex(port);
   Nexx__setupdatagram(port, &(port->txbuf[idx]), Nex_CMD_LRW, idx, LO_WORD(LogAdr), HI_WORD(LogAdr), length, data);
   wkc = Nexx__srconfirm(port, idx, timeout);
   if ((wkc > 0) && (port->rxbuf[idx][Nex_CMDOFFSET] == Nex_CMD_LRW))
   {
      memcpy(data, &(port->rxbuf[idx][Nex_HEADERSIZE]), length);
   }
   Nexx__setbufstat(port, idx, Nex_BUF_EMPTY);

   return wkc;
}

/** LRD "logical memory read" primitive. Blocking.
 *
 * @param[in] port        = port context struct
 * @param[in]  LogAdr     = Logical memory address
 * @param[in]  length     = length of bytes to read from slave.
 * @param[out] data       = databuffer to read from slave.
 * @param[in]  timeout    = timeout in us, standard is Nex_TIMEOUTRET
 * @return Workcounter or Nex_NOFRAME
 */
int Nexx__LRD(Nexx__portt *port, uint32 LogAdr, uint16 length, void *data, int timeout)
{
   uint8 idx;
   int wkc;

   idx = Nexx__getindex(port);
   Nexx__setupdatagram(port, &(port->txbuf[idx]), Nex_CMD_LRD, idx, LO_WORD(LogAdr), HI_WORD(LogAdr), length, data);
   wkc = Nexx__srconfirm(port, idx, timeout);
   if ((wkc > 0) && (port->rxbuf[idx][Nex_CMDOFFSET]==Nex_CMD_LRD))
   {
      memcpy(data, &(port->rxbuf[idx][Nex_HEADERSIZE]), length);
   }
   Nexx__setbufstat(port, idx, Nex_BUF_EMPTY);

   return wkc;
}

/** LWR "logical memory write" primitive. Blocking.
 *
 * @param[in] port        = port context struct
 * @param[in] LogAdr      = Logical memory address
 * @param[in] length      = length of databuffer
 * @param[in] data        = databuffer to write to slave.
 * @param[in] timeout     = timeout in us, standard is Nex_TIMEOUTRET
 * @return Workcounter or Nex_NOFRAME
 */
int Nexx__LWR(Nexx__portt *port, uint32 LogAdr, uint16 length, void *data, int timeout)
{
   uint8 idx;
   int wkc;

   idx = Nexx__getindex(port);
   Nexx__setupdatagram(port, &(port->txbuf[idx]), Nex_CMD_LWR, idx, LO_WORD(LogAdr), HI_WORD(LogAdr), length, data);
   wkc = Nexx__srconfirm(port, idx, timeout);
   Nexx__setbufstat(port, idx, Nex_BUF_EMPTY);

   return wkc;
}

/** LRW "logical memory read / write" primitive plus Clock Distribution. Blocking.
 * Frame consists of two datagrams, one LRW and one FPRMW.
 *
 * @param[in] port        = port context struct
 * @param[in]     LogAdr  = Logical memory address
 * @param[in]     length  = length of databuffer
 * @param[in,out] data    = databuffer to write to and read from slave.
 * @param[in]     DCrs    = Distributed Clock reference slave address.
 * @param[out]    DCtime  = DC time read from reference slave.
 * @param[in]     timeout = timeout in us, standard is Nex_TIMEOUTRET
 * @return Workcounter or Nex_NOFRAME
 */
int Nexx__LRWDC(Nexx__portt *port, uint32 LogAdr, uint16 length, void *data, uint16 DCrs, int64 *DCtime, int timeout)
{
   uint16 DCtO;
   uint8 idx;
   int wkc;
   uint64 DCtE;

   idx = Nexx__getindex(port);
   /* LRW in first datagram */
   Nexx__setupdatagram(port, &(port->txbuf[idx]), Nex_CMD_LRW, idx, LO_WORD(LogAdr), HI_WORD(LogAdr), length, data);
   /* FPRMW in second datagram */
   DCtE = htoell(*DCtime);
   DCtO = Nexx__adddatagram(port, &(port->txbuf[idx]), Nex_CMD_FRMW, idx, FALSE, DCrs, ECT_REG_DCSYSTIME, sizeof(DCtime), &DCtE);
   wkc = Nexx__srconfirm(port, idx, timeout);
   if ((wkc > 0) && (port->rxbuf[idx][Nex_CMDOFFSET] == Nex_CMD_LRW))
   {
      memcpy(data, &(port->rxbuf[idx][Nex_HEADERSIZE]), length);
      memcpy(&wkc, &(port->rxbuf[idx][Nex_HEADERSIZE + length]), Nex_WKCSIZE);
      memcpy(&DCtE, &(port->rxbuf[idx][DCtO]), sizeof(*DCtime));
      *DCtime = etohll(DCtE);
   }
   Nexx__setbufstat(port, idx, Nex_BUF_EMPTY);

   return wkc;
}


int Nex_setupdatagram(void *frame, uint8 com, uint8 idx, uint16 ADP, uint16 ADO, uint16 length, void *data)
{
   return Nexx__setupdatagram (&Nexx__port, frame, com, idx, ADP, ADO, length, data);
}

int Nex_adddatagram (void *frame, uint8 com, uint8 idx, boolean more, uint16 ADP, uint16 ADO, uint16 length, void *data)
{
   return Nexx__adddatagram (&Nexx__port, frame, com, idx, more, ADP, ADO, length, data);
}

int Nex_BWR(uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout)
{
   return Nexx__BWR (&Nexx__port, ADP, ADO, length, data, timeout);
}

int Nex_BRD(uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout)
{
   return Nexx__BRD(&Nexx__port, ADP, ADO, length, data, timeout);
}

int Nex_APRD(uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout)
{
   return Nexx__APRD(&Nexx__port, ADP, ADO, length, data, timeout);
}

int Nex_ARMW(uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout)
{
   return Nexx__ARMW(&Nexx__port, ADP, ADO, length, data, timeout);
}

int Nex_FRMW(uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout)
{
   return Nexx__FRMW(&Nexx__port, ADP, ADO, length, data, timeout);
}

uint16 Nex_APRDw(uint16 ADP, uint16 ADO, int timeout)
{
   uint16 w;

   w = 0;
   Nex_APRD(ADP, ADO, sizeof(w), &w, timeout);

   return w;
}

int Nex_FPRD(uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout)
{
   return Nexx__FPRD(&Nexx__port, ADP, ADO, length, data, timeout);
}

uint16 Nex_FPRDw(uint16 ADP, uint16 ADO, int timeout)
{
   uint16 w;

   w = 0;
   Nex_FPRD(ADP, ADO, sizeof(w), &w, timeout);
   return w;
}

int Nex_APWR(uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout)
{
   return Nexx__APWR(&Nexx__port, ADP, ADO, length, data, timeout);
}

int Nex_APWRw(uint16 ADP, uint16 ADO, uint16 data, int timeout)
{
   return Nex_APWR(ADP, ADO, sizeof(data), &data, timeout);
}

int Nex_FPWR(uint16 ADP, uint16 ADO, uint16 length, void *data, int timeout)
{
   return Nexx__FPWR(&Nexx__port, ADP, ADO, length, data, timeout);
}

int Nex_FPWRw(uint16 ADP, uint16 ADO, uint16 data, int timeout)
{
   return Nex_FPWR(ADP, ADO, sizeof(data), &data, timeout);
}

int Nex_LRW(uint32 LogAdr, uint16 length, void *data, int timeout)
{
   return Nexx__LRW(&Nexx__port, LogAdr, length, data, timeout);
}

int Nex_LRD(uint32 LogAdr, uint16 length, void *data, int timeout)
{
   return Nexx__LRD(&Nexx__port, LogAdr, length, data, timeout);
}

int Nex_LWR(uint32 LogAdr, uint16 length, void *data, int timeout)
{
   return Nexx__LWR(&Nexx__port, LogAdr, length, data, timeout);
}

int Nex_LRWDC(uint32 LogAdr, uint16 length, void *data, uint16 DCrs, int64 *DCtime, int timeout)
{
   return Nexx__LRWDC(&Nexx__port, LogAdr, length, data, DCrs, DCtime, timeout);
}
