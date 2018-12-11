/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

/** \file
 * \brief
 * File over EtherCAT (FoE) module.
 *
 * SDO read / write and SDO service functions
 */

#include <stdio.h>
#include <string.h>
#include "osal.h"
#include "oshw.h"
#include "ethercattype.h"
#include "ethercatbase.h"
#include "ethercatmain.h"
#include "ethercatfoe.h"

#define NEX_MAXFOEDATA 512

/** FOE structure.
 * Used for Read, Write, Data, Ack and Error mailbox packets.
 */
PACKED_BEGIN
typedef struct PACKED
{
   nex_mbxheadert MbxHeader;
   uint8         OpCode;
   uint8         Reserved;
   union
   {
      uint32        Password;
      uint32        PacketNumber;
      uint32        ErrorCode;
   };
   union
   {
      char          FileName[NEX_MAXFOEDATA];
      uint8         Data[NEX_MAXFOEDATA];
      char          ErrorText[NEX_MAXFOEDATA];
   };
} nex_FOEt;
PACKED_END

/** FoE progress hook.
 *
 * @param[in]  context        = context struct
 * @param[in]     hook       = Pointer to hook function.
 * @return 1
 */
int nexx_FOEdefinehook(nexx_contextt *context, void *hook)
{
  context->FOEhook = hook;
  return 1;
}

/** FoE read, blocking.
 *
 * @param[in]  context        = context struct
 * @param[in]     slave      = Slave number.
 * @param[in]     filename   = Filename of file to read.
 * @param[in]     password   = password.
 * @param[in,out] psize      = Size in bytes of file buffer, returns bytes read from file.
 * @param[out]    p          = Pointer to file buffer
 * @param[in]     timeout    = Timeout per mailbox cycle in us, standard is NEX_TIMEOUTRXM
 * @return Workcounter from last slave response
 */
int nexx_FOEread(nexx_contextt *context, uint16 slave, char *filename, uint32 password, int *psize, void *p, int timeout)
{
   nex_FOEt *FOEp, *aFOEp;
   int wkc;
   int32 dataread = 0;
   int32 buffersize, packetnumber, prevpacket = 0;
   uint16 fnsize, maxdata, segmentdata;
   nex_mbxbuft MbxIn, MbxOut;
   uint8 cnt;
   boolean worktodo;

   buffersize = *psize;
   nex_clearmbx(&MbxIn);
   /* Empty slave out mailbox if something is in. Timout set to 0 */
   wkc = nexx_mbxreceive(context, slave, (nex_mbxbuft *)&MbxIn, 0);
   nex_clearmbx(&MbxOut);
   aFOEp = (nex_FOEt *)&MbxIn;
   FOEp = (nex_FOEt *)&MbxOut;
   fnsize = (uint16)strlen(filename);
   maxdata = context->slavelist[slave].mbx_l - 12;
   if (fnsize > maxdata)
   {
      fnsize = maxdata;
   }
   FOEp->MbxHeader.length = htoes(0x0006 + fnsize);
   FOEp->MbxHeader.address = htoes(0x0000);
   FOEp->MbxHeader.priority = 0x00;
   /* get new mailbox count value, used as session handle */
   cnt = nex_nextmbxcnt(context->slavelist[slave].mbx_cnt);
   context->slavelist[slave].mbx_cnt = cnt;
   FOEp->MbxHeader.mbxtype = ECT_MBXT_FOE + (cnt << 4); /* FoE */
   FOEp->OpCode = ECT_FOE_READ;
   FOEp->Password = htoel(password);
   /* copy filename in mailbox */
   memcpy(&FOEp->FileName[0], filename, fnsize);
   /* send FoE request to slave */
   wkc = nexx_mbxsend(context, slave, (nex_mbxbuft *)&MbxOut, NEX_TIMEOUTTXM);
   if (wkc > 0) /* succeeded to place mailbox in slave ? */
   {
      do
      {
         worktodo = FALSE;
         /* clean mailboxbuffer */
         nex_clearmbx(&MbxIn);
         /* read slave response */
         wkc = nexx_mbxreceive(context, slave, (nex_mbxbuft *)&MbxIn, timeout);
         if (wkc > 0) /* succeeded to read slave response ? */
         {
            /* slave response should be FoE */
            if ((aFOEp->MbxHeader.mbxtype & 0x0f) == ECT_MBXT_FOE)
            {
               if(aFOEp->OpCode == ECT_FOE_DATA)
               {
                  segmentdata = etohs(aFOEp->MbxHeader.length) - 0x0006;
                  packetnumber = etohl(aFOEp->PacketNumber);
                  if ((packetnumber == ++prevpacket) && (dataread + segmentdata <= buffersize))
                  {
                     memcpy(p, &aFOEp->Data[0], segmentdata);
                     dataread += segmentdata;
                     p = (uint8 *)p + segmentdata;
                     if (segmentdata == maxdata)
                     {
                        worktodo = TRUE;
                     }
                     FOEp->MbxHeader.length = htoes(0x0006);
                     FOEp->MbxHeader.address = htoes(0x0000);
                     FOEp->MbxHeader.priority = 0x00;
                     /* get new mailbox count value */
                     cnt = nex_nextmbxcnt(context->slavelist[slave].mbx_cnt);
                     context->slavelist[slave].mbx_cnt = cnt;
                     FOEp->MbxHeader.mbxtype = ECT_MBXT_FOE + (cnt << 4); /* FoE */
                     FOEp->OpCode = ECT_FOE_ACK;
                     FOEp->PacketNumber = htoel(packetnumber);
                     /* send FoE ack to slave */
                     wkc = nexx_mbxsend(context, slave, (nex_mbxbuft *)&MbxOut, NEX_TIMEOUTTXM);
                     if (wkc <= 0)
                     {
                        worktodo = FALSE;
                     }
                     if (context->FOEhook)
                     {
                        context->FOEhook(slave, packetnumber, dataread);
                     }
                  }
                  else
                  {
                     /* FoE error */
                     wkc = -NEX_ERR_TYPE_FOE_BUF2SMALL;
                  }
               }
               else
               {
                  if(aFOEp->OpCode == ECT_FOE_ERROR)
                  {
                     /* FoE error */
                     wkc = -NEX_ERR_TYPE_FOE_ERROR;
                  }
                  else
                  {
                     /* unexpected mailbox received */
                     wkc = -NEX_ERR_TYPE_PACKET_ERROR;
                  }
               }
            }
            else
            {
               /* unexpected mailbox received */
               wkc = -NEX_ERR_TYPE_PACKET_ERROR;
            }
            *psize = dataread;
         }
      } while (worktodo);
   }

   return wkc;
}

/** FoE write, blocking.
 *
 * @param[in]  context        = context struct
 * @param[in]  slave      = Slave number.
 * @param[in]  filename   = Filename of file to write.
 * @param[in]  password   = password.
 * @param[in]  psize      = Size in bytes of file buffer.
 * @param[out] p          = Pointer to file buffer
 * @param[in]  timeout    = Timeout per mailbox cycle in us, standard is NEX_TIMEOUTRXM
 * @return Workcounter from last slave response
 */
int nexx_FOEwrite(nexx_contextt *context, uint16 slave, char *filename, uint32 password, int psize, void *p, int timeout)
{
   nex_FOEt *FOEp, *aFOEp;
   int wkc;
   int32 packetnumber, sendpacket = 0;
   uint16 fnsize, maxdata;
   int segmentdata;
   nex_mbxbuft MbxIn, MbxOut;
   uint8 cnt;
   boolean worktodo, dofinalzero;
   int tsize;

   nex_clearmbx(&MbxIn);
   /* Empty slave out mailbox if something is in. Timout set to 0 */
   wkc = nexx_mbxreceive(context, slave, (nex_mbxbuft *)&MbxIn, 0);
   nex_clearmbx(&MbxOut);
   aFOEp = (nex_FOEt *)&MbxIn;
   FOEp = (nex_FOEt *)&MbxOut;
   dofinalzero = FALSE;
   fnsize = (uint16)strlen(filename);
   maxdata = context->slavelist[slave].mbx_l - 12;
   if (fnsize > maxdata)
   {
      fnsize = maxdata;
   }
   FOEp->MbxHeader.length = htoes(0x0006 + fnsize);
   FOEp->MbxHeader.address = htoes(0x0000);
   FOEp->MbxHeader.priority = 0x00;
   /* get new mailbox count value, used as session handle */
   cnt = nex_nextmbxcnt(context->slavelist[slave].mbx_cnt);
   context->slavelist[slave].mbx_cnt = cnt;
   FOEp->MbxHeader.mbxtype = ECT_MBXT_FOE + (cnt << 4); /* FoE */
   FOEp->OpCode = ECT_FOE_WRITE;
   FOEp->Password = htoel(password);
   /* copy filename in mailbox */
   memcpy(&FOEp->FileName[0], filename, fnsize);
   /* send FoE request to slave */
   wkc = nexx_mbxsend(context, slave, (nex_mbxbuft *)&MbxOut, NEX_TIMEOUTTXM);
   if (wkc > 0) /* succeeded to place mailbox in slave ? */
   {
      do
      {
         worktodo = FALSE;
         /* clean mailboxbuffer */
         nex_clearmbx(&MbxIn);
         /* read slave response */
         wkc = nexx_mbxreceive(context, slave, (nex_mbxbuft *)&MbxIn, timeout);
         if (wkc > 0) /* succeeded to read slave response ? */
         {
            /* slave response should be FoE */
            if ((aFOEp->MbxHeader.mbxtype & 0x0f) == ECT_MBXT_FOE)
            {
               switch (aFOEp->OpCode)
               {
                  case ECT_FOE_ACK:
                  {
                     packetnumber = etohl(aFOEp->PacketNumber);
                     if (packetnumber == sendpacket)
                     {
                        if (context->FOEhook)
                        {
                           context->FOEhook(slave, packetnumber, psize);
                        }
                        tsize = psize;
                        if (tsize > maxdata)
                        {
                           tsize = maxdata;
                        }
                        if(tsize || dofinalzero)
                        {
                           worktodo = TRUE;
                           dofinalzero = FALSE;
                           segmentdata = tsize;
                           psize -= segmentdata;
                           /* if last packet was full size, add a zero size packet as final */
                           /* EOF is defined as packetsize < full packetsize */
                           if (!psize && (segmentdata == maxdata))
                           {
                              dofinalzero = TRUE;
                           }
                           FOEp->MbxHeader.length = htoes(0x0006 + segmentdata);
                           FOEp->MbxHeader.address = htoes(0x0000);
                           FOEp->MbxHeader.priority = 0x00;
                           /* get new mailbox count value */
                           cnt = nex_nextmbxcnt(context->slavelist[slave].mbx_cnt);
                           context->slavelist[slave].mbx_cnt = cnt;
                           FOEp->MbxHeader.mbxtype = ECT_MBXT_FOE + (cnt << 4); /* FoE */
                           FOEp->OpCode = ECT_FOE_DATA;
                           sendpacket++;
                           FOEp->PacketNumber = htoel(sendpacket);
                           memcpy(&FOEp->Data[0], p, segmentdata);
                           p = (uint8 *)p + segmentdata;
                           /* send FoE data to slave */
                           wkc = nexx_mbxsend(context, slave, (nex_mbxbuft *)&MbxOut, NEX_TIMEOUTTXM);
                           if (wkc <= 0)
                           {
                              worktodo = FALSE;
                           }
                        }
                     }
                     else
                     {
                        /* FoE error */
                        wkc = -NEX_ERR_TYPE_FOE_PACKETNUMBER;
                     }
                     break;
                  }
                  case ECT_FOE_BUSY:
                  {
                     /* resend if data has been send before */
                     /* otherwise ignore */
                     if (sendpacket)
                     {
                        if (!psize)
                        {
                           dofinalzero = TRUE;
                        }
                        psize += segmentdata;
                        p = (uint8 *)p - segmentdata;
                        --sendpacket;
                     }
                     break;
                  }
                  case ECT_FOE_ERROR:
                  {
                     /* FoE error */
                     if (aFOEp->ErrorCode == 0x8001)
                     {
                        wkc = -NEX_ERR_TYPE_FOE_FILE_NOTFOUND;
                     }
                     else
                     {
                        wkc = -NEX_ERR_TYPE_FOE_ERROR;
                     }
                     break;
                  }
                  default:
                  {
                     /* unexpected mailbox received */
                     wkc = -NEX_ERR_TYPE_PACKET_ERROR;
                     break;
                  }
               }
            }
            else
            {
               /* unexpected mailbox received */
               wkc = -NEX_ERR_TYPE_PACKET_ERROR;
            }
         }
      } while (worktodo);
   }

   return wkc;
}

#ifdef NEX_VER1
int nex_FOEdefinehook(void *hook)
{
   return nexx_FOEdefinehook(&nexx_context, hook);
}

int nex_FOEread(uint16 slave, char *filename, uint32 password, int *psize, void *p, int timeout)
{
   return nexx_FOEread(&nexx_context, slave, filename, password, psize, p, timeout);
}

int nex_FOEwrite(uint16 slave, char *filename, uint32 password, int psize, void *p, int timeout)
{
   return nexx_FOEwrite(&nexx_context, slave, filename, password, psize, p, timeout);
}
#endif
