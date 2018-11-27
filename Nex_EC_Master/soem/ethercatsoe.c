/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

/** \file
 * \brief
 * Servo over EtherCAT (SoE) Module.
 */

#include <stdio.h>
#include <string.h>
#include "osal.h"
#include "oshw.h"
#include "ethercattype.h"
#include "ethercatbase.h"
#include "ethercatmain.h"
#include "ethercatsoe.h"

#define Nex_SOE_MAX_DRIVES 8

/** SoE (Servo over EtherCAT) mailbox structure */
PACKED_BEGIN
typedef struct PACKED
{
   Nex_mbxheadert MbxHeader;
   uint8         opCode         :3;
   uint8         incomplete     :1;
   uint8         error          :1;
   uint8         driveNo        :3;
   uint8         elementflags;
   union
   {
      uint16     idn;
      uint16     fragmentsleft;
   };
} Nex_SoEt;
PACKED_END

/** Report SoE error.
 *
 * @param[in]  context        = context struct
 * @param[in]  Slave      = Slave number
 * @param[in]  idn        = IDN that generated error
 * @param[in]  Error      = Error code, see EtherCAT documentation for list
 */
void Nexx__SoEerror(Nexx__contextt *context, uint16 Slave, uint16 idn, uint16 Error)
{
   Nex_errort Ec;

   memset(&Ec, 0, sizeof(Ec));
   Ec.Time = osal_current_time();
   Ec.Slave = Slave;
   Ec.Index = idn;
   Ec.SubIdx = 0;
   *(context->ecaterror) = TRUE;
   Ec.Etype = Nex_ERR_TYPE_SOE_ERROR;
   Ec.ErrorCode = Error;
   Nexx__pusherror(context, &Ec);
}

/** SoE read, blocking.
 *
 * The IDN object of the selected slave and DriveNo is read. If a response
 * is larger than the mailbox size then the response is segmented. The function
 * will combine all segments and copy them to the parameter buffer.
 *
 * @param[in]  context        = context struct
 * @param[in]  slave         = Slave number
 * @param[in]  driveNo       = Drive number in slave
 * @param[in]  elementflags  = Flags to select what properties of IDN are to be transfered.
 * @param[in]  idn           = IDN.
 * @param[in,out] psize      = Size in bytes of parameter buffer, returns bytes read from SoE.
 * @param[out] p             = Pointer to parameter buffer
 * @param[in]  timeout       = Timeout in us, standard is Nex_TIMEOUTRXM
 * @return Workcounter from last slave response
 */
int Nexx__SoEread(Nexx__contextt *context, uint16 slave, uint8 driveNo, uint8 elementflags, uint16 idn, int *psize, void *p, int timeout)
{
   Nex_SoEt *SoEp, *aSoEp;
   uint16 totalsize, framedatasize;
   int wkc;
   uint8 *bp;
   uint8 *mp;
   uint16 *errorcode;
   Nex_mbxbuft MbxIn, MbxOut;
   uint8 cnt;
   boolean NotLast;

   Nex_clearmbx(&MbxIn);
   /* Empty slave out mailbox if something is in. Timeout set to 0 */
   wkc = Nexx__mbxreceive(context, slave, (Nex_mbxbuft *)&MbxIn, 0);
   Nex_clearmbx(&MbxOut);
   aSoEp = (Nex_SoEt *)&MbxIn;
   SoEp = (Nex_SoEt *)&MbxOut;
   SoEp->MbxHeader.length = htoes(sizeof(Nex_SoEt) - sizeof(Nex_mbxheadert));
   SoEp->MbxHeader.address = htoes(0x0000);
   SoEp->MbxHeader.priority = 0x00;
   /* get new mailbox count value, used as session handle */
   cnt = Nex_nextmbxcnt(context->slavelist[slave].mbx_cnt);
   context->slavelist[slave].mbx_cnt = cnt;
   SoEp->MbxHeader.mbxtype = ECT_MBXT_SOE + (cnt << 4); /* SoE */
   SoEp->opCode = ECT_SOE_READREQ;
   SoEp->incomplete = 0;
   SoEp->error = 0;
   SoEp->driveNo = driveNo;
   SoEp->elementflags = elementflags;
   SoEp->idn = htoes(idn);
   totalsize = 0;
   bp = p;
   mp = (uint8 *)&MbxIn + sizeof(Nex_SoEt);
   NotLast = TRUE;
   /* send SoE request to slave */
   wkc = Nexx__mbxsend(context, slave, (Nex_mbxbuft *)&MbxOut, Nex_TIMEOUTTXM);
   if (wkc > 0) /* succeeded to place mailbox in slave ? */
   {
      while (NotLast)
      {
         /* clean mailboxbuffer */
         Nex_clearmbx(&MbxIn);
         /* read slave response */
         wkc = Nexx__mbxreceive(context, slave, (Nex_mbxbuft *)&MbxIn, timeout);
         if (wkc > 0) /* succeeded to read slave response ? */
         {
            /* slave response should be SoE, ReadRes */
            if (((aSoEp->MbxHeader.mbxtype & 0x0f) == ECT_MBXT_SOE) &&
                (aSoEp->opCode == ECT_SOE_READRES) &&
                (aSoEp->error == 0) &&
                (aSoEp->driveNo == driveNo) &&
                (aSoEp->elementflags == elementflags))
            {
               framedatasize = etohs(aSoEp->MbxHeader.length) - sizeof(Nex_SoEt)  + sizeof(Nex_mbxheadert);
               totalsize += framedatasize;
               /* Does parameter fit in parameter buffer ? */
               if (totalsize <= *psize)
               {
                  /* copy parameter data in parameter buffer */
                  memcpy(bp, mp, framedatasize);
                  /* increment buffer pointer */
                  bp += framedatasize;
               }
               else
               {
                  framedatasize -= totalsize - *psize;
                  totalsize = *psize;
                  /* copy parameter data in parameter buffer */
                  if (framedatasize > 0) memcpy(bp, mp, framedatasize);
               }

               if (!aSoEp->incomplete)
               {
                  NotLast = FALSE;
                  *psize = totalsize;
               }
            }
            /* other slave response */
            else
            {
               NotLast = FALSE;
               if (((aSoEp->MbxHeader.mbxtype & 0x0f) == ECT_MBXT_SOE) &&
                   (aSoEp->opCode == ECT_SOE_READRES) &&
                   (aSoEp->error == 1))
               {
                  mp = (uint8 *)&MbxIn + (etohs(aSoEp->MbxHeader.length) + sizeof(Nex_mbxheadert) - sizeof(uint16));
                  errorcode = (uint16 *)mp;
                  Nexx__SoEerror(context, slave, idn, *errorcode);
               }
               else
               {
                  Nexx__packeterror(context, slave, idn, 0, 1); /* Unexpected frame returned */
               }
               wkc = 0;
            }
         }
         else
         {
            NotLast = FALSE;
            Nexx__packeterror(context, slave, idn, 0, 4); /* no response */
         }
      }
   }
   return wkc;
}

/** SoE write, blocking.
 *
 * The IDN object of the selected slave and DriveNo is written. If a response
 * is larger than the mailbox size then the response is segmented.
 *
 * @param[in]  context        = context struct
 * @param[in]  slave         = Slave number
 * @param[in]  driveNo       = Drive number in slave
 * @param[in]  elementflags  = Flags to select what properties of IDN are to be transfered.
 * @param[in]  idn           = IDN.
 * @param[in]  psize         = Size in bytes of parameter buffer.
 * @param[out] p             = Pointer to parameter buffer
 * @param[in]  timeout       = Timeout in us, standard is Nex_TIMEOUTRXM
 * @return Workcounter from last slave response
 */
int Nexx__SoEwrite(Nexx__contextt *context, uint16 slave, uint8 driveNo, uint8 elementflags, uint16 idn, int psize, void *p, int timeout)
{
   Nex_SoEt *SoEp, *aSoEp;
   uint16 framedatasize, maxdata;
   int wkc;
   uint8 *mp;
   uint8 *hp;
   uint16 *errorcode;
   Nex_mbxbuft MbxIn, MbxOut;
   uint8 cnt;
   boolean NotLast;

   Nex_clearmbx(&MbxIn);
   /* Empty slave out mailbox if something is in. Timeout set to 0 */
   wkc = Nexx__mbxreceive(context, slave, (Nex_mbxbuft *)&MbxIn, 0);
   Nex_clearmbx(&MbxOut);
   aSoEp = (Nex_SoEt *)&MbxIn;
   SoEp = (Nex_SoEt *)&MbxOut;
   SoEp->MbxHeader.address = htoes(0x0000);
   SoEp->MbxHeader.priority = 0x00;
   SoEp->opCode = ECT_SOE_WRITEREQ;
   SoEp->error = 0;
   SoEp->driveNo = driveNo;
   SoEp->elementflags = elementflags;
   hp = p;
   mp = (uint8 *)&MbxOut + sizeof(Nex_SoEt);
   maxdata = context->slavelist[slave].mbx_l - sizeof(Nex_SoEt);
   NotLast = TRUE;
   while (NotLast)
   {
      framedatasize = psize;
      NotLast = FALSE;
      SoEp->idn = htoes(idn);
      SoEp->incomplete = 0;
      if (framedatasize > maxdata)
      {
         framedatasize = maxdata;  /*  segmented transfer needed  */
         NotLast = TRUE;
         SoEp->incomplete = 1;
         SoEp->fragmentsleft = psize / maxdata;
      }
      SoEp->MbxHeader.length = htoes(sizeof(Nex_SoEt) - sizeof(Nex_mbxheadert) + framedatasize);
      /* get new mailbox counter, used for session handle */
      cnt = Nex_nextmbxcnt(context->slavelist[slave].mbx_cnt);
      context->slavelist[slave].mbx_cnt = cnt;
      SoEp->MbxHeader.mbxtype = ECT_MBXT_SOE + (cnt << 4); /* SoE */
      /* copy parameter data to mailbox */
      memcpy(mp, hp, framedatasize);
      hp += framedatasize;
      psize -= framedatasize;
      /* send SoE request to slave */
      wkc = Nexx__mbxsend(context, slave, (Nex_mbxbuft *)&MbxOut, Nex_TIMEOUTTXM);
      if (wkc > 0) /* succeeded to place mailbox in slave ? */
      {
         if (!NotLast || !Nexx__mbxempty(context, slave, timeout))
         {
            /* clean mailboxbuffer */
            Nex_clearmbx(&MbxIn);
            /* read slave response */
            wkc = Nexx__mbxreceive(context, slave, (Nex_mbxbuft *)&MbxIn, timeout);
            if (wkc > 0) /* succeeded to read slave response ? */
            {
               NotLast = FALSE;
               /* slave response should be SoE, WriteRes */
               if (((aSoEp->MbxHeader.mbxtype & 0x0f) == ECT_MBXT_SOE) &&
                   (aSoEp->opCode == ECT_SOE_WRITERES) &&
                   (aSoEp->error == 0) &&
                   (aSoEp->driveNo == driveNo) &&
                   (aSoEp->elementflags == elementflags))
               {
                  /* SoE write succeeded */
               }
               /* other slave response */
               else
               {
                  if (((aSoEp->MbxHeader.mbxtype & 0x0f) == ECT_MBXT_SOE) &&
                      (aSoEp->opCode == ECT_SOE_READRES) &&
                      (aSoEp->error == 1))
                  {
                     mp = (uint8 *)&MbxIn + (etohs(aSoEp->MbxHeader.length) + sizeof(Nex_mbxheadert) - sizeof(uint16));
                     errorcode = (uint16 *)mp;
                     Nexx__SoEerror(context, slave, idn, *errorcode);
                  }
                  else
                  {
                     Nexx__packeterror(context, slave, idn, 0, 1); /* Unexpected frame returned */
                  }
                  wkc = 0;
               }
            }
            else
            {
               Nexx__packeterror(context, slave, idn, 0, 4); /* no response */
            }
         }
      }
   }
   return wkc;
}

/** SoE read AT and MTD mapping.
 *
 * SoE has standard indexes defined for mapping. This function
 * tries to read them and collect a full input and output mapping size
 * of designated slave.
 *
 * @param[in]  context = context struct
 * @param[in]  slave   = Slave number
 * @param[out] Osize   = Size in bits of output mapping (MTD) found
 * @param[out] Isize   = Size in bits of input mapping (AT) found
 * @return >0 if mapping succesful.
 */
int Nexx__readIDNmap(Nexx__contextt *context, uint16 slave, int *Osize, int *Isize)
{
   int retVal = 0;
   int   wkc;
   int psize;
   int driveNr;
   uint16 entries, itemcount;
   Nex_SoEmappingt     SoEmapping;
   Nex_SoEattributet   SoEattribute;

   *Isize = 0;
   *Osize = 0;
   for(driveNr = 0; driveNr < Nex_SOE_MAX_DRIVES; driveNr++)
   {
      psize = sizeof(SoEmapping);
      /* read output mapping via SoE */
      wkc = Nexx__SoEread(context, slave, driveNr, Nex_SOE_VALUE_B, Nex_IDN_MDTCONFIG, &psize, &SoEmapping, Nex_TIMEOUTRXM);
      if ((wkc > 0) && (psize >= 4) && ((entries = etohs(SoEmapping.currentlength) / 2) > 0) && (entries <= Nex_SOE_MAXMAPPING))
      {
         /* command word (uint16) is always mapped but not in list */
         *Osize = 16;
         for (itemcount = 0 ; itemcount < entries ; itemcount++)
         {
            psize = sizeof(SoEattribute);
            /* read attribute of each IDN in mapping list */
            wkc = Nexx__SoEread(context, slave, driveNr, Nex_SOE_ATTRIBUTE_B, SoEmapping.idn[itemcount], &psize, &SoEattribute, Nex_TIMEOUTRXM);
            if ((wkc > 0) && (!SoEattribute.list))
            {
               /* length : 0 = 8bit, 1 = 16bit .... */
               *Osize += (int)8 << SoEattribute.length;
            }
         }
      }
      psize = sizeof(SoEmapping);
      /* read input mapping via SoE */
      wkc = Nexx__SoEread(context, slave, driveNr, Nex_SOE_VALUE_B, Nex_IDN_ATCONFIG, &psize, &SoEmapping, Nex_TIMEOUTRXM);
      if ((wkc > 0) && (psize >= 4) && ((entries = etohs(SoEmapping.currentlength) / 2) > 0) && (entries <= Nex_SOE_MAXMAPPING))
      {
         /* status word (uint16) is always mapped but not in list */
         *Isize = 16;
         for (itemcount = 0 ; itemcount < entries ; itemcount++)
         {
            psize = sizeof(SoEattribute);
            /* read attribute of each IDN in mapping list */
            wkc = Nexx__SoEread(context, slave, driveNr, Nex_SOE_ATTRIBUTE_B, SoEmapping.idn[itemcount], &psize, &SoEattribute, Nex_TIMEOUTRXM);
            if ((wkc > 0) && (!SoEattribute.list))
            {
               /* length : 0 = 8bit, 1 = 16bit .... */
               *Isize += (int)8 << SoEattribute.length;
            }
         }
      }
   }

   /* found some I/O bits ? */
   if ((*Isize > 0) || (*Osize > 0))
   {
      retVal = 1;
   }
   return retVal;
}


int Nex_SoEread(uint16 slave, uint8 driveNo, uint8 elementflags, uint16 idn, int *psize, void *p, int timeout)
{
   return Nexx__SoEread(&Nexx__context, slave, driveNo, elementflags, idn, psize, p, timeout);
}

int Nex_SoEwrite(uint16 slave, uint8 driveNo, uint8 elementflags, uint16 idn, int psize, void *p, int timeout)
{
   return Nexx__SoEwrite(&Nexx__context, slave, driveNo, elementflags, idn, psize, p, timeout);
}

int Nex_readIDNmap(uint16 slave, int *Osize, int *Isize)
{
   return Nexx__readIDNmap(&Nexx__context, slave, Osize, Isize);
}

