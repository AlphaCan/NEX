/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

/**
 * \file
 * \brief
 * Main EtherCAT functions.
 *
 * Initialisation, state set and read, mailbox primitives, EEPROM primitives,
 * SII reading and processdata exchange.
 *
 * Defines Nex_slave[]. All slave information is put in this structure.
 * Needed for most user interaction with slaves.
 */

#include <stdio.h>
#include <string.h>
#include "osal.h"
#include "oshw.h"
#include "ethercattype.h"
#include "ethercatbase.h"
#include "ethercatmain.h"


/** delay in us for eeprom ready loop */
#define Nex_LOCALDELAY  200

/** record for ethercat eeprom communications */
PACKED_BEGIN
typedef struct PACKED
{
   uint16    comm;
   uint16    addr;
   uint16    d2;
} Nex_eepromt;
PACKED_END

/** mailbox error structure */
PACKED_BEGIN
typedef struct PACKED
{
   Nex_mbxheadert   MbxHeader;
   uint16          Type;
   uint16          Detail;
} Nex_mbxerrort;
PACKED_END

/** emergency request structure */
PACKED_BEGIN
typedef struct PACKED
{
   Nex_mbxheadert   MbxHeader;
   uint16          CANOpen;
   uint16          ErrorCode;
   uint8           ErrorReg;
   uint8           bData;
   uint16          w1,w2;
} Nex_emcyt;
PACKED_END


/** Main slave data array.
 *  Each slave found on the network gets its own record.
 *  Nex_slave[0] is reserved for the master. Structure gets filled
 *  in by the configuration function Nex_config().
 */
Nex_slavet               Nex_slave[Nex_MAXSLAVE];
/** number of slaves found on the network */
int                     Nex_slavecount;
/** slave group structure */
Nex_groupt               Nex_group[Nex_MAXGROUP];

/** cache for EEPROM read functions */
static uint8            Nex_esibuf[Nex_MAXEEPBUF];
/** bitmap for filled cache buffer bytes */
static uint32           Nex_esimap[Nex_MAXEEPBITMAP];
/** current slave for EEPROM cache buffer */
static Nex_eringt        Nex_elist;
static Nex_idxstackT     Nex_idxstack;

/** SyncManager Communication Type struct to store data of one slave */
static Nex_SMcommtypet   Nex_SMcommtype[Nex_MAX_MAPT];
/** PDO assign struct to store data of one slave */
static Nex_PDOassignt    Nex_PDOassign[Nex_MAX_MAPT];
/** PDO description struct to store data of one slave */
static Nex_PDOdesct      Nex_PDOdesc[Nex_MAX_MAPT];

/** buffer for EEPROM SM data */
static Nex_eepromSMt     Nex_SM;
/** buffer for EEPROM FMMU data */
static Nex_eepromFMMUt   Nex_FMMU;
/** Global variable TRUE if error available in error stack */
boolean                 EcatError = FALSE;

int64                   Nex_DCtime;

Nexx__portt               Nexx__port;
Nexx__redportt            Nexx__redport;

Nexx__contextt  Nexx__context = {
    &Nexx__port,          // .port          =
    &Nex_slave[0],       // .slavelist     =
    &Nex_slavecount,     // .slavecount    =
    Nex_MAXSLAVE,        // .maxslave      =
    &Nex_group[0],       // .grouplist     =
    Nex_MAXGROUP,        // .maxgroup      =
    &Nex_esibuf[0],      // .esibuf        =
    &Nex_esimap[0],      // .esimap        =
    0,                  // .esislave      =
    &Nex_elist,          // .elist         =
    &Nex_idxstack,       // .idxstack      =
    &EcatError,         // .ecaterror     =
    0,                  // .DCtO          =
    0,                  // .DCl           =
    &Nex_DCtime,         // .DCtime        =
    &Nex_SMcommtype[0],  // .SMcommtype    =
    &Nex_PDOassign[0],   // .PDOassign     =
    &Nex_PDOdesc[0],     // .PDOdesc       =
    &Nex_SM,             // .eepSM         =
    &Nex_FMMU,           // .eepFMMU       =
    NULL                // .FOEhook()
};


/** Create list over available network adapters.
 *
 * @return First element in list over available network adapters.
 */
Nex_adaptert * Nex_find_adapters (void)
{
   Nex_adaptert * ret_adapter;

   ret_adapter = oshw_find_adapters ();

   return ret_adapter;
}

/** Free dynamically allocated list over available network adapters.
 *
 * @param[in] adapter = Struct holding adapter name, description and pointer to next.
 */
void Nex_free_adapters (Nex_adaptert * adapter)
{
   oshw_free_adapters (adapter);
}

/** Pushes an error on the error list.
 *
 * @param[in] context        = context struct
 * @param[in] Ec pointer describing the error.
 */
void Nexx__pusherror(Nexx__contextt *context, const Nex_errort *Ec)
{
   context->elist->Error[context->elist->head] = *Ec;
   context->elist->Error[context->elist->head].Signal = TRUE;
   context->elist->head++;
   if (context->elist->head > Nex_MAXELIST)
   {
      context->elist->head = 0;
   }
   if (context->elist->head == context->elist->tail)
   {
      context->elist->tail++;
   }
   if (context->elist->tail > Nex_MAXELIST)
   {
      context->elist->tail = 0;
   }
   *(context->ecaterror) = TRUE;
}

/** Pops an error from the list.
 *
 * @param[in] context        = context struct
 * @param[out] Ec = Struct describing the error.
 * @return TRUE if an error was popped.
 */
boolean Nexx__poperror(Nexx__contextt *context, Nex_errort *Ec)
{
   boolean notEmpty = (context->elist->head != context->elist->tail);

   *Ec = context->elist->Error[context->elist->tail];
   context->elist->Error[context->elist->tail].Signal = FALSE;
   if (notEmpty)
   {
      context->elist->tail++;
      if (context->elist->tail > Nex_MAXELIST)
      {
         context->elist->tail = 0;
      }
   }
   else
   {
      *(context->ecaterror) = FALSE;
   }
   return notEmpty;
}

/** Check if error list has entries.
 *
 * @param[in] context        = context struct
 * @return TRUE if error list contains entries.
 */
boolean Nexx__iserror(Nexx__contextt *context)
{
   return (context->elist->head != context->elist->tail);
}

/** Report packet error
 *
 * @param[in]  context        = context struct
 * @param[in]  Slave      = Slave number
 * @param[in]  Index      = Index that generated error
 * @param[in]  SubIdx     = Subindex that generated error
 * @param[in]  ErrorCode  = Error code
 */
void Nexx__packeterror(Nexx__contextt *context, uint16 Slave, uint16 Index, uint8 SubIdx, uint16 ErrorCode)
{
   Nex_errort Ec;

   memset(&Ec, 0, sizeof(Ec));
   Ec.Time = osal_current_time();
   Ec.Slave = Slave;
   Ec.Index = Index;
   Ec.SubIdx = SubIdx;
   *(context->ecaterror) = TRUE;
   Ec.Etype = Nex_ERR_TYPE_PACKET_ERROR;
   Ec.ErrorCode = ErrorCode;
   Nexx__pusherror(context, &Ec);
}

/** Report Mailbox Error
 *
 * @param[in]  context        = context struct
 * @param[in]  Slave        = Slave number
 * @param[in]  Detail       = Following EtherCAT specification
 */
static void Nexx__mbxerror(Nexx__contextt *context, uint16 Slave,uint16 Detail)
{
   Nex_errort Ec;

   memset(&Ec, 0, sizeof(Ec));
   Ec.Time = osal_current_time();
   Ec.Slave = Slave;
   Ec.Index = 0;
   Ec.SubIdx = 0;
   Ec.Etype = Nex_ERR_TYPE_MBX_ERROR;
   Ec.ErrorCode = Detail;
   Nexx__pusherror(context, &Ec);
}

/** Report Mailbox Emergency Error
 *
 * @param[in]  context        = context struct
 * @param[in]  Slave      = Slave number
 * @param[in]  ErrorCode  = Following EtherCAT specification
 * @param[in]  ErrorReg
 * @param[in]  b1
 * @param[in]  w1
 * @param[in]  w2
 */
static void Nexx__mbxemergencyerror(Nexx__contextt *context, uint16 Slave,uint16 ErrorCode,uint16 ErrorReg,
    uint8 b1, uint16 w1, uint16 w2)
{
   Nex_errort Ec;

   memset(&Ec, 0, sizeof(Ec));
   Ec.Time = osal_current_time();
   Ec.Slave = Slave;
   Ec.Index = 0;
   Ec.SubIdx = 0;
   Ec.Etype = Nex_ERR_TYPE_EMERGENCY;
   Ec.ErrorCode = ErrorCode;
   Ec.ErrorReg = (uint8)ErrorReg;
   Ec.b1 = b1;
   Ec.w1 = w1;
   Ec.w2 = w2;
   Nexx__pusherror(context, &Ec);
}

/** Initialise lib in single NIC mode
 * @param[in]  context = context struct
 * @param[in] ifname   = Dev name, f.e. "eth0"
 * @return >0 if OK
 */
int Nexx__init(Nexx__contextt *context, const char * ifname)
{
   return Nexx__setupnic(context->port, ifname, FALSE);
}

/** Initialise lib in redundant NIC mode
 * @param[in]  context  = context struct
 * @param[in]  redport  = pointer to redport, redundant port data
 * @param[in]  ifname   = Primary Dev name, f.e. "eth0"
 * @param[in]  if2name  = Secondary Dev name, f.e. "eth1"
 * @return >0 if OK
 */
int Nexx__init_redundant(Nexx__contextt *context, Nexx__redportt *redport, const char *ifname, char *if2name)
{
   int rval, zbuf;
   Nex_etherheadert *ehp;

   context->port->redport = redport;
   Nexx__setupnic(context->port, ifname, FALSE);
   rval = Nexx__setupnic(context->port, if2name, TRUE);
   /* prepare "dummy" BRD tx frame for redundant operation */
   ehp = (Nex_etherheadert *)&(context->port->txbuf2);
   ehp->sa1 = oshw_htons(secMAC[0]);
   zbuf = 0;
   Nexx__setupdatagram(context->port, &(context->port->txbuf2), Nex_CMD_BRD, 0, 0x0000, 0x0000, 2, &zbuf);
   context->port->txbuflength2 = ETH_HEADERSIZE + Nex_HEADERSIZE + Nex_WKCSIZE + 2;

   return rval;
}

/** Close lib.
 * @param[in]  context        = context struct
 */
void Nexx__close(Nexx__contextt *context)
{
   Nexx__closenic(context->port);
};

/** Read one byte from slave EEPROM via cache.
 *  If the cache location is empty then a read request is made to the slave.
 *  Depending on the slave capabillities the request is 4 or 8 bytes.
 *  @param[in] context = context struct
 *  @param[in] slave   = slave number
 *  @param[in] address = eeprom address in bytes (slave uses words)
 *  @return requested byte, if not available then 0xff
 */
uint8 Nexx__siigetbyte(Nexx__contextt *context, uint16 slave, uint16 address)
{
   uint16 configadr, eadr;
   uint64 edat;
   uint16 mapw, mapb;
   int lp,cnt;
   uint8 retval;

   retval = 0xff;
   if (slave != context->esislave) /* not the same slave? */
   {
      memset(context->esimap, 0x00, Nex_MAXEEPBITMAP * sizeof(uint32)); /* clear esibuf cache map */
      context->esislave = slave;
   }
   if (address < Nex_MAXEEPBUF)
   {
      mapw = address >> 5;
      mapb = address - (mapw << 5);
      if (context->esimap[mapw] & (uint32)(1 << mapb))
      {
         /* byte is already in buffer */
         retval = context->esibuf[address];
      }
      else
      {
         /* byte is not in buffer, put it there */
         configadr = context->slavelist[slave].configadr;
         Nexx__eeprom2master(context, slave); /* set eeprom control to master */
         eadr = address >> 1;
         edat = Nexx__readeepromFP (context, configadr, eadr, Nex_TIMEOUTEEP);
         /* 8 byte response */
         if (context->slavelist[slave].eep_8byte)
         {
            put_unaligned64(edat, &(context->esibuf[eadr << 1]));
            cnt = 8;
         }
         /* 4 byte response */
         else
         {
            put_unaligned32(edat, &(context->esibuf[eadr << 1]));
            cnt = 4;
         }
         /* find bitmap location */
         mapw = eadr >> 4;
         mapb = (eadr << 1) - (mapw << 5);
         for(lp = 0 ; lp < cnt ; lp++)
         {
            /* set bitmap for each byte that is read */
            context->esimap[mapw] |= (1 << mapb);
            mapb++;
            if (mapb > 31)
            {
               mapb = 0;
               mapw++;
            }
         }
         retval = context->esibuf[address];
      }
   }

   return retval;
}

/** Find SII section header in slave EEPROM.
 *  @param[in]  context        = context struct
 *  @param[in] slave   = slave number
 *  @param[in] cat     = section category
 *  @return byte address of section at section length entry, if not available then 0
 */
int16 Nexx__siifind(Nexx__contextt *context, uint16 slave, uint16 cat)
{
   int16 a;
   uint16 p;
   uint8 eectl = context->slavelist[slave].eep_pdi;

   a = ECT_SII_START << 1;
   /* read first SII section category */
   p = Nexx__siigetbyte(context, slave, a++);
   p += (Nexx__siigetbyte(context, slave, a++) << 8);
   /* traverse SII while category is not found and not EOF */
   while ((p != cat) && (p != 0xffff))
   {
      /* read section length */
      p = Nexx__siigetbyte(context, slave, a++);
      p += (Nexx__siigetbyte(context, slave, a++) << 8);
      /* locate next section category */
      a += p << 1;
      /* read section category */
      p = Nexx__siigetbyte(context, slave, a++);
      p += (Nexx__siigetbyte(context, slave, a++) << 8);
   }
   if (p != cat)
   {
      a = 0;
   }
   if (eectl)
   {
      Nexx__eeprom2pdi(context, slave); /* if eeprom control was previously pdi then restore */
   }

   return a;
}

/** Get string from SII string section in slave EEPROM.
 *  @param[in]  context = context struct
 *  @param[out] str     = requested string, 0x00 if not found
 *  @param[in]  slave   = slave number
 *  @param[in]  Sn      = string number
 */
void Nexx__siistring(Nexx__contextt *context, char *str, uint16 slave, uint16 Sn)
{
   uint16 a,i,j,l,n,ba;
   char *ptr;
   uint8 eectl = context->slavelist[slave].eep_pdi;

   ptr = str;
   a = Nexx__siifind (context, slave, ECT_SII_STRING); /* find string section */
   if (a > 0)
   {
      ba = a + 2; /* skip SII section header */
      n = Nexx__siigetbyte(context, slave, ba++); /* read number of strings in section */
      if (Sn <= n) /* is req string available? */
      {
         for (i = 1; i <= Sn; i++) /* walk through strings */
         {
            l = Nexx__siigetbyte(context, slave, ba++); /* length of this string */
            if (i < Sn)
            {
               ba += l;
            }
            else
            {
               ptr = str;
               for (j = 1; j <= l; j++) /* copy one string */
               {
                  if(j <= Nex_MAXNAME)
                  {
                     *ptr = (char)Nexx__siigetbyte(context, slave, ba++);
                     ptr++;
                  }
                  else
                  {
                     ba++;
                  }
               }
            }
         }
         *ptr = 0; /* add zero terminator */
      }
      else
      {
         ptr = str;
         *ptr = 0; /* empty string */
      }
   }
   if (eectl)
   {
      Nexx__eeprom2pdi(context, slave); /* if eeprom control was previously pdi then restore */
   }
}

/** Get FMMU data from SII FMMU section in slave EEPROM.
 *  @param[in]  context = context struct
 *  @param[in]  slave   = slave number
 *  @param[out] FMMU    = FMMU struct from SII, max. 4 FMMU's
 *  @return number of FMMU's defined in section
 */
uint16 Nexx__siiFMMU(Nexx__contextt *context, uint16 slave, Nex_eepromFMMUt* FMMU)
{
   uint16  a;
   uint8 eectl = context->slavelist[slave].eep_pdi;

   FMMU->nFMMU = 0;
   FMMU->FMMU0 = 0;
   FMMU->FMMU1 = 0;
   FMMU->FMMU2 = 0;
   FMMU->FMMU3 = 0;
   FMMU->Startpos = Nexx__siifind(context, slave, ECT_SII_FMMU);

   if (FMMU->Startpos > 0)
   {
      a = FMMU->Startpos;
      FMMU->nFMMU = Nexx__siigetbyte(context, slave, a++);
      FMMU->nFMMU += (Nexx__siigetbyte(context, slave, a++) << 8);
      FMMU->nFMMU *= 2;
      FMMU->FMMU0 = Nexx__siigetbyte(context, slave, a++);
      FMMU->FMMU1 = Nexx__siigetbyte(context, slave, a++);
      if (FMMU->nFMMU > 2)
      {
         FMMU->FMMU2 = Nexx__siigetbyte(context, slave, a++);
         FMMU->FMMU3 = Nexx__siigetbyte(context, slave, a++);
      }
   }
   if (eectl)
   {
      Nexx__eeprom2pdi(context, slave); /* if eeprom control was previously pdi then restore */
   }

   return FMMU->nFMMU;
}

/** Get SM data from SII SM section in slave EEPROM.
 *  @param[in]  context = context struct
 *  @param[in]  slave   = slave number
 *  @param[out] SM      = first SM struct from SII
 *  @return number of SM's defined in section
 */
uint16 Nexx__siiSM(Nexx__contextt *context, uint16 slave, Nex_eepromSMt* SM)
{
   uint16 a,w;
   uint8 eectl = context->slavelist[slave].eep_pdi;

   SM->nSM = 0;
   SM->Startpos = Nexx__siifind(context, slave, ECT_SII_SM);
   if (SM->Startpos > 0)
   {
      a = SM->Startpos;
      w = Nexx__siigetbyte(context, slave, a++);
      w += (Nexx__siigetbyte(context, slave, a++) << 8);
      SM->nSM = (w / 4);
      SM->PhStart = Nexx__siigetbyte(context, slave, a++);
      SM->PhStart += (Nexx__siigetbyte(context, slave, a++) << 8);
      SM->Plength = Nexx__siigetbyte(context, slave, a++);
      SM->Plength += (Nexx__siigetbyte(context, slave, a++) << 8);
      SM->Creg = Nexx__siigetbyte(context, slave, a++);
      SM->Sreg = Nexx__siigetbyte(context, slave, a++);
      SM->Activate = Nexx__siigetbyte(context, slave, a++);
      SM->PDIctrl = Nexx__siigetbyte(context, slave, a++);
   }
   if (eectl)
   {
      Nexx__eeprom2pdi(context, slave); /* if eeprom control was previously pdi then restore */
   }

   return SM->nSM;
}

/** Get next SM data from SII SM section in slave EEPROM.
 *  @param[in]  context = context struct
 *  @param[in]  slave   = slave number
 *  @param[out] SM      = first SM struct from SII
 *  @param[in]  n       = SM number
 *  @return >0 if OK
 */
uint16 Nexx__siiSMnext(Nexx__contextt *context, uint16 slave, Nex_eepromSMt* SM, uint16 n)
{
   uint16 a;
   uint16 retVal = 0;
   uint8 eectl = context->slavelist[slave].eep_pdi;

   if (n < SM->nSM)
   {
      a = SM->Startpos + 2 + (n * 8);
      SM->PhStart = Nexx__siigetbyte(context, slave, a++);
      SM->PhStart += (Nexx__siigetbyte(context, slave, a++) << 8);
      SM->Plength = Nexx__siigetbyte(context, slave, a++);
      SM->Plength += (Nexx__siigetbyte(context, slave, a++) << 8);
      SM->Creg = Nexx__siigetbyte(context, slave, a++);
      SM->Sreg = Nexx__siigetbyte(context, slave, a++);
      SM->Activate = Nexx__siigetbyte(context, slave, a++);
      SM->PDIctrl = Nexx__siigetbyte(context, slave, a++);
      retVal = 1;
   }
   if (eectl)
   {
      Nexx__eeprom2pdi(context, slave); /* if eeprom control was previously pdi then restore */
   }

   return retVal;
}

/** Get PDO data from SII PDO section in slave EEPROM.
 *  @param[in]  context = context struct
 *  @param[in]  slave   = slave number
 *  @param[out] PDO     = PDO struct from SII
 *  @param[in]  t       = 0=RXPDO 1=TXPDO
 *  @return mapping size in bits of PDO
 */
int Nexx__siiPDO(Nexx__contextt *context, uint16 slave, Nex_eepromPDOt* PDO, uint8 t)
{
   uint16 a , w, c, e, er, Size;
   uint8 eectl = context->slavelist[slave].eep_pdi;

   Size = 0;
   PDO->nPDO = 0;
   PDO->Length = 0;
   PDO->Index[1] = 0;
   for (c = 0 ; c < Nex_MAXSM ; c++) PDO->SMbitsize[c] = 0;
   if (t > 1)
      t = 1;
   PDO->Startpos = Nexx__siifind(context, slave, ECT_SII_PDO + t);
   if (PDO->Startpos > 0)
   {
      a = PDO->Startpos;
      w = Nexx__siigetbyte(context, slave, a++);
      w += (Nexx__siigetbyte(context, slave, a++) << 8);
      PDO->Length = w;
      c = 1;
      /* traverse through all PDOs */
      do
      {
         PDO->nPDO++;
         PDO->Index[PDO->nPDO] = Nexx__siigetbyte(context, slave, a++);
         PDO->Index[PDO->nPDO] += (Nexx__siigetbyte(context, slave, a++) << 8);
         PDO->BitSize[PDO->nPDO] = 0;
         c++;
         e = Nexx__siigetbyte(context, slave, a++);
         PDO->SyncM[PDO->nPDO] = Nexx__siigetbyte(context, slave, a++);
         a += 4;
         c += 2;
         if (PDO->SyncM[PDO->nPDO] < Nex_MAXSM) /* active and in range SM? */
         {
            /* read all entries defined in PDO */
            for (er = 1; er <= e; er++)
            {
               c += 4;
               a += 5;
               PDO->BitSize[PDO->nPDO] += Nexx__siigetbyte(context, slave, a++);
               a += 2;
            }
            PDO->SMbitsize[ PDO->SyncM[PDO->nPDO] ] += PDO->BitSize[PDO->nPDO];
            Size += PDO->BitSize[PDO->nPDO];
            c++;
         }
         else /* PDO deactivated because SM is 0xff or > Nex_MAXSM */
         {
            c += 4 * e;
            a += 8 * e;
            c++;
         }
         if (PDO->nPDO >= (Nex_MAXEEPDO - 1))
         {
            c = PDO->Length; /* limit number of PDO entries in buffer */
         }
      }
      while (c < PDO->Length);
   }
   if (eectl)
   {
      Nexx__eeprom2pdi(context, slave); /* if eeprom control was previously pdi then restore */
   }

   return (Size);
}

#define MAX_FPRD_MULTI 64

int Nexx__FPRD_multi(Nexx__contextt *context, int n, uint16 *configlst, Nex_alstatust *slstatlst, int timeout)
{
   int wkc;
   uint8 idx;
   Nexx__portt *port;
   int sldatapos[MAX_FPRD_MULTI];
   int slcnt;

   port = context->port;
   idx = Nexx__getindex(port);
   slcnt = 0;
   Nexx__setupdatagram(port, &(port->txbuf[idx]), Nex_CMD_FPRD, idx,
      *(configlst + slcnt), ECT_REG_ALSTAT, sizeof(Nex_alstatust), slstatlst + slcnt);
   sldatapos[slcnt] = Nex_HEADERSIZE;
   while(++slcnt < (n - 1))
   {
      sldatapos[slcnt] = Nexx__adddatagram(port, &(port->txbuf[idx]), Nex_CMD_FPRD, idx, TRUE,
                            *(configlst + slcnt), ECT_REG_ALSTAT, sizeof(Nex_alstatust), slstatlst + slcnt);
   }
   if(slcnt < n)
   {
      sldatapos[slcnt] = Nexx__adddatagram(port, &(port->txbuf[idx]), Nex_CMD_FPRD, idx, FALSE,
                            *(configlst + slcnt), ECT_REG_ALSTAT, sizeof(Nex_alstatust), slstatlst + slcnt);
   }
   wkc = Nexx__srconfirm(port, idx, timeout);
   if (wkc >= 0)
   {
      for(slcnt = 0 ; slcnt < n ; slcnt++)
      {
         memcpy(slstatlst + slcnt, &(port->rxbuf[idx][sldatapos[slcnt]]), sizeof(Nex_alstatust));
      }
   }
   Nexx__setbufstat(port, idx, Nex_BUF_EMPTY);
   return wkc;
}

/** Read all slave states in Nex_slave.
 * @param[in] context = context struct
 * @return lowest state found
 */
int Nexx__readstate(Nexx__contextt *context)
{
   uint16 slave, fslave, lslave, configadr, lowest, rval, bitwisestate;
   Nex_alstatust sl[MAX_FPRD_MULTI];
   uint16 slca[MAX_FPRD_MULTI];
   boolean noerrorflag, allslavessamestate;
   boolean allslavespresent = FALSE;
   int wkc;

   /* Try to establish the state of all slaves sending only one broadcast datargam.
    * This way a number of datagrams equal to the number of slaves will be sent only if needed.*/
   rval = 0;
   wkc = Nexx__BRD(context->port, 0, ECT_REG_ALSTAT, sizeof(rval), &rval, Nex_TIMEOUTRET);

   if(wkc >= *(context->slavecount))
   {
      allslavespresent = TRUE;
   }

   rval = etohs(rval);
   bitwisestate = (rval & 0x0f);

   if ((rval & Nex_STATE_ERROR) == 0)
   {
      noerrorflag = TRUE;
      context->slavelist[0].ALstatuscode = 0;
   }   
   else
   {
      noerrorflag = FALSE;
   }

   switch (bitwisestate)
   {
      case Nex_STATE_INIT:
      case Nex_STATE_PRE_OP:
      case Nex_STATE_BOOT:
      case Nex_STATE_SAFE_OP:
      case Nex_STATE_OPERATIONAL:
         allslavessamestate = TRUE;
         context->slavelist[0].state = bitwisestate;
         break;
      default:
         allslavessamestate = FALSE;
         break;
   }
    
   if (noerrorflag && allslavessamestate && allslavespresent)
   {
      /* No slave has toggled the error flag so the alstatuscode
       * (even if different from 0) should be ignored and
       * the slaves have reached the same state so the internal state
       * can be updated without sending any datagram. */
      for (slave = 1; slave <= *(context->slavecount); slave++)
      {
         context->slavelist[slave].ALstatuscode = 0x0000;
         context->slavelist[slave].state = bitwisestate;
      }
      lowest = bitwisestate;
   }
   else
   {
      /* Not all slaves have the same state or at least one is in error so one datagram per slave
       * is needed. */
      context->slavelist[0].ALstatuscode = 0;
      lowest = 0xff;
      fslave = 1;
      do
      {
         lslave = *(context->slavecount);
         if ((lslave - fslave) >= MAX_FPRD_MULTI)
         {
            lslave = fslave + MAX_FPRD_MULTI - 1;
         }
         for (slave = fslave; slave <= lslave; slave++)
         {
            const Nex_alstatust zero = { 0, 0, 0 };

            configadr = context->slavelist[slave].configadr;
            slca[slave - fslave] = configadr;
            sl[slave - fslave] = zero;
         }
         Nexx__FPRD_multi(context, (lslave - fslave) + 1, &(slca[0]), &(sl[0]), Nex_TIMEOUTRET3);
         for (slave = fslave; slave <= lslave; slave++)
         {
            configadr = context->slavelist[slave].configadr;
            rval = etohs(sl[slave - fslave].alstatus);
            context->slavelist[slave].ALstatuscode = etohs(sl[slave - fslave].alstatuscode);
            if ((rval & 0xf) < lowest)
            {
               lowest = (rval & 0xf);
            }
            context->slavelist[slave].state = rval;
            context->slavelist[0].ALstatuscode |= context->slavelist[slave].ALstatuscode;
         }
         fslave = lslave + 1;
      } while (lslave < *(context->slavecount));
      context->slavelist[0].state = lowest;
   }
  
   return lowest;
}

/** Write slave state, if slave = 0 then write to all slaves.
 * The function does not check if the actual state is changed.
 * @param[in]  context        = context struct
 * @param[in] slave    = Slave number, 0 = master
 * @return Workcounter or Nex_NOFRAME
 */
int Nexx__writestate(Nexx__contextt *context, uint16 slave)
{
   int ret;
   uint16 configadr, slstate;

   if (slave == 0)
   {
      slstate = htoes(context->slavelist[slave].state);
      ret = Nexx__BWR(context->port, 0, ECT_REG_ALCTL, sizeof(slstate),
	            &slstate, Nex_TIMEOUTRET3);
   }
   else
   {
      configadr = context->slavelist[slave].configadr;

      ret = Nexx__FPWRw(context->port, configadr, ECT_REG_ALCTL,
	        htoes(context->slavelist[slave].state), Nex_TIMEOUTRET3);
   }
   return ret;
}

/** Check actual slave state.
 * This is a blocking function.
 * To refresh the state of all slaves Nexx__readstate()should be called
 * @param[in] context     = context struct
 * @param[in] slave       = Slave number, 0 = all slaves (only the "slavelist[0].state" is refreshed)
 * @param[in] reqstate    = Requested state
 * @param[in] timeout     = Timout value in us
 * @return Requested state, or found state after timeout.
 */
uint16 Nexx__statecheck(Nexx__contextt *context, uint16 slave, uint16 reqstate, int timeout)
{
   uint16 configadr, state, rval;
   Nex_alstatust slstat;
   osal_timert timer;

   if ( slave > *(context->slavecount) )
   {
      return 0;
   }
   osal_timer_start(&timer, timeout);
   configadr = context->slavelist[slave].configadr;
   do
   {
      if (slave < 1)
      {
         rval = 0;
         Nexx__BRD(context->port, 0, ECT_REG_ALSTAT, sizeof(rval), &rval , Nex_TIMEOUTRET);
         rval = etohs(rval);
      }
      else
      {
         slstat.alstatus = 0;
         slstat.alstatuscode = 0;
         Nexx__FPRD(context->port, configadr, ECT_REG_ALSTAT, sizeof(slstat), &slstat, Nex_TIMEOUTRET);
         rval = etohs(slstat.alstatus);
         context->slavelist[slave].ALstatuscode = etohs(slstat.alstatuscode);
      }
      state = rval & 0x000f; /* read slave status */
      if (state != reqstate)
      {
         osal_usleep(1000);
      }
   }
   while ((state != reqstate) && (osal_timer_is_expired(&timer) == FALSE));
   context->slavelist[slave].state = rval;

   return state;
}

/** Get index of next mailbox counter value.
 * Used for Mailbox Link Layer.
 * @param[in] cnt     = Mailbox counter value [0..7]
 * @return next mailbox counter value
 */
uint8 Nex_nextmbxcnt(uint8 cnt)
{
   cnt++;
   if (cnt > 7)
   {
      cnt = 1; /* wrap around to 1, not 0 */
   }

   return cnt;
}

/** Clear mailbox buffer.
 * @param[out] Mbx     = Mailbox buffer to clear
 */
void Nex_clearmbx(Nex_mbxbuft *Mbx)
{
    memset(Mbx, 0x00, Nex_MAXMBX);
}

/** Check if IN mailbox of slave is empty.
 * @param[in] context  = context struct
 * @param[in] slave    = Slave number
 * @param[in] timeout  = Timeout in us
 * @return >0 is success
 */
int Nexx__mbxempty(Nexx__contextt *context, uint16 slave, int timeout)
{
   uint16 configadr;
   uint8 SMstat;
   int wkc;
   osal_timert timer;

   osal_timer_start(&timer, timeout);
   configadr = context->slavelist[slave].configadr;
   do
   {
      SMstat = 0;
      wkc = Nexx__FPRD(context->port, configadr, ECT_REG_SM0STAT, sizeof(SMstat), &SMstat, Nex_TIMEOUTRET);
      SMstat = etohs(SMstat);
      if (((SMstat & 0x08) != 0) && (timeout > Nex_LOCALDELAY))
      {
         osal_usleep(Nex_LOCALDELAY);
      }
   }
   while (((wkc <= 0) || ((SMstat & 0x08) != 0)) && (osal_timer_is_expired(&timer) == FALSE));

   if ((wkc > 0) && ((SMstat & 0x08) == 0))
   {
      return 1;
   }

   return 0;
}

/** Write IN mailbox to slave.
 * @param[in]  context    = context struct
 * @param[in]  slave      = Slave number
 * @param[out] mbx        = Mailbox data
 * @param[in]  timeout    = Timeout in us
 * @return Work counter (>0 is success)
 */
int Nexx__mbxsend(Nexx__contextt *context, uint16 slave,Nex_mbxbuft *mbx, int timeout)
{
   uint16 mbxwo,mbxl,configadr;
   int wkc;

   wkc = 0;
   configadr = context->slavelist[slave].configadr;
   mbxl = context->slavelist[slave].mbx_l;
   if ((mbxl > 0) && (mbxl <= Nex_MAXMBX))
   {
      if (Nexx__mbxempty(context, slave, timeout))
      {
         mbxwo = context->slavelist[slave].mbx_wo;
         /* write slave in mailbox */
         wkc = Nexx__FPWR(context->port, configadr, mbxwo, mbxl, mbx, Nex_TIMEOUTRET3);
      }
      else
      {
         wkc = 0;
      }
   }

   return wkc;
}

/** Read OUT mailbox from slave.
 * Supports Mailbox Link Layer with repeat requests.
 * @param[in]  context    = context struct
 * @param[in]  slave      = Slave number
 * @param[out] mbx        = Mailbox data
 * @param[in]  timeout    = Timeout in us
 * @return Work counter (>0 is success)
 */
int Nexx__mbxreceive(Nexx__contextt *context, uint16 slave, Nex_mbxbuft *mbx, int timeout)
{
   uint16 mbxro,mbxl,configadr;
   int wkc=0;
   int wkc2;
   uint16 SMstat;
   uint8 SMcontr;
   Nex_mbxheadert *mbxh;
   Nex_emcyt *EMp;
   Nex_mbxerrort *MBXEp;

   configadr = context->slavelist[slave].configadr;
   mbxl = context->slavelist[slave].mbx_rl;
   if ((mbxl > 0) && (mbxl <= Nex_MAXMBX))
   {
      osal_timert timer;

      osal_timer_start(&timer, timeout);
      wkc = 0;
      do /* wait for read mailbox available */
      {
         SMstat = 0;
         wkc = Nexx__FPRD(context->port, configadr, ECT_REG_SM1STAT, sizeof(SMstat), &SMstat, Nex_TIMEOUTRET);
         SMstat = etohs(SMstat);
         if (((SMstat & 0x08) == 0) && (timeout > Nex_LOCALDELAY))
         {
            osal_usleep(Nex_LOCALDELAY);
         }
      }
      while (((wkc <= 0) || ((SMstat & 0x08) == 0)) && (osal_timer_is_expired(&timer) == FALSE));

      if ((wkc > 0) && ((SMstat & 0x08) > 0)) /* read mailbox available ? */
      {
         mbxro = context->slavelist[slave].mbx_ro;
         mbxh = (Nex_mbxheadert *)mbx;
         do
         {
            wkc = Nexx__FPRD(context->port, configadr, mbxro, mbxl, mbx, Nex_TIMEOUTRET); /* get mailbox */
            if ((wkc > 0) && ((mbxh->mbxtype & 0x0f) == 0x00)) /* Mailbox error response? */
            {
               MBXEp = (Nex_mbxerrort *)mbx;
               Nexx__mbxerror(context, slave, etohs(MBXEp->Detail));
               wkc = 0; /* prevent emergency to cascade up, it is already handled. */
            }
            else if ((wkc > 0) && ((mbxh->mbxtype & 0x0f) == 0x03)) /* CoE response? */
            {
               EMp = (Nex_emcyt *)mbx;
               if ((etohs(EMp->CANOpen) >> 12) == 0x01) /* Emergency request? */
               {
                  Nexx__mbxemergencyerror(context, slave, etohs(EMp->ErrorCode), EMp->ErrorReg,
                          EMp->bData, etohs(EMp->w1), etohs(EMp->w2));
                  wkc = 0; /* prevent emergency to cascade up, it is already handled. */
               }
            }
            else
            {
               if (wkc <= 0) /* read mailbox lost */
               {
                  SMstat ^= 0x0200; /* toggle repeat request */
                  SMstat = htoes(SMstat);
                  wkc2 = Nexx__FPWR(context->port, configadr, ECT_REG_SM1STAT, sizeof(SMstat), &SMstat, Nex_TIMEOUTRET);
                  SMstat = etohs(SMstat);
                  do /* wait for toggle ack */
                  {
                     wkc2 = Nexx__FPRD(context->port, configadr, ECT_REG_SM1CONTR, sizeof(SMcontr), &SMcontr, Nex_TIMEOUTRET);
                   } while (((wkc2 <= 0) || ((SMcontr & 0x02) != (HI_BYTE(SMstat) & 0x02))) && (osal_timer_is_expired(&timer) == FALSE));
                  do /* wait for read mailbox available */
                  {
                     wkc2 = Nexx__FPRD(context->port, configadr, ECT_REG_SM1STAT, sizeof(SMstat), &SMstat, Nex_TIMEOUTRET);
                     SMstat = etohs(SMstat);
                     if (((SMstat & 0x08) == 0) && (timeout > Nex_LOCALDELAY))
                     {
                        osal_usleep(Nex_LOCALDELAY);
                     }
                  } while (((wkc2 <= 0) || ((SMstat & 0x08) == 0)) && (osal_timer_is_expired(&timer) == FALSE));
               }
            }
         } while ((wkc <= 0) && (osal_timer_is_expired(&timer) == FALSE)); /* if WKC<=0 repeat */
      }
      else /* no read mailbox available */
      {
          wkc = 0;
      }
   }

   return wkc;
}

/** Dump complete EEPROM data from slave in buffer.
 * @param[in]  context  = context struct
 * @param[in]  slave    = Slave number
 * @param[out] esibuf   = EEPROM data buffer, make sure it is big enough.
 */
void Nexx__esidump(Nexx__contextt *context, uint16 slave, uint8 *esibuf)
{
   int address, incr;
   uint16 configadr;
   uint64 *p64;
   uint16 *p16;
   uint64 edat;
   uint8 eectl = context->slavelist[slave].eep_pdi;

   Nexx__eeprom2master(context, slave); /* set eeprom control to master */
   configadr = context->slavelist[slave].configadr;
   address = ECT_SII_START;
   p16=(uint16*)esibuf;
   if (context->slavelist[slave].eep_8byte)
   {
      incr = 4;
   }
   else
   {
      incr = 2;
   }
   do
   {
      edat = Nexx__readeepromFP(context, configadr, address, Nex_TIMEOUTEEP);
      p64 = (uint64*)p16;
      *p64 = edat;
      p16 += incr;
      address += incr;
   } while ((address <= (Nex_MAXEEPBUF >> 1)) && ((uint32)edat != 0xffffffff));

   if (eectl)
   {
      Nexx__eeprom2pdi(context, slave); /* if eeprom control was previously pdi then restore */
   }
}

/** Read EEPROM from slave bypassing cache.
 * @param[in] context   = context struct
 * @param[in] slave     = Slave number
 * @param[in] eeproma   = (WORD) Address in the EEPROM
 * @param[in] timeout   = Timeout in us.
 * @return EEPROM data 32bit
 */
uint32 Nexx__readeeprom(Nexx__contextt *context, uint16 slave, uint16 eeproma, int timeout)
{
   uint16 configadr;

   Nexx__eeprom2master(context, slave); /* set eeprom control to master */
   configadr = context->slavelist[slave].configadr;

   return ((uint32)Nexx__readeepromFP(context, configadr, eeproma, timeout));
}

/** Write EEPROM to slave bypassing cache.
 * @param[in] context   = context struct
 * @param[in] slave     = Slave number
 * @param[in] eeproma   = (WORD) Address in the EEPROM
 * @param[in] data      = 16bit data
 * @param[in] timeout   = Timeout in us.
 * @return >0 if OK
 */
int Nexx__writeeeprom(Nexx__contextt *context, uint16 slave, uint16 eeproma, uint16 data, int timeout)
{
   uint16 configadr;

   Nexx__eeprom2master(context, slave); /* set eeprom control to master */
   configadr = context->slavelist[slave].configadr;
   return (Nexx__writeeepromFP(context, configadr, eeproma, data, timeout));
}

/** Set eeprom control to master. Only if set to PDI.
 * @param[in] context   = context struct
 * @param[in] slave     = Slave number
 * @return >0 if OK
 */
int Nexx__eeprom2master(Nexx__contextt *context, uint16 slave)
{
   int wkc = 1, cnt = 0;
   uint16 configadr;
   uint8 eepctl;

   if ( context->slavelist[slave].eep_pdi )
   {
      configadr = context->slavelist[slave].configadr;
      eepctl = 2;
      do
      {
         wkc = Nexx__FPWR(context->port, configadr, ECT_REG_EEPCFG, sizeof(eepctl), &eepctl , Nex_TIMEOUTRET); /* force Eeprom from PDI */
      }
      while ((wkc <= 0) && (cnt++ < Nex_DEFAULTRETRIES));
      eepctl = 0;
      cnt = 0;
      do
      {
         wkc = Nexx__FPWR(context->port, configadr, ECT_REG_EEPCFG, sizeof(eepctl), &eepctl , Nex_TIMEOUTRET); /* set Eeprom to master */
      }
      while ((wkc <= 0) && (cnt++ < Nex_DEFAULTRETRIES));
      context->slavelist[slave].eep_pdi = 0;
   }

   return wkc;
}

/** Set eeprom control to PDI. Only if set to master.
 * @param[in]  context        = context struct
 * @param[in] slave     = Slave number
 * @return >0 if OK
 */
int Nexx__eeprom2pdi(Nexx__contextt *context, uint16 slave)
{
   int wkc = 1, cnt = 0;
   uint16 configadr;
   uint8 eepctl;

   if ( !context->slavelist[slave].eep_pdi )
   {
      configadr = context->slavelist[slave].configadr;
      eepctl = 1;
      do
      {
         wkc = Nexx__FPWR(context->port, configadr, ECT_REG_EEPCFG, sizeof(eepctl), &eepctl , Nex_TIMEOUTRET); /* set Eeprom to PDI */
      }
      while ((wkc <= 0) && (cnt++ < Nex_DEFAULTRETRIES));
      context->slavelist[slave].eep_pdi = 1;
   }

   return wkc;
}

uint16 Nexx__eeprom_waitnotbusyAP(Nexx__contextt *context, uint16 aiadr,uint16 *estat, int timeout)
{
   int wkc, cnt = 0, retval = 0;
   osal_timert timer;

   osal_timer_start(&timer, timeout);
   do
   {
      if (cnt++)
      {
         osal_usleep(Nex_LOCALDELAY);
      }
      *estat = 0;
      wkc=Nexx__APRD(context->port, aiadr, ECT_REG_EEPSTAT, sizeof(*estat), estat, Nex_TIMEOUTRET);
      *estat = etohs(*estat);
   }
   while (((wkc <= 0) || ((*estat & Nex_ESTAT_BUSY) > 0)) && (osal_timer_is_expired(&timer) == FALSE)); /* wait for eeprom ready */
   if ((*estat & Nex_ESTAT_BUSY) == 0)
   {
      retval = 1;
   }

   return retval;
}

/** Read EEPROM from slave bypassing cache. APRD method.
 * @param[in] context     = context struct
 * @param[in] aiadr       = auto increment address of slave
 * @param[in] eeproma     = (WORD) Address in the EEPROM
 * @param[in] timeout     = Timeout in us.
 * @return EEPROM data 64bit or 32bit
 */
uint64 Nexx__readeepromAP(Nexx__contextt *context, uint16 aiadr, uint16 eeproma, int timeout)
{
   uint16 estat;
   uint32 edat32;
   uint64 edat64;
   Nex_eepromt ed;
   int wkc, cnt, nackcnt = 0;

   edat64 = 0;
   edat32 = 0;
   if (Nexx__eeprom_waitnotbusyAP(context, aiadr, &estat, timeout))
   {
      if (estat & Nex_ESTAT_EMASK) /* error bits are set */
      {
         estat = htoes(Nex_ECMD_NOP); /* clear error bits */
         wkc = Nexx__APWR(context->port, aiadr, ECT_REG_EEPCTL, sizeof(estat), &estat, Nex_TIMEOUTRET3);
      }

      do
      {
         ed.comm = htoes(Nex_ECMD_READ);
         ed.addr = htoes(eeproma);
         ed.d2   = 0x0000;
         cnt = 0;
         do
         {
            wkc = Nexx__APWR(context->port, aiadr, ECT_REG_EEPCTL, sizeof(ed), &ed, Nex_TIMEOUTRET);
         }
         while ((wkc <= 0) && (cnt++ < Nex_DEFAULTRETRIES));
         if (wkc)
         {
            osal_usleep(Nex_LOCALDELAY);
            estat = 0x0000;
            if (Nexx__eeprom_waitnotbusyAP(context, aiadr, &estat, timeout))
            {
               if (estat & Nex_ESTAT_NACK)
               {
                  nackcnt++;
                  osal_usleep(Nex_LOCALDELAY * 5);
               }
               else
               {
                  nackcnt = 0;
                  if (estat & Nex_ESTAT_R64)
                  {
                     cnt = 0;
                     do
                     {
                        wkc = Nexx__APRD(context->port, aiadr, ECT_REG_EEPDAT, sizeof(edat64), &edat64, Nex_TIMEOUTRET);
                     }
                     while ((wkc <= 0) && (cnt++ < Nex_DEFAULTRETRIES));
                  }
                  else
                  {
                     cnt = 0;
                     do
                     {
                        wkc = Nexx__APRD(context->port, aiadr, ECT_REG_EEPDAT, sizeof(edat32), &edat32, Nex_TIMEOUTRET);
                     }
                     while ((wkc <= 0) && (cnt++ < Nex_DEFAULTRETRIES));
                     edat64=(uint64)edat32;
                  }
               }
            }
         }
      }
      while ((nackcnt > 0) && (nackcnt < 3));
   }

   return edat64;
}

/** Write EEPROM to slave bypassing cache. APWR method.
 * @param[in] context   = context struct
 * @param[in] aiadr     = configured address of slave
 * @param[in] eeproma   = (WORD) Address in the EEPROM
 * @param[in] data      = 16bit data
 * @param[in] timeout   = Timeout in us.
 * @return >0 if OK
 */
int Nexx__writeeepromAP(Nexx__contextt *context, uint16 aiadr, uint16 eeproma, uint16 data, int timeout)
{
   uint16 estat;
   Nex_eepromt ed;
   int wkc, rval = 0, cnt = 0, nackcnt = 0;

   if (Nexx__eeprom_waitnotbusyAP(context, aiadr, &estat, timeout))
   {
      if (estat & Nex_ESTAT_EMASK) /* error bits are set */
      {
         estat = htoes(Nex_ECMD_NOP); /* clear error bits */
         wkc = Nexx__APWR(context->port, aiadr, ECT_REG_EEPCTL, sizeof(estat), &estat, Nex_TIMEOUTRET3);
      }
      do
      {
         cnt = 0;
         do
         {
            wkc = Nexx__APWR(context->port, aiadr, ECT_REG_EEPDAT, sizeof(data), &data, Nex_TIMEOUTRET);
         }
         while ((wkc <= 0) && (cnt++ < Nex_DEFAULTRETRIES));

         ed.comm = Nex_ECMD_WRITE;
         ed.addr = eeproma;
         ed.d2   = 0x0000;
         cnt = 0;
         do
         {
            wkc = Nexx__APWR(context->port, aiadr, ECT_REG_EEPCTL, sizeof(ed), &ed, Nex_TIMEOUTRET);
         }
         while ((wkc <= 0) && (cnt++ < Nex_DEFAULTRETRIES));
         if (wkc)
         {
            osal_usleep(Nex_LOCALDELAY * 2);
            estat = 0x0000;
            if (Nexx__eeprom_waitnotbusyAP(context, aiadr, &estat, timeout))
            {
               if (estat & Nex_ESTAT_NACK)
               {
                  nackcnt++;
                  osal_usleep(Nex_LOCALDELAY * 5);
               }
               else
               {
                  nackcnt = 0;
                  rval = 1;
               }
            }
         }

      }
      while ((nackcnt > 0) && (nackcnt < 3));
   }

   return rval;
}

uint16 Nexx__eeprom_waitnotbusyFP(Nexx__contextt *context, uint16 configadr,uint16 *estat, int timeout)
{
   int wkc, cnt = 0, retval = 0;
   osal_timert timer;

   osal_timer_start(&timer, timeout);
   do
   {
      if (cnt++)
      {
         osal_usleep(Nex_LOCALDELAY);
      }
      *estat = 0;
      wkc=Nexx__FPRD(context->port, configadr, ECT_REG_EEPSTAT, sizeof(*estat), estat, Nex_TIMEOUTRET);
      *estat = etohs(*estat);
   }
   while (((wkc <= 0) || ((*estat & Nex_ESTAT_BUSY) > 0)) && (osal_timer_is_expired(&timer) == FALSE)); /* wait for eeprom ready */
   if ((*estat & Nex_ESTAT_BUSY) == 0)
   {
      retval = 1;
   }

   return retval;
}

/** Read EEPROM from slave bypassing cache. FPRD method.
 * @param[in] context     = context struct
 * @param[in] configadr   = configured address of slave
 * @param[in] eeproma     = (WORD) Address in the EEPROM
 * @param[in] timeout     = Timeout in us.
 * @return EEPROM data 64bit or 32bit
 */
uint64 Nexx__readeepromFP(Nexx__contextt *context, uint16 configadr, uint16 eeproma, int timeout)
{
   uint16 estat;
   uint32 edat32;
   uint64 edat64;
   Nex_eepromt ed;
   int wkc, cnt, nackcnt = 0;

   edat64 = 0;
   edat32 = 0;
   if (Nexx__eeprom_waitnotbusyFP(context, configadr, &estat, timeout))
   {
      if (estat & Nex_ESTAT_EMASK) /* error bits are set */
      {
         estat = htoes(Nex_ECMD_NOP); /* clear error bits */
         wkc=Nexx__FPWR(context->port, configadr, ECT_REG_EEPCTL, sizeof(estat), &estat, Nex_TIMEOUTRET3);
      }

      do
      {
         ed.comm = htoes(Nex_ECMD_READ);
         ed.addr = htoes(eeproma);
         ed.d2   = 0x0000;
         cnt = 0;
         do
         {
            wkc=Nexx__FPWR(context->port, configadr, ECT_REG_EEPCTL, sizeof(ed), &ed, Nex_TIMEOUTRET);
         }
         while ((wkc <= 0) && (cnt++ < Nex_DEFAULTRETRIES));
         if (wkc)
         {
            osal_usleep(Nex_LOCALDELAY);
            estat = 0x0000;
            if (Nexx__eeprom_waitnotbusyFP(context, configadr, &estat, timeout))
            {
               if (estat & Nex_ESTAT_NACK)
               {
                  nackcnt++;
                  osal_usleep(Nex_LOCALDELAY * 5);
               }
               else
               {
                  nackcnt = 0;
                  if (estat & Nex_ESTAT_R64)
                  {
                     cnt = 0;
                     do
                     {
                        wkc=Nexx__FPRD(context->port, configadr, ECT_REG_EEPDAT, sizeof(edat64), &edat64, Nex_TIMEOUTRET);
                     }
                     while ((wkc <= 0) && (cnt++ < Nex_DEFAULTRETRIES));
                  }
                  else
                  {
                     cnt = 0;
                     do
                     {
                        wkc=Nexx__FPRD(context->port, configadr, ECT_REG_EEPDAT, sizeof(edat32), &edat32, Nex_TIMEOUTRET);
                     }
                     while ((wkc <= 0) && (cnt++ < Nex_DEFAULTRETRIES));
                     edat64=(uint64)edat32;
                  }
               }
            }
         }
      }
      while ((nackcnt > 0) && (nackcnt < 3));
   }

   return edat64;
}

/** Write EEPROM to slave bypassing cache. FPWR method.
 * @param[in]  context        = context struct
 * @param[in] configadr   = configured address of slave
 * @param[in] eeproma     = (WORD) Address in the EEPROM
 * @param[in] data        = 16bit data
 * @param[in] timeout     = Timeout in us.
 * @return >0 if OK
 */
int Nexx__writeeepromFP(Nexx__contextt *context, uint16 configadr, uint16 eeproma, uint16 data, int timeout)
{
   uint16 estat;
   Nex_eepromt ed;
   int wkc, rval = 0, cnt = 0, nackcnt = 0;

   if (Nexx__eeprom_waitnotbusyFP(context, configadr, &estat, timeout))
   {
      if (estat & Nex_ESTAT_EMASK) /* error bits are set */
      {
         estat = htoes(Nex_ECMD_NOP); /* clear error bits */
         wkc = Nexx__FPWR(context->port, configadr, ECT_REG_EEPCTL, sizeof(estat), &estat, Nex_TIMEOUTRET3);
      }
      do
      {
         cnt = 0;
         do
         {
            wkc = Nexx__FPWR(context->port, configadr, ECT_REG_EEPDAT, sizeof(data), &data, Nex_TIMEOUTRET);
         }
         while ((wkc <= 0) && (cnt++ < Nex_DEFAULTRETRIES));
         ed.comm = Nex_ECMD_WRITE;
         ed.addr = eeproma;
         ed.d2   = 0x0000;
         cnt = 0;
         do
         {
            wkc = Nexx__FPWR(context->port, configadr, ECT_REG_EEPCTL, sizeof(ed), &ed, Nex_TIMEOUTRET);
         }
         while ((wkc <= 0) && (cnt++ < Nex_DEFAULTRETRIES));
         if (wkc)
         {
            osal_usleep(Nex_LOCALDELAY * 2);
            estat = 0x0000;
            if (Nexx__eeprom_waitnotbusyFP(context, configadr, &estat, timeout))
            {
               if (estat & Nex_ESTAT_NACK)
               {
                  nackcnt++;
                  osal_usleep(Nex_LOCALDELAY * 5);
               }
               else
               {
                  nackcnt = 0;
                  rval = 1;
               }
            }
         }
      }
      while ((nackcnt > 0) && (nackcnt < 3));
   }

   return rval;
}

/** Read EEPROM from slave bypassing cache.
 * Parallel read step 1, make request to slave.
 * @param[in] context     = context struct
 * @param[in] slave       = Slave number
 * @param[in] eeproma     = (WORD) Address in the EEPROM
 */
void Nexx__readeeprom1(Nexx__contextt *context, uint16 slave, uint16 eeproma)
{
   uint16 configadr, estat;
   Nex_eepromt ed;
   int wkc, cnt = 0;

   Nexx__eeprom2master(context, slave); /* set eeprom control to master */
   configadr = context->slavelist[slave].configadr;
   if (Nexx__eeprom_waitnotbusyFP(context, configadr, &estat, Nex_TIMEOUTEEP))
   {
      if (estat & Nex_ESTAT_EMASK) /* error bits are set */
      {
         estat = htoes(Nex_ECMD_NOP); /* clear error bits */
         wkc = Nexx__FPWR(context->port, configadr, ECT_REG_EEPCTL, sizeof(estat), &estat, Nex_TIMEOUTRET3);
      }
      ed.comm = htoes(Nex_ECMD_READ);
      ed.addr = htoes(eeproma);
      ed.d2   = 0x0000;
      do
      {
         wkc = Nexx__FPWR(context->port, configadr, ECT_REG_EEPCTL, sizeof(ed), &ed, Nex_TIMEOUTRET);
      }
      while ((wkc <= 0) && (cnt++ < Nex_DEFAULTRETRIES));
   }
}

/** Read EEPROM from slave bypassing cache.
 * Parallel read step 2, actual read from slave.
 * @param[in]  context        = context struct
 * @param[in] slave       = Slave number
 * @param[in] timeout     = Timeout in us.
 * @return EEPROM data 32bit
 */
uint32 Nexx__readeeprom2(Nexx__contextt *context, uint16 slave, int timeout)
{
   uint16 estat, configadr;
   uint32 edat;
   int wkc, cnt = 0;

   configadr = context->slavelist[slave].configadr;
   edat = 0;
   estat = 0x0000;
   if (Nexx__eeprom_waitnotbusyFP(context, configadr, &estat, timeout))
   {
      do
      {
          wkc = Nexx__FPRD(context->port, configadr, ECT_REG_EEPDAT, sizeof(edat), &edat, Nex_TIMEOUTRET);
      }
      while ((wkc <= 0) && (cnt++ < Nex_DEFAULTRETRIES));
   }

   return edat;
}

/** Push index of segmented LRD/LWR/LRW combination.
 * @param[in]  context        = context struct
 * @param[in] idx         = Used datagram index.
 * @param[in] data        = Pointer to process data segment.
 * @param[in] length      = Length of data segment in bytes.
 */
static void Nexx__pushindex(Nexx__contextt *context, uint8 idx, void *data, uint16 length)
{
   if(context->idxstack->pushed < Nex_MAXBUF)
   {
      context->idxstack->idx[context->idxstack->pushed] = idx;
      context->idxstack->data[context->idxstack->pushed] = data;
      context->idxstack->length[context->idxstack->pushed] = length;
      context->idxstack->pushed++;
   }
}

/** Pull index of segmented LRD/LWR/LRW combination.
 * @param[in]  context        = context struct
 * @return Stack location, -1 if stack is empty.
 */
static int Nexx__pullindex(Nexx__contextt *context)
{
   int rval = -1;
   if(context->idxstack->pulled < context->idxstack->pushed)
   {
      rval = context->idxstack->pulled;
      context->idxstack->pulled++;
   }

   return rval;
}

/** 
 * Clear the idx stack.
 * 
 * @param context           = context struct
 */
static void Nexx__clearindex(Nexx__contextt *context)  {

   context->idxstack->pushed = 0;
   context->idxstack->pulled = 0;

}

/** Transmit processdata to slaves.
 * Uses LRW, or LRD/LWR if LRW is not allowed (blockLRW).
 * Both the input and output processdata are transmitted.
 * The outputs with the actual data, the inputs have a placeholder.
 * The inputs are gathered with the receive processdata function.
 * In contrast to the base LRW function this function is non-blocking.
 * If the processdata does not fit in one datagram, multiple are used.
 * In order to recombine the slave response, a stack is used.
 * @param[in]  context        = context struct
 * @param[in]  group          = group number
 * @return >0 if processdata is transmitted.
 */
static int Nexx__main_send_processdata(Nexx__contextt *context, uint8 group, boolean use_overlap_io)
{
   uint32 LogAdr;
   uint16 w1, w2;
   int length, sublength;
   uint8 idx;
   int wkc;
   uint8* data;
   boolean first=FALSE;
   uint16 currentsegment = 0;
   uint32 iomapinputoffset;

   wkc = 0;
   if(context->grouplist[group].hasdc)
   {
      first = TRUE;
   }

   /* For overlapping IO map use the biggest */
   if(use_overlap_io == TRUE)
   {
      /* For overlap IOmap make the frame EQ big to biggest part */
      length = (context->grouplist[group].Obytes > context->grouplist[group].Ibytes) ?
         context->grouplist[group].Obytes : context->grouplist[group].Ibytes;
      /* Save the offset used to compensate where to save inputs when frame returns */
      iomapinputoffset = context->grouplist[group].Obytes;
   }
   else
   {
      length = context->grouplist[group].Obytes + context->grouplist[group].Ibytes;
      iomapinputoffset = 0;
   }
   
   LogAdr = context->grouplist[group].logstartaddr;
   if(length)
   {

      wkc = 1;
      /* LRW blocked by one or more slaves ? */
      if(context->grouplist[group].blockLRW)
      {
         /* if inputs available generate LRD */
         if(context->grouplist[group].Ibytes)
         {
            currentsegment = context->grouplist[group].Isegment;
            data = context->grouplist[group].inputs;
            length = context->grouplist[group].Ibytes;
            LogAdr += context->grouplist[group].Obytes;
            /* segment transfer if needed */
            do
            {
               if(currentsegment == context->grouplist[group].Isegment)
               {
                  sublength = context->grouplist[group].IOsegment[currentsegment++] - context->grouplist[group].Ioffset;
               }
               else
               {
                  sublength = context->grouplist[group].IOsegment[currentsegment++];
               }
               /* get new index */
               idx = Nexx__getindex(context->port);
               w1 = LO_WORD(LogAdr);
               w2 = HI_WORD(LogAdr);
               Nexx__setupdatagram(context->port, &(context->port->txbuf[idx]), Nex_CMD_LRD, idx, w1, w2, sublength, data);
               if(first)
               {
                  context->DCl = sublength;
                  /* FPRMW in second datagram */
                  context->DCtO = Nexx__adddatagram(context->port, &(context->port->txbuf[idx]), Nex_CMD_FRMW, idx, FALSE,
                                           context->slavelist[context->grouplist[group].DCnext].configadr,
                                           ECT_REG_DCSYSTIME, sizeof(int64), context->DCtime);
                  first = FALSE;
               }
               /* send frame */
               Nexx__outframe_red(context->port, idx);
               /* push index and data pointer on stack */
               Nexx__pushindex(context, idx, data, sublength);
               length -= sublength;
               LogAdr += sublength;
               data += sublength;
            } while (length && (currentsegment < context->grouplist[group].nsegments));
         }
         /* if outputs available generate LWR */
         if(context->grouplist[group].Obytes)
         {
            data = context->grouplist[group].outputs;
            length = context->grouplist[group].Obytes;
            LogAdr = context->grouplist[group].logstartaddr;
            currentsegment = 0;
            /* segment transfer if needed */
            do
            {
               sublength = context->grouplist[group].IOsegment[currentsegment++];
               if((length - sublength) < 0)
               {
                  sublength = length;
               }
               /* get new index */
               idx = Nexx__getindex(context->port);
               w1 = LO_WORD(LogAdr);
               w2 = HI_WORD(LogAdr);
               Nexx__setupdatagram(context->port, &(context->port->txbuf[idx]), Nex_CMD_LWR, idx, w1, w2, sublength, data);
               if(first)
               {
                  context->DCl = sublength;
                  /* FPRMW in second datagram */
                  context->DCtO = Nexx__adddatagram(context->port, &(context->port->txbuf[idx]), Nex_CMD_FRMW, idx, FALSE,
                                           context->slavelist[context->grouplist[group].DCnext].configadr,
                                           ECT_REG_DCSYSTIME, sizeof(int64), context->DCtime);
                  first = FALSE;
               }
               /* send frame */
               Nexx__outframe_red(context->port, idx);
               /* push index and data pointer on stack */
               Nexx__pushindex(context, idx, data, sublength);
               length -= sublength;
               LogAdr += sublength;
               data += sublength;
            } while (length && (currentsegment < context->grouplist[group].nsegments));
         }
      }
      /* LRW can be used */
      else
      {
         if (context->grouplist[group].Obytes)
         {
            data = context->grouplist[group].outputs;
         }
         else
         {
            data = context->grouplist[group].inputs;
            /* Clear offset, don't compensate for overlapping IOmap if we only got inputs */
            iomapinputoffset = 0;
         }
         /* segment transfer if needed */
         do
         {
            sublength = context->grouplist[group].IOsegment[currentsegment++];
            /* get new index */
            idx = Nexx__getindex(context->port);
            w1 = LO_WORD(LogAdr);
            w2 = HI_WORD(LogAdr);
            Nexx__setupdatagram(context->port, &(context->port->txbuf[idx]), Nex_CMD_LRW, idx, w1, w2, sublength, data);
            if(first)
            {
               context->DCl = sublength;
               /* FPRMW in second datagram */
               context->DCtO = Nexx__adddatagram(context->port, &(context->port->txbuf[idx]), Nex_CMD_FRMW, idx, FALSE,
                                        context->slavelist[context->grouplist[group].DCnext].configadr,
                                        ECT_REG_DCSYSTIME, sizeof(int64), context->DCtime);
               first = FALSE;
            }
            /* send frame */
            Nexx__outframe_red(context->port, idx);
            /* push index and data pointer on stack.
             * the iomapinputoffset compensate for where the inputs are stored 
             * in the IOmap if we use an overlapping IOmap. If a regular IOmap
             * is used it should always be 0.
             */
            Nexx__pushindex(context, idx, (data + iomapinputoffset), sublength);      
            length -= sublength;
            LogAdr += sublength;
            data += sublength;
         } while (length && (currentsegment < context->grouplist[group].nsegments));
      }
   }

   return wkc;
}

/** Transmit processdata to slaves.
* Uses LRW, or LRD/LWR if LRW is not allowed (blockLRW).
* Both the input and output processdata are transmitted in the overlapped IOmap.
* The outputs with the actual data, the inputs replace the output data in the
* returning frame. The inputs are gathered with the receive processdata function.
* In contrast to the base LRW function this function is non-blocking.
* If the processdata does not fit in one datagram, multiple are used.
* In order to recombine the slave response, a stack is used.
* @param[in]  context        = context struct
* @param[in]  group          = group number
* @return >0 if processdata is transmitted.
*/
int Nexx__send_overlap_processdata_group(Nexx__contextt *context, uint8 group)
{
   return Nexx__main_send_processdata(context, group, TRUE);
}

/** Transmit processdata to slaves.
* Uses LRW, or LRD/LWR if LRW is not allowed (blockLRW).
* Both the input and output processdata are transmitted.
* The outputs with the actual data, the inputs have a placeholder.
* The inputs are gathered with the receive processdata function.
* In contrast to the base LRW function this function is non-blocking.
* If the processdata does not fit in one datagram, multiple are used.
* In order to recombine the slave response, a stack is used.
* @param[in]  context        = context struct
* @param[in]  group          = group number
* @return >0 if processdata is transmitted.
*/
int Nexx__send_processdata_group(Nexx__contextt *context, uint8 group)
{
   return Nexx__main_send_processdata(context, group, FALSE);
}

/** Receive processdata from slaves.
 * Second part from Nex_send_processdata().
 * Received datagrams are recombined with the processdata with help from the stack.
 * If a datagram contains input processdata it copies it to the processdata structure.
 * @param[in]  context        = context struct
 * @param[in]  group          = group number
 * @param[in]  timeout        = Timeout in us.
 * @return Work counter.
 */
int Nexx__receive_processdata_group(Nexx__contextt *context, uint8 group, int timeout)
{
   int pos, idx;
   int wkc = 0, wkc2;
   uint16 le_wkc = 0;
   int valid_wkc = 0;
   int64 le_DCtime;
   boolean first = FALSE;

   if(context->grouplist[group].hasdc)
   {
      first = TRUE;
   }
   /* get first index */
   pos = Nexx__pullindex(context);
   /* read the same number of frames as send */
   while (pos >= 0)
   {
      idx = context->idxstack->idx[pos];
      wkc2 = Nexx__waitinframe(context->port, context->idxstack->idx[pos], timeout);
      /* check if there is input data in frame */
      if (wkc2 > Nex_NOFRAME)
      {
         if((context->port->rxbuf[idx][Nex_CMDOFFSET]==Nex_CMD_LRD) || (context->port->rxbuf[idx][Nex_CMDOFFSET]==Nex_CMD_LRW))
         {
            if(first)
            {
               memcpy(context->idxstack->data[pos], &(context->port->rxbuf[idx][Nex_HEADERSIZE]), context->DCl);
               memcpy(&le_wkc, &(context->port->rxbuf[idx][Nex_HEADERSIZE + context->DCl]), Nex_WKCSIZE);
               wkc = etohs(le_wkc);
               memcpy(&le_DCtime, &(context->port->rxbuf[idx][context->DCtO]), sizeof(le_DCtime));
               *(context->DCtime) = etohll(le_DCtime);
               first = FALSE;
            }
            else
            {
               /* copy input data back to process data buffer */
               memcpy(context->idxstack->data[pos], &(context->port->rxbuf[idx][Nex_HEADERSIZE]), context->idxstack->length[pos]);
               wkc += wkc2;
            }
            valid_wkc = 1;
         }
         else if(context->port->rxbuf[idx][Nex_CMDOFFSET]==Nex_CMD_LWR)
         {
            if(first)
            {
               memcpy(&le_wkc, &(context->port->rxbuf[idx][Nex_HEADERSIZE + context->DCl]), Nex_WKCSIZE);
               /* output WKC counts 2 times when using LRW, emulate the same for LWR */
               wkc = etohs(le_wkc) * 2;
               memcpy(&le_DCtime, &(context->port->rxbuf[idx][context->DCtO]), sizeof(le_DCtime));
               *(context->DCtime) = etohll(le_DCtime);
               first = FALSE;
            }
            else
            {
               /* output WKC counts 2 times when using LRW, emulate the same for LWR */
               wkc += wkc2 * 2;
            }
            valid_wkc = 1;
         }
      }
      /* release buffer */
      Nexx__setbufstat(context->port, idx, Nex_BUF_EMPTY);
      /* get next index */
      pos = Nexx__pullindex(context);
   }

   Nexx__clearindex(context);

   /* if no frames has arrived */
   if (valid_wkc == 0)
   {
      return Nex_NOFRAME;
   }
   return wkc;
}


int Nexx__send_processdata(Nexx__contextt *context)
{
   return Nexx__send_processdata_group(context, 0);
}

int Nexx__send_overlap_processdata(Nexx__contextt *context)
{
   return Nexx__send_overlap_processdata_group(context, 0);
}

int Nexx__receive_processdata(Nexx__contextt *context, int timeout)
{
   return Nexx__receive_processdata_group(context, 0, timeout);
}


void Nex_pusherror(const Nex_errort *Ec)
{
   Nexx__pusherror(&Nexx__context, Ec);
}

boolean Nex_poperror(Nex_errort *Ec)
{
   return Nexx__poperror(&Nexx__context, Ec);
}

boolean Nex_iserror(void)
{
   return Nexx__iserror(&Nexx__context);
}

void Nex_packeterror(uint16 Slave, uint16 Index, uint8 SubIdx, uint16 ErrorCode)
{
   Nexx__packeterror(&Nexx__context, Slave, Index, SubIdx, ErrorCode);
}

/** Initialise lib in single NIC mode
 * @param[in] ifname   = Dev name, f.e. "eth0"
 * @return >0 if OK
 * @see Nexx__init
 */
int Nex_init(const char * ifname)
{
   return Nexx__init(&Nexx__context, ifname);
}

/** Initialise lib in redundant NIC mode
 * @param[in]  ifname   = Primary Dev name, f.e. "eth0"
 * @param[in]  if2name  = Secondary Dev name, f.e. "eth1"
 * @return >0 if OK
 * @see Nexx__init_redundant
 */
int Nex_init_redundant(const char *ifname, char *if2name)
{
   return Nexx__init_redundant (&Nexx__context, &Nexx__redport, ifname, if2name);
}

/** Close lib.
 * @see Nexx__close
 */
void Nex_close(void)
{
   Nexx__close(&Nexx__context);
};

/** Read one byte from slave EEPROM via cache.
 *  If the cache location is empty then a read request is made to the slave.
 *  Depending on the slave capabillities the request is 4 or 8 bytes.
 *  @param[in] slave   = slave number
 *  @param[in] address = eeprom address in bytes (slave uses words)
 *  @return requested byte, if not available then 0xff
 * @see Nexx__siigetbyte
 */
uint8 Nex_siigetbyte(uint16 slave, uint16 address)
{
   return Nexx__siigetbyte (&Nexx__context, slave, address);
}

/** Find SII section header in slave EEPROM.
 *  @param[in] slave   = slave number
 *  @param[in] cat     = section category
 *  @return byte address of section at section length entry, if not available then 0
 *  @see Nexx__siifind
 */
int16 Nex_siifind(uint16 slave, uint16 cat)
{
   return Nexx__siifind (&Nexx__context, slave, cat);
}

/** Get string from SII string section in slave EEPROM.
 *  @param[out] str    = requested string, 0x00 if not found
 *  @param[in]  slave  = slave number
 *  @param[in]  Sn     = string number
 *  @see Nexx__siistring
 */
void Nex_siistring(char *str, uint16 slave, uint16 Sn)
{
   Nexx__siistring(&Nexx__context, str, slave, Sn);
}

/** Get FMMU data from SII FMMU section in slave EEPROM.
 *  @param[in]  slave  = slave number
 *  @param[out] FMMU   = FMMU struct from SII, max. 4 FMMU's
 *  @return number of FMMU's defined in section
 *  @see Nexx__siiFMMU
 */
uint16 Nex_siiFMMU(uint16 slave, Nex_eepromFMMUt* FMMU)
{
   return Nexx__siiFMMU (&Nexx__context, slave, FMMU);
}

/** Get SM data from SII SM section in slave EEPROM.
 *  @param[in]  slave   = slave number
 *  @param[out] SM      = first SM struct from SII
 *  @return number of SM's defined in section
 *  @see Nexx__siiSM
 */
uint16 Nex_siiSM(uint16 slave, Nex_eepromSMt* SM)
{
   return Nexx__siiSM (&Nexx__context, slave, SM);
}

/** Get next SM data from SII SM section in slave EEPROM.
 *  @param[in]  slave  = slave number
 *  @param[out] SM     = first SM struct from SII
 *  @param[in]  n      = SM number
 *  @return >0 if OK
 *  @see Nexx__siiSMnext
 */
uint16 Nex_siiSMnext(uint16 slave, Nex_eepromSMt* SM, uint16 n)
{
   return Nexx__siiSMnext (&Nexx__context, slave, SM, n);
}

/** Get PDO data from SII PDO section in slave EEPROM.
 *  @param[in]  slave  = slave number
 *  @param[out] PDO    = PDO struct from SII
 *  @param[in]  t      = 0=RXPDO 1=TXPDO
 *  @return mapping size in bits of PDO
 *  @see Nexx__siiPDO
 */
int Nex_siiPDO(uint16 slave, Nex_eepromPDOt* PDO, uint8 t)
{
   return Nexx__siiPDO (&Nexx__context, slave, PDO, t);
}

/** Read all slave states in Nex_slave.
 * @return lowest state found
 * @see Nexx__readstate
 */
int Nex_readstate(void)
{
   return Nexx__readstate (&Nexx__context);
}

/** Write slave state, if slave = 0 then write to all slaves.
 * The function does not check if the actual state is changed.
 * @param[in] slave = Slave number, 0 = master
 * @return 0
 * @see Nexx__writestate
 */
int Nex_writestate(uint16 slave)
{
   return Nexx__writestate(&Nexx__context, slave);
}

/** Check actual slave state.
 * This is a blocking function.
 * @param[in] slave       = Slave number, 0 = all slaves
 * @param[in] reqstate    = Requested state
 * @param[in] timeout     = Timout value in us
 * @return Requested state, or found state after timeout.
 * @see Nexx__statecheck
 */
uint16 Nex_statecheck(uint16 slave, uint16 reqstate, int timeout)
{
   return Nexx__statecheck (&Nexx__context, slave, reqstate, timeout);
}

/** Check if IN mailbox of slave is empty.
 * @param[in] slave    = Slave number
 * @param[in] timeout  = Timeout in us
 * @return >0 is success
 * @see Nexx__mbxempty
 */
int Nex_mbxempty(uint16 slave, int timeout)
{
   return Nexx__mbxempty (&Nexx__context, slave, timeout);
}

/** Write IN mailbox to slave.
 * @param[in]  slave      = Slave number
 * @param[out] mbx        = Mailbox data
 * @param[in]  timeout    = Timeout in us
 * @return Work counter (>0 is success)
 * @see Nexx__mbxsend
 */
int Nex_mbxsend(uint16 slave,Nex_mbxbuft *mbx, int timeout)
{
   return Nexx__mbxsend (&Nexx__context, slave, mbx, timeout);
}

/** Read OUT mailbox from slave.
 * Supports Mailbox Link Layer with repeat requests.
 * @param[in]  slave      = Slave number
 * @param[out] mbx        = Mailbox data
 * @param[in]  timeout    = Timeout in us
 * @return Work counter (>0 is success)
 * @see Nexx__mbxreceive
 */
int Nex_mbxreceive(uint16 slave, Nex_mbxbuft *mbx, int timeout)
{
   return Nexx__mbxreceive (&Nexx__context, slave, mbx, timeout);
}

/** Dump complete EEPROM data from slave in buffer.
 * @param[in]  slave    = Slave number
 * @param[out] esibuf   = EEPROM data buffer, make sure it is big enough.
 * @see Nexx__esidump
 */
void Nex_esidump(uint16 slave, uint8 *esibuf)
{
   Nexx__esidump (&Nexx__context, slave, esibuf);
}

/** Read EEPROM from slave bypassing cache.
 * @param[in] slave     = Slave number
 * @param[in] eeproma   = (WORD) Address in the EEPROM
 * @param[in] timeout   = Timeout in us.
 * @return EEPROM data 32bit
 * @see Nexx__readeeprom
 */
uint32 Nex_readeeprom(uint16 slave, uint16 eeproma, int timeout)
{
   return Nexx__readeeprom (&Nexx__context, slave, eeproma, timeout);
}

/** Write EEPROM to slave bypassing cache.
 * @param[in] slave     = Slave number
 * @param[in] eeproma   = (WORD) Address in the EEPROM
 * @param[in] data      = 16bit data
 * @param[in] timeout   = Timeout in us.
 * @return >0 if OK
 * @see Nexx__writeeeprom
 */
int Nex_writeeeprom(uint16 slave, uint16 eeproma, uint16 data, int timeout)
{
   return Nexx__writeeeprom (&Nexx__context, slave, eeproma, data, timeout);
}

/** Set eeprom control to master. Only if set to PDI.
 * @param[in] slave = Slave number
 * @return >0 if OK
 * @see Nexx__eeprom2master
 */
int Nex_eeprom2master(uint16 slave)
{
   return Nexx__eeprom2master(&Nexx__context, slave);
}

int Nex_eeprom2pdi(uint16 slave)
{
   return Nexx__eeprom2pdi(&Nexx__context, slave);
}

uint16 Nex_eeprom_waitnotbusyAP(uint16 aiadr,uint16 *estat, int timeout)
{
   return Nexx__eeprom_waitnotbusyAP (&Nexx__context, aiadr, estat, timeout);
}

/** Read EEPROM from slave bypassing cache. APRD method.
 * @param[in] aiadr       = auto increment address of slave
 * @param[in] eeproma     = (WORD) Address in the EEPROM
 * @param[in] timeout     = Timeout in us.
 * @return EEPROM data 64bit or 32bit
 */
uint64 Nex_readeepromAP(uint16 aiadr, uint16 eeproma, int timeout)
{
   return Nexx__readeepromAP (&Nexx__context, aiadr, eeproma, timeout);
}

/** Write EEPROM to slave bypassing cache. APWR method.
 * @param[in] aiadr     = configured address of slave
 * @param[in] eeproma   = (WORD) Address in the EEPROM
 * @param[in] data      = 16bit data
 * @param[in] timeout   = Timeout in us.
 * @return >0 if OK
 * @see Nexx__writeeepromAP
 */
int Nex_writeeepromAP(uint16 aiadr, uint16 eeproma, uint16 data, int timeout)
{
   return Nexx__writeeepromAP (&Nexx__context, aiadr, eeproma, data, timeout);
}

uint16 Nex_eeprom_waitnotbusyFP(uint16 configadr,uint16 *estat, int timeout)
{
   return Nexx__eeprom_waitnotbusyFP (&Nexx__context, configadr, estat, timeout);
}

/** Read EEPROM from slave bypassing cache. FPRD method.
 * @param[in] configadr   = configured address of slave
 * @param[in] eeproma     = (WORD) Address in the EEPROM
 * @param[in] timeout     = Timeout in us.
 * @return EEPROM data 64bit or 32bit
 * @see Nexx__readeepromFP
 */
uint64 Nex_readeepromFP(uint16 configadr, uint16 eeproma, int timeout)
{
   return Nexx__readeepromFP (&Nexx__context, configadr, eeproma, timeout);
}

/** Write EEPROM to slave bypassing cache. FPWR method.
 * @param[in] configadr   = configured address of slave
 * @param[in] eeproma     = (WORD) Address in the EEPROM
 * @param[in] data        = 16bit data
 * @param[in] timeout     = Timeout in us.
 * @return >0 if OK
 * @see Nexx__writeeepromFP
 */
int Nex_writeeepromFP(uint16 configadr, uint16 eeproma, uint16 data, int timeout)
{
   return Nexx__writeeepromFP (&Nexx__context, configadr, eeproma, data, timeout);
}

/** Read EEPROM from slave bypassing cache.
 * Parallel read step 1, make request to slave.
 * @param[in] slave       = Slave number
 * @param[in] eeproma     = (WORD) Address in the EEPROM
 * @see Nexx__readeeprom1
 */
void Nex_readeeprom1(uint16 slave, uint16 eeproma)
{
   Nexx__readeeprom1 (&Nexx__context, slave, eeproma);
}

/** Read EEPROM from slave bypassing cache.
 * Parallel read step 2, actual read from slave.
 * @param[in] slave       = Slave number
 * @param[in] timeout     = Timeout in us.
 * @return EEPROM data 32bit
 * @see Nexx__readeeprom2
 */
uint32 Nex_readeeprom2(uint16 slave, int timeout)
{
   return Nexx__readeeprom2 (&Nexx__context, slave, timeout);
}

/** Transmit processdata to slaves.
 * Uses LRW, or LRD/LWR if LRW is not allowed (blockLRW).
 * Both the input and output processdata are transmitted.
 * The outputs with the actual data, the inputs have a placeholder.
 * The inputs are gathered with the receive processdata function.
 * In contrast to the base LRW function this function is non-blocking.
 * If the processdata does not fit in one datagram, multiple are used.
 * In order to recombine the slave response, a stack is used.
 * @param[in]  group          = group number
 * @return >0 if processdata is transmitted.
 * @see Nexx__send_processdata_group
 */
int Nex_send_processdata_group(uint8 group)
{
   return Nexx__send_processdata_group (&Nexx__context, group);
}

/** Transmit processdata to slaves.
* Uses LRW, or LRD/LWR if LRW is not allowed (blockLRW).
* Both the input and output processdata are transmitted in the overlapped IOmap.
* The outputs with the actual data, the inputs replace the output data in the
* returning frame. The inputs are gathered with the receive processdata function.
* In contrast to the base LRW function this function is non-blocking.
* If the processdata does not fit in one datagram, multiple are used.
* In order to recombine the slave response, a stack is used.
* @param[in]  context        = context struct
* @param[in]  group          = group number
* @return >0 if processdata is transmitted.
* @see Nexx__send_overlap_processdata_group
*/
int Nex_send_overlap_processdata_group(uint8 group)
{
   return Nexx__send_overlap_processdata_group(&Nexx__context, group);
}

/** Receive processdata from slaves.
 * Second part from Nex_send_processdata().
 * Received datagrams are recombined with the processdata with help from the stack.
 * If a datagram contains input processdata it copies it to the processdata structure.
 * @param[in]  group          = group number
 * @param[in]  timeout        = Timeout in us.
 * @return Work counter.
 * @see Nexx__receive_processdata_group
 */
int Nex_receive_processdata_group(uint8 group, int timeout)
{
   return Nexx__receive_processdata_group (&Nexx__context, group, timeout);
}

int Nex_send_processdata(void)
{
   return Nex_send_processdata_group(0);
}

int Nex_send_overlap_processdata(void)
{
   return Nex_send_overlap_processdata_group(0);
}

int Nex_receive_processdata(int timeout)
{
   return Nex_receive_processdata_group(0, timeout);
}

