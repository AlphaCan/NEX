/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

/** \file
 * \brief
 * Configuration module for EtherCAT master.
 *
 * After successful initialisation with Nex_init() or Nex_init_redundant()
 * the slaves can be auto configured with this module.
 */

#include <stdio.h>
#include <string.h>
#include "osal.h"
#include "oshw.h"
#include "ethercattype.h"
#include "ethercatbase.h"
#include "ethercatmain.h"
#include "ethercatcoe.h"
#include "ethercatsoe.h"
#include "ethercatconfig.h"

// define if debug printf is needed
//#define Nex_DEBUG

#ifdef Nex_DEBUG
#define Nex_PRINT printf
#else
#define Nex_PRINT(...) do {} while (0)
#endif

typedef struct
{
   int thread_n;
   int running;
   Nexx__contextt *context;
   uint16 slave;
} Nexx__mapt_t;

Nexx__mapt_t Nexx__mapt[Nex_MAX_MAPT];
OSAL_THREAD_HANDLE Nexx__threadh[Nex_MAX_MAPT];


/** Slave configuration structure */
typedef const struct
{
   /** Manufacturer code of slave */
   uint32           man;
   /** ID of slave */
   uint32           id;
   /** Readable name */
   char             name[Nex_MAXNAME + 1];
   /** Data type */
   uint8            Dtype;
   /** Input bits */
   uint16            Ibits;
   /** Output bits */
   uint16           Obits;
   /** SyncManager 2 address */
   uint16           SM2a;
   /** SyncManager 2 flags */
   uint32           SM2f;
   /** SyncManager 3 address */
   uint16           SM3a;
   /** SyncManager 3 flags */
   uint32           SM3f;
   /** FMMU 0 activation */
   uint8            FM0ac;
   /** FMMU 1 activation */
   uint8            FM1ac;
} Nex_configlist_t;




/** standard SM0 flags configuration for mailbox slaves */
#define Nex_DEFAULTMBXSM0  0x00010026
/** standard SM1 flags configuration for mailbox slaves */
#define Nex_DEFAULTMBXSM1  0x00010022
/** standard SM0 flags configuration for digital output slaves */
#define Nex_DEFAULTDOSM0   0x00010044



void Nexx__init_context(Nexx__contextt *context)
{
   int lp;
   *(context->slavecount) = 0;
   /* clean Nex_slave array */
   memset(context->slavelist, 0x00, sizeof(Nex_slavet) * context->maxslave);
   memset(context->grouplist, 0x00, sizeof(Nex_groupt) * context->maxgroup);
   /* clear slave eeprom cache, does not actually read any eeprom */
   Nexx__siigetbyte(context, 0, Nex_MAXEEPBUF);
   for(lp = 0; lp < context->maxgroup; lp++)
   {
      context->grouplist[lp].logstartaddr = lp << 16; /* default start address per group entry */
   }
}

int Nexx__detect_slaves(Nexx__contextt *context)
{
   uint8  b;
   uint16 w;
   int    wkc;

   /* make special pre-init register writes to enable MAC[1] local administered bit *
    * setting for old netX100 slaves */
   b = 0x00;
   Nexx__BWR(context->port, 0x0000, ECT_REG_DLALIAS, sizeof(b), &b, Nex_TIMEOUTRET3);     /* Ignore Alias register */
   b = Nex_STATE_INIT | Nex_STATE_ACK;
   Nexx__BWR(context->port, 0x0000, ECT_REG_ALCTL, sizeof(b), &b, Nex_TIMEOUTRET3);       /* Reset all slaves to Init */
   /* netX100 should now be happy */
   Nexx__BWR(context->port, 0x0000, ECT_REG_ALCTL, sizeof(b), &b, Nex_TIMEOUTRET3);       /* Reset all slaves to Init */
   wkc = Nexx__BRD(context->port, 0x0000, ECT_REG_TYPE, sizeof(w), &w, Nex_TIMEOUTSAFE);  /* detect number of slaves */
   if (wkc > 0)
   {
      /* this is strictly "less than" since the master is "slave 0" */
      if (wkc < Nex_MAXSLAVE)
      {
         *(context->slavecount) = wkc;
      }
      else
      {
         Nex_PRINT("Error: too many slaves on network: num_slaves=%d, Nex_MAXSLAVE=%d\n",
               wkc, Nex_MAXSLAVE);
         return -2;
      }
   }
   return wkc;
}

static void Nexx__set_slaves_to_default(Nexx__contextt *context)
{
   uint8 b;
   uint16 w;
   uint8 zbuf[64];
   memset(&zbuf, 0x00, sizeof(zbuf));
   b = 0x00;
   Nexx__BWR(context->port, 0x0000, ECT_REG_DLPORT      , sizeof(b) , &b, Nex_TIMEOUTRET3);     /* deact loop manual */
   w = htoes(0x0004);
   Nexx__BWR(context->port, 0x0000, ECT_REG_IRQMASK     , sizeof(w) , &w, Nex_TIMEOUTRET3);     /* set IRQ mask */
   Nexx__BWR(context->port, 0x0000, ECT_REG_RXERR       , 8         , &zbuf, Nex_TIMEOUTRET3);  /* reset CRC counters */
   Nexx__BWR(context->port, 0x0000, ECT_REG_FMMU0       , 16 * 3    , &zbuf, Nex_TIMEOUTRET3);  /* reset FMMU's */
   Nexx__BWR(context->port, 0x0000, ECT_REG_SM0         , 8 * 4     , &zbuf, Nex_TIMEOUTRET3);  /* reset SyncM */
   b = 0x00; 
   Nexx__BWR(context->port, 0x0000, ECT_REG_DCSYNCACT   , sizeof(b) , &b, Nex_TIMEOUTRET3);     /* reset activation register */ 
   Nexx__BWR(context->port, 0x0000, ECT_REG_DCSYSTIME   , 4         , &zbuf, Nex_TIMEOUTRET3);  /* reset system time+ofs */
   w = htoes(0x1000);
   Nexx__BWR(context->port, 0x0000, ECT_REG_DCSPEEDCNT  , sizeof(w) , &w, Nex_TIMEOUTRET3);     /* DC speedstart */
   w = htoes(0x0c00);
   Nexx__BWR(context->port, 0x0000, ECT_REG_DCTIMEFILT  , sizeof(w) , &w, Nex_TIMEOUTRET3);     /* DC filt expr */
   b = 0x00;
   Nexx__BWR(context->port, 0x0000, ECT_REG_DLALIAS     , sizeof(b) , &b, Nex_TIMEOUTRET3);     /* Ignore Alias register */
   b = Nex_STATE_INIT | Nex_STATE_ACK;
   Nexx__BWR(context->port, 0x0000, ECT_REG_ALCTL       , sizeof(b) , &b, Nex_TIMEOUTRET3);     /* Reset all slaves to Init */
   b = 2;
   Nexx__BWR(context->port, 0x0000, ECT_REG_EEPCFG      , sizeof(b) , &b, Nex_TIMEOUTRET3);     /* force Eeprom from PDI */
   b = 0;
   Nexx__BWR(context->port, 0x0000, ECT_REG_EEPCFG      , sizeof(b) , &b, Nex_TIMEOUTRET3);     /* set Eeprom to master */
}



/* If slave has SII and same slave ID done before, use previous data.
 * This is safe because SII is constant for same slave ID.
 */
static int Nexx__lookup_prev_sii(Nexx__contextt *context, uint16 slave)
{
   int i, nSM;
   if ((slave > 1) && (*(context->slavecount) > 0))
   {
      i = 1;
      while(((context->slavelist[i].eep_man != context->slavelist[slave].eep_man) ||
             (context->slavelist[i].eep_id  != context->slavelist[slave].eep_id ) ||
             (context->slavelist[i].eep_rev != context->slavelist[slave].eep_rev)) &&
            (i < slave))
      {
         i++;
      }
      if(i < slave)
      {
         context->slavelist[slave].CoEdetails = context->slavelist[i].CoEdetails;
         context->slavelist[slave].FoEdetails = context->slavelist[i].FoEdetails;
         context->slavelist[slave].EoEdetails = context->slavelist[i].EoEdetails;
         context->slavelist[slave].SoEdetails = context->slavelist[i].SoEdetails;
         if(context->slavelist[i].blockLRW > 0)
         {
            context->slavelist[slave].blockLRW = 1;
            context->slavelist[0].blockLRW++;
         }
         context->slavelist[slave].Ebuscurrent = context->slavelist[i].Ebuscurrent;
         context->slavelist[0].Ebuscurrent += context->slavelist[slave].Ebuscurrent;
         memcpy(context->slavelist[slave].name, context->slavelist[i].name, Nex_MAXNAME + 1);
         for( nSM=0 ; nSM < Nex_MAXSM ; nSM++ )
         {
            context->slavelist[slave].SM[nSM].StartAddr = context->slavelist[i].SM[nSM].StartAddr;
            context->slavelist[slave].SM[nSM].SMlength  = context->slavelist[i].SM[nSM].SMlength;
            context->slavelist[slave].SM[nSM].SMflags   = context->slavelist[i].SM[nSM].SMflags;
         }
         context->slavelist[slave].FMMU0func = context->slavelist[i].FMMU0func;
         context->slavelist[slave].FMMU1func = context->slavelist[i].FMMU1func;
         context->slavelist[slave].FMMU2func = context->slavelist[i].FMMU2func;
         context->slavelist[slave].FMMU3func = context->slavelist[i].FMMU3func;
         Nex_PRINT("Copy SII slave %d from %d.\n", slave, i);
         return 1;
      }
   }
   return 0;
}

/** Enumerate and init all slaves.
 *
 * @param[in] context      = context struct
 * @param[in] usetable     = TRUE when using configtable to init slaves, FALSE otherwise
 * @return Workcounter of slave discover datagram = number of slaves found
 */
//int Nexx__config_init(Nexx__contextt *context, uint8 usetable)
int Nexx__config_init(Nexx__contextt *context)
{
   uint16 slave, ADPh, configadr, ssigen;
   uint16 topology, estat;
   int16 topoc, slavec, aliasadr;
   uint8 b,h;
   uint8 SMc;
   uint32 eedat;
   int wkc, cindex, nSM;

//   Nex_PRINT("Nex_config_init %d\n",usetable);
   Nexx__init_context(context);
   wkc = Nexx__detect_slaves(context);
   if (wkc > 0)
   {
      Nexx__set_slaves_to_default(context);
      for (slave = 1; slave <= *(context->slavecount); slave++)
      {
         ADPh = (uint16)(1 - slave);
         context->slavelist[slave].Itype =
            etohs(Nexx__APRDw(context->port, ADPh, ECT_REG_PDICTL, Nex_TIMEOUTRET3)); /* read interface type of slave */
         /* a node offset is used to improve readibility of network frames */
         /* this has no impact on the number of addressable slaves (auto wrap around) */
         Nexx__APWRw(context->port, ADPh, ECT_REG_STADR, htoes(slave + Nex_NODEOFFSET) , Nex_TIMEOUTRET3); /* set node address of slave */
         if (slave == 1)
         {
            b = 1; /* kill non ecat frames for first slave */
         }
         else
         {
            b = 0; /* pass all frames for following slaves */
         }
         Nexx__APWRw(context->port, ADPh, ECT_REG_DLCTL, htoes(b), Nex_TIMEOUTRET3); /* set non ecat frame behaviour */
         configadr = etohs(Nexx__APRDw(context->port, ADPh, ECT_REG_STADR, Nex_TIMEOUTRET3));
         context->slavelist[slave].configadr = configadr;
         Nexx__FPRD(context->port, configadr, ECT_REG_ALIAS, sizeof(aliasadr), &aliasadr, Nex_TIMEOUTRET3);
         context->slavelist[slave].aliasadr = etohs(aliasadr);
         Nexx__FPRD(context->port, configadr, ECT_REG_EEPSTAT, sizeof(estat), &estat, Nex_TIMEOUTRET3);
         estat = etohs(estat);
         if (estat & Nex_ESTAT_R64) /* check if slave can read 8 byte chunks */
         {
            context->slavelist[slave].eep_8byte = 1;
         }
         Nexx__readeeprom1(context, slave, ECT_SII_MANUF); /* Manuf */
      }
      for (slave = 1; slave <= *(context->slavecount); slave++)
      {
         context->slavelist[slave].eep_man =
            etohl(Nexx__readeeprom2(context, slave, Nex_TIMEOUTEEP)); /* Manuf */
         Nexx__readeeprom1(context, slave, ECT_SII_ID); /* ID */
      }
      for (slave = 1; slave <= *(context->slavecount); slave++)
      {
         context->slavelist[slave].eep_id =
            etohl(Nexx__readeeprom2(context, slave, Nex_TIMEOUTEEP)); /* ID */
         Nexx__readeeprom1(context, slave, ECT_SII_REV); /* revision */
      }
      for (slave = 1; slave <= *(context->slavecount); slave++)
      {
         context->slavelist[slave].eep_rev =
            etohl(Nexx__readeeprom2(context, slave, Nex_TIMEOUTEEP)); /* revision */
         Nexx__readeeprom1(context, slave, ECT_SII_RXMBXADR); /* write mailbox address + mailboxsize */
      }
      for (slave = 1; slave <= *(context->slavecount); slave++)
      {
         eedat = etohl(Nexx__readeeprom2(context, slave, Nex_TIMEOUTEEP)); /* write mailbox address and mailboxsize */
         context->slavelist[slave].mbx_wo = (uint16)LO_WORD(eedat);
         context->slavelist[slave].mbx_l = (uint16)HI_WORD(eedat);
         if (context->slavelist[slave].mbx_l > 0)
         {
            Nexx__readeeprom1(context, slave, ECT_SII_TXMBXADR); /* read mailbox offset */
         }
      }
      for (slave = 1; slave <= *(context->slavecount); slave++)
      {
         if (context->slavelist[slave].mbx_l > 0)
         {
            eedat = etohl(Nexx__readeeprom2(context, slave, Nex_TIMEOUTEEP)); /* read mailbox offset */
            context->slavelist[slave].mbx_ro = (uint16)LO_WORD(eedat); /* read mailbox offset */
            context->slavelist[slave].mbx_rl = (uint16)HI_WORD(eedat); /*read mailbox length */
            if (context->slavelist[slave].mbx_rl == 0)
            {
               context->slavelist[slave].mbx_rl = context->slavelist[slave].mbx_l;
            }
            Nexx__readeeprom1(context, slave, ECT_SII_MBXPROTO);
         }
         configadr = context->slavelist[slave].configadr;
         if ((etohs(Nexx__FPRDw(context->port, configadr, ECT_REG_ESCSUP, Nex_TIMEOUTRET3)) & 0x04) > 0)  /* Support DC? */
         {
            context->slavelist[slave].hasdc = TRUE;
         }
         else
         {
            context->slavelist[slave].hasdc = FALSE;
         }
         topology = etohs(Nexx__FPRDw(context->port, configadr, ECT_REG_DLSTAT, Nex_TIMEOUTRET3)); /* extract topology from DL status */
         h = 0;
         b = 0;
         if ((topology & 0x0300) == 0x0200) /* port0 open and communication established */
         {
            h++;
            b |= 0x01;
         }
         if ((topology & 0x0c00) == 0x0800) /* port1 open and communication established */
         {
            h++;
            b |= 0x02;
         }
         if ((topology & 0x3000) == 0x2000) /* port2 open and communication established */
         {
            h++;
            b |= 0x04;
         }
         if ((topology & 0xc000) == 0x8000) /* port3 open and communication established */
         {
            h++;
            b |= 0x08;
         }
         /* ptype = Physical type*/
         context->slavelist[slave].ptype =
            LO_BYTE(etohs(Nexx__FPRDw(context->port, configadr, ECT_REG_PORTDES, Nex_TIMEOUTRET3)));
         context->slavelist[slave].topology = h;
         context->slavelist[slave].activeports = b;
         /* 0=no links, not possible             */
         /* 1=1 link  , end of line              */
         /* 2=2 links , one before and one after */
         /* 3=3 links , split point              */
         /* 4=4 links , cross point              */
         /* search for parent */
         context->slavelist[slave].parent = 0; /* parent is master */
         if (slave > 1)
         {
            topoc = 0;
            slavec = slave - 1;
            do
            {
               topology = context->slavelist[slavec].topology;
               if (topology == 1)
               {
                  topoc--; /* endpoint found */
               }
               if (topology == 3)
               {
                  topoc++; /* split found */
               }
               if (topology == 4)
               {
                  topoc += 2; /* cross found */
               }
               if (((topoc >= 0) && (topology > 1)) ||
                   (slavec == 1)) /* parent found */
               {
                  context->slavelist[slave].parent = slavec;
                  slavec = 1;
               }
               slavec--;
            }
            while (slavec > 0);
         }
         (void)Nexx__statecheck(context, slave, Nex_STATE_INIT,  Nex_TIMEOUTSTATE); //* check state change Init */

         /* set default mailbox configuration if slave has mailbox */
         if (context->slavelist[slave].mbx_l>0)
         {
            context->slavelist[slave].SMtype[0] = 1;
            context->slavelist[slave].SMtype[1] = 2;
            context->slavelist[slave].SMtype[2] = 3;
            context->slavelist[slave].SMtype[3] = 4;
            context->slavelist[slave].SM[0].StartAddr = htoes(context->slavelist[slave].mbx_wo);
            context->slavelist[slave].SM[0].SMlength = htoes(context->slavelist[slave].mbx_l);
            context->slavelist[slave].SM[0].SMflags = htoel(Nex_DEFAULTMBXSM0);
            context->slavelist[slave].SM[1].StartAddr = htoes(context->slavelist[slave].mbx_ro);
            context->slavelist[slave].SM[1].SMlength = htoes(context->slavelist[slave].mbx_rl);
            context->slavelist[slave].SM[1].SMflags = htoel(Nex_DEFAULTMBXSM1);
            context->slavelist[slave].mbx_proto =
               Nexx__readeeprom2(context, slave, Nex_TIMEOUTEEP);
         }
         cindex = 0;
         /* use configuration table ? */
 //        if (usetable == 1)
  //       {
 //           cindex = Nexx__config_from_table(context, slave);
 //        }
         /* slave not in configuration table, find out via SII */
         if (!cindex && !Nexx__lookup_prev_sii(context, slave))
         {
            ssigen = Nexx__siifind(context, slave, ECT_SII_GENERAL);
            /* SII general section */
            if (ssigen)
            {
               context->slavelist[slave].CoEdetails = Nexx__siigetbyte(context, slave, ssigen + 0x07);
               context->slavelist[slave].FoEdetails = Nexx__siigetbyte(context, slave, ssigen + 0x08);
               context->slavelist[slave].EoEdetails = Nexx__siigetbyte(context, slave, ssigen + 0x09);
               context->slavelist[slave].SoEdetails = Nexx__siigetbyte(context, slave, ssigen + 0x0a);
               if((Nexx__siigetbyte(context, slave, ssigen + 0x0d) & 0x02) > 0)
               {
                  context->slavelist[slave].blockLRW = 1;
                  context->slavelist[0].blockLRW++;
               }
               context->slavelist[slave].Ebuscurrent = Nexx__siigetbyte(context, slave, ssigen + 0x0e);
               context->slavelist[slave].Ebuscurrent += Nexx__siigetbyte(context, slave, ssigen + 0x0f) << 8;
               context->slavelist[0].Ebuscurrent += context->slavelist[slave].Ebuscurrent;
            }
            /* SII strings section */
            if (Nexx__siifind(context, slave, ECT_SII_STRING) > 0)
            {
               Nexx__siistring(context, context->slavelist[slave].name, slave, 1);
            }
            /* no name for slave found, use constructed name */
            else
            {
               sprintf(context->slavelist[slave].name, "? M:%8.8x I:%8.8x",
                       (unsigned int)context->slavelist[slave].eep_man,
                       (unsigned int)context->slavelist[slave].eep_id);
            }
            /* SII SM section */
            nSM = Nexx__siiSM(context, slave, context->eepSM);
            if (nSM>0)
            {
               context->slavelist[slave].SM[0].StartAddr = htoes(context->eepSM->PhStart);
               context->slavelist[slave].SM[0].SMlength = htoes(context->eepSM->Plength);
               context->slavelist[slave].SM[0].SMflags =
                  htoel((context->eepSM->Creg) + (context->eepSM->Activate << 16));
               SMc = 1;
               while ((SMc < Nex_MAXSM) &&  Nexx__siiSMnext(context, slave, context->eepSM, SMc))
               {
                  context->slavelist[slave].SM[SMc].StartAddr = htoes(context->eepSM->PhStart);
                  context->slavelist[slave].SM[SMc].SMlength = htoes(context->eepSM->Plength);
                  context->slavelist[slave].SM[SMc].SMflags =
                     htoel((context->eepSM->Creg) + (context->eepSM->Activate << 16));
                  SMc++;
               }
            }
            /* SII FMMU section */
            if (Nexx__siiFMMU(context, slave, context->eepFMMU))
            {
               if (context->eepFMMU->FMMU0 !=0xff)
               {
                  context->slavelist[slave].FMMU0func = context->eepFMMU->FMMU0;
               }
               if (context->eepFMMU->FMMU1 !=0xff)
               {
                  context->slavelist[slave].FMMU1func = context->eepFMMU->FMMU1;
               }
               if (context->eepFMMU->FMMU2 !=0xff)
               {
                  context->slavelist[slave].FMMU2func = context->eepFMMU->FMMU2;
               }
               if (context->eepFMMU->FMMU3 !=0xff)
               {
                  context->slavelist[slave].FMMU3func = context->eepFMMU->FMMU3;
               }
            }
         }

         if (context->slavelist[slave].mbx_l > 0)
         {
            if (context->slavelist[slave].SM[0].StartAddr == 0x0000) /* should never happen */
            {
               Nex_PRINT("Slave %d has no proper mailbox in configuration, try default.\n", slave);
               context->slavelist[slave].SM[0].StartAddr = htoes(0x1000);
               context->slavelist[slave].SM[0].SMlength = htoes(0x0080);
               context->slavelist[slave].SM[0].SMflags = htoel(Nex_DEFAULTMBXSM0);
               context->slavelist[slave].SMtype[0] = 1;
            }
            if (context->slavelist[slave].SM[1].StartAddr == 0x0000) /* should never happen */
            {
               Nex_PRINT("Slave %d has no proper mailbox out configuration, try default.\n", slave);
               context->slavelist[slave].SM[1].StartAddr = htoes(0x1080);
               context->slavelist[slave].SM[1].SMlength = htoes(0x0080);
               context->slavelist[slave].SM[1].SMflags = htoel(Nex_DEFAULTMBXSM1);
               context->slavelist[slave].SMtype[1] = 2;
            }
            /* program SM0 mailbox in and SM1 mailbox out for slave */
            /* writing both SM in one datagram will solve timing issue in old NETX */
            Nexx__FPWR(context->port, configadr, ECT_REG_SM0, sizeof(Nex_smt) * 2,
               &(context->slavelist[slave].SM[0]), Nex_TIMEOUTRET3);
         }
         /* some slaves need eeprom available to PDI in init->preop transition */
         Nexx__eeprom2pdi(context, slave);
         /* request pre_op for slave */
         Nexx__FPWRw(context->port, configadr, ECT_REG_ALCTL, htoes(Nex_STATE_PRE_OP | Nex_STATE_ACK) , Nex_TIMEOUTRET3); /* set preop status */
      }
   }
   return wkc;
}

/* If slave has SII mapping and same slave ID done before, use previous mapping.
 * This is safe because SII mapping is constant for same slave ID.
 */
static int Nexx__lookup_mapping(Nexx__contextt *context, uint16 slave, int *Osize, int *Isize)
{
   int i, nSM;
   if ((slave > 1) && (*(context->slavecount) > 0))
   {
      i = 1;
      while(((context->slavelist[i].eep_man != context->slavelist[slave].eep_man) ||
             (context->slavelist[i].eep_id  != context->slavelist[slave].eep_id ) ||
             (context->slavelist[i].eep_rev != context->slavelist[slave].eep_rev)) &&
            (i < slave))
      {
         i++;
      }
      if(i < slave)
      {
         for( nSM=0 ; nSM < Nex_MAXSM ; nSM++ )
         {
            context->slavelist[slave].SM[nSM].SMlength = context->slavelist[i].SM[nSM].SMlength;
            context->slavelist[slave].SMtype[nSM] = context->slavelist[i].SMtype[nSM];
         }
         *Osize = context->slavelist[i].Obits;
         *Isize = context->slavelist[i].Ibits;
         context->slavelist[slave].Obits = *Osize;
         context->slavelist[slave].Ibits = *Isize;
         Nex_PRINT("Copy mapping slave %d from %d.\n", slave, i);
         return 1;
      }
   }
   return 0;
}

static int Nexx__map_coe_soe(Nexx__contextt *context, uint16 slave, int thread_n)
{
   int Isize, Osize;
   int rval;

   Nexx__statecheck(context, slave, Nex_STATE_PRE_OP, Nex_TIMEOUTSTATE); /* check state change pre-op */

   Nex_PRINT(" >Slave %d, configadr %x, state %2.2x\n",
            slave, context->slavelist[slave].configadr, context->slavelist[slave].state);

   /* execute special slave configuration hook Pre-Op to Safe-OP */
   if(context->slavelist[slave].PO2SOconfig) /* only if registered */
   {
      context->slavelist[slave].PO2SOconfig(slave);
   }
   /* if slave not found in configlist find IO mapping in slave self */
   if (!context->slavelist[slave].configindex)
   {
      Isize = 0;
      Osize = 0;
      if (context->slavelist[slave].mbx_proto & ECT_MBXPROT_COE) /* has CoE */
      {
         rval = 0;
         if (context->slavelist[slave].CoEdetails & ECT_COEDET_SDOCA) /* has Complete Access */
         {
            /* read PDO mapping via CoE and use Complete Access */
            rval = Nexx__readPDOmapCA(context, slave, thread_n, &Osize, &Isize);
         }
         if (!rval) /* CA not available or not succeeded */
         {
            /* read PDO mapping via CoE */
            rval = Nexx__readPDOmap(context, slave, &Osize, &Isize);
         }
         Nex_PRINT("  CoE Osize:%d Isize:%d\n", Osize, Isize);
      }
      if ((!Isize && !Osize) && (context->slavelist[slave].mbx_proto & ECT_MBXPROT_SOE)) /* has SoE */
      {
         /* read AT / MDT mapping via SoE */
         rval = Nexx__readIDNmap(context, slave, &Osize, &Isize);
         context->slavelist[slave].SM[2].SMlength = htoes((Osize + 7) / 8);
         context->slavelist[slave].SM[3].SMlength = htoes((Isize + 7) / 8);
         Nex_PRINT("  SoE Osize:%d Isize:%d\n", Osize, Isize);
      }
      context->slavelist[slave].Obits = Osize;
      context->slavelist[slave].Ibits = Isize;
   }

   return 1;
}

static int Nexx__map_sii(Nexx__contextt *context, uint16 slave)
{
   int Isize, Osize;
   int nSM;
   Nex_eepromPDOt eepPDO;

   Osize = context->slavelist[slave].Obits;
   Isize = context->slavelist[slave].Ibits;

   if (!Isize && !Osize) /* find PDO in previous slave with same ID */
   {
      (void)Nexx__lookup_mapping(context, slave, &Osize, &Isize);
   }
   if (!Isize && !Osize) /* find PDO mapping by SII */
   {
      memset(&eepPDO, 0, sizeof(eepPDO));
      Isize = (int)Nexx__siiPDO(context, slave, &eepPDO, 0);
      Nex_PRINT("  SII Isize:%d\n", Isize);
      for( nSM=0 ; nSM < Nex_MAXSM ; nSM++ )
      {
         if (eepPDO.SMbitsize[nSM] > 0)
         {
            context->slavelist[slave].SM[nSM].SMlength =  htoes((eepPDO.SMbitsize[nSM] + 7) / 8);
            context->slavelist[slave].SMtype[nSM] = 4;
            Nex_PRINT("    SM%d length %d\n", nSM, eepPDO.SMbitsize[nSM]);
         }
      }
      Osize = (int)Nexx__siiPDO(context, slave, &eepPDO, 1);
      Nex_PRINT("  SII Osize:%d\n", Osize);
      for( nSM=0 ; nSM < Nex_MAXSM ; nSM++ )
      {
         if (eepPDO.SMbitsize[nSM] > 0)
         {
            context->slavelist[slave].SM[nSM].SMlength =  htoes((eepPDO.SMbitsize[nSM] + 7) / 8);
            context->slavelist[slave].SMtype[nSM] = 3;
            Nex_PRINT("    SM%d length %d\n", nSM, eepPDO.SMbitsize[nSM]);
         }
      }
   }
   context->slavelist[slave].Obits = Osize;
   context->slavelist[slave].Ibits = Isize;
   Nex_PRINT("     ISIZE:%d %d OSIZE:%d\n",
      context->slavelist[slave].Ibits, Isize,context->slavelist[slave].Obits);

   return 1;
}

static int Nexx__map_sm(Nexx__contextt *context, uint16 slave)
{
   uint16 configadr;
   int nSM;

   configadr = context->slavelist[slave].configadr;

   Nex_PRINT("  SM programming\n");
   if (!context->slavelist[slave].mbx_l && context->slavelist[slave].SM[0].StartAddr)
   {
      Nexx__FPWR(context->port, configadr, ECT_REG_SM0,
         sizeof(Nex_smt), &(context->slavelist[slave].SM[0]), Nex_TIMEOUTRET3);
      Nex_PRINT("    SM0 Type:%d StartAddr:%4.4x Flags:%8.8x\n",
          context->slavelist[slave].SMtype[0],
          context->slavelist[slave].SM[0].StartAddr,
          context->slavelist[slave].SM[0].SMflags);
   }
   if (!context->slavelist[slave].mbx_l && context->slavelist[slave].SM[1].StartAddr)
   {
      Nexx__FPWR(context->port, configadr, ECT_REG_SM1,
         sizeof(Nex_smt), &context->slavelist[slave].SM[1], Nex_TIMEOUTRET3);
      Nex_PRINT("    SM1 Type:%d StartAddr:%4.4x Flags:%8.8x\n",
          context->slavelist[slave].SMtype[1],
          context->slavelist[slave].SM[1].StartAddr,
          context->slavelist[slave].SM[1].SMflags);
   }
   /* program SM2 to SMx */
   for( nSM = 2 ; nSM < Nex_MAXSM ; nSM++ )
   {
      if (context->slavelist[slave].SM[nSM].StartAddr)
      {
         /* check if SM length is zero -> clear enable flag */
         if( context->slavelist[slave].SM[nSM].SMlength == 0)
         {
            context->slavelist[slave].SM[nSM].SMflags =
               htoel( etohl(context->slavelist[slave].SM[nSM].SMflags) & Nex_SMENABLEMASK);
         }
         Nexx__FPWR(context->port, configadr, (uint16)(ECT_REG_SM0 + (nSM * sizeof(Nex_smt))),
            sizeof(Nex_smt), &context->slavelist[slave].SM[nSM], Nex_TIMEOUTRET3);
         Nex_PRINT("    SM%d Type:%d StartAddr:%4.4x Flags:%8.8x\n", nSM,
             context->slavelist[slave].SMtype[nSM],
             context->slavelist[slave].SM[nSM].StartAddr,
             context->slavelist[slave].SM[nSM].SMflags);
      }
   }
   if (context->slavelist[slave].Ibits > 7)
   {
      context->slavelist[slave].Ibytes = (context->slavelist[slave].Ibits + 7) / 8;
   }
   if (context->slavelist[slave].Obits > 7)
   {
      context->slavelist[slave].Obytes = (context->slavelist[slave].Obits + 7) / 8;
   }

   return 1;
}

OSAL_THREAD_FUNC Nexx__mapper_thread(void *param)
{
   Nexx__mapt_t *maptp;
   maptp = param;
   Nexx__map_coe_soe(maptp->context, maptp->slave, maptp->thread_n);
   maptp->running = 0;
}

static int Nexx__find_mapt(void)
{
   int p;
   p = 0;
   while((p < Nex_MAX_MAPT) && Nexx__mapt[p].running)
   {
      p++;
   }
   if(p < Nex_MAX_MAPT)
   {
      return p;
   }
   else
   {
      return -1;
   }
}

static int Nexx__get_threadcount(void)
{
   int thrc, thrn;
   thrc = 0;
   for(thrn = 0 ; thrn < Nex_MAX_MAPT ; thrn++)
   {
      thrc += Nexx__mapt[thrn].running;
   }
   return thrc;
}

static void Nexx__config_find_mappings(Nexx__contextt *context, uint8 group)
{
   int thrn, thrc;
   uint16 slave;

   for (thrn = 0; thrn < Nex_MAX_MAPT; thrn++)
   {
      Nexx__mapt[thrn].running = 0;
   }
   /* find CoE and SoE mapping of slaves in multiple threads */
   for (slave = 1; slave <= *(context->slavecount); slave++)
   {
      if (!group || (group == context->slavelist[slave].group))
      {
         if (Nex_MAX_MAPT <= 1)
         {
            /* serialised version */
            Nexx__map_coe_soe(context, slave, 0);
         }
         else
         {
            /* multi-threaded version */
            while ((thrn = Nexx__find_mapt()) < 0)
            {
               osal_usleep(1000);
            }
            Nexx__mapt[thrn].context = context;
            Nexx__mapt[thrn].slave = slave;
            Nexx__mapt[thrn].thread_n = thrn;
            Nexx__mapt[thrn].running = 1;
            osal_thread_create(&(Nexx__threadh[thrn]), 128000,
               &Nexx__mapper_thread, &(Nexx__mapt[thrn]));
         }
      }
   }
   /* wait for all threads to finish */
   do
   {
      thrc = Nexx__get_threadcount();
      if (thrc)
      {
         osal_usleep(1000);
      }
   } while (thrc);
   /* find SII mapping of slave and program SM */
   for (slave = 1; slave <= *(context->slavecount); slave++)
   {
      if (!group || (group == context->slavelist[slave].group))
      {
         Nexx__map_sii(context, slave);
         Nexx__map_sm(context, slave);
      }
   }
}

static void Nexx__config_create_input_mappings(Nexx__contextt *context, void *pIOmap, 
   uint8 group, int16 slave, uint32 * LogAddr, uint8 * BitPos)
{
   int BitCount = 0;
   int ByteCount = 0;
   int FMMUsize = 0;
   int FMMUdone = 0;
   uint8 SMc = 0;
   uint16 EndAddr;
   uint16 SMlength;
   uint16 configadr;
   uint8 FMMUc;

   Nex_PRINT(" =Slave %d, INPUT MAPPING\n", slave);

   configadr = context->slavelist[slave].configadr;
   FMMUc = context->slavelist[slave].FMMUunused;
   if (context->slavelist[slave].Obits) /* find free FMMU */
   {
      while (context->slavelist[slave].FMMU[FMMUc].LogStart)
      {
         FMMUc++;
      }
   }
   /* search for SM that contribute to the input mapping */
   while ((SMc < (Nex_MAXSM - 1)) && (FMMUdone < ((context->slavelist[slave].Ibits + 7) / 8)))
   {
      Nex_PRINT("    FMMU %d\n", FMMUc);
      while ((SMc < (Nex_MAXSM - 1)) && (context->slavelist[slave].SMtype[SMc] != 4))
      {
         SMc++;
      }
      Nex_PRINT("      SM%d\n", SMc);
      context->slavelist[slave].FMMU[FMMUc].PhysStart =
         context->slavelist[slave].SM[SMc].StartAddr;
      SMlength = etohs(context->slavelist[slave].SM[SMc].SMlength);
      ByteCount += SMlength;
      BitCount += SMlength * 8;
      EndAddr = etohs(context->slavelist[slave].SM[SMc].StartAddr) + SMlength;
      while ((BitCount < context->slavelist[slave].Ibits) && (SMc < (Nex_MAXSM - 1))) /* more SM for input */
      {
         SMc++;
         while ((SMc < (Nex_MAXSM - 1)) && (context->slavelist[slave].SMtype[SMc] != 4))
         {
            SMc++;
         }
         /* if addresses from more SM connect use one FMMU otherwise break up in mutiple FMMU */
         if (etohs(context->slavelist[slave].SM[SMc].StartAddr) > EndAddr)
         {
            break;
         }
         Nex_PRINT("      SM%d\n", SMc);
         SMlength = etohs(context->slavelist[slave].SM[SMc].SMlength);
         ByteCount += SMlength;
         BitCount += SMlength * 8;
         EndAddr = etohs(context->slavelist[slave].SM[SMc].StartAddr) + SMlength;
      }

      /* bit oriented slave */
      if (!context->slavelist[slave].Ibytes)
      {
         context->slavelist[slave].FMMU[FMMUc].LogStart = htoel(*LogAddr);
         context->slavelist[slave].FMMU[FMMUc].LogStartbit = *BitPos;
         *BitPos += context->slavelist[slave].Ibits - 1;
         if (*BitPos > 7)
         {
            *LogAddr += 1;
            *BitPos -= 8;
         }
         FMMUsize = *LogAddr - etohl(context->slavelist[slave].FMMU[FMMUc].LogStart) + 1;
         context->slavelist[slave].FMMU[FMMUc].LogLength = htoes(FMMUsize);
         context->slavelist[slave].FMMU[FMMUc].LogEndbit = *BitPos;
         *BitPos += 1;
         if (*BitPos > 7)
         {
            *LogAddr += 1;
            *BitPos -= 8;
         }
      }
      /* byte oriented slave */
      else
      {
         if (*BitPos)
         {
            *LogAddr += 1;
            *BitPos = 0;
         }
         context->slavelist[slave].FMMU[FMMUc].LogStart = htoel(*LogAddr);
         context->slavelist[slave].FMMU[FMMUc].LogStartbit = *BitPos;
         *BitPos = 7;
         FMMUsize = ByteCount;
         if ((FMMUsize + FMMUdone)> (int)context->slavelist[slave].Ibytes)
         {
            FMMUsize = context->slavelist[slave].Ibytes - FMMUdone;
         }
         *LogAddr += FMMUsize;
         context->slavelist[slave].FMMU[FMMUc].LogLength = htoes(FMMUsize);
         context->slavelist[slave].FMMU[FMMUc].LogEndbit = *BitPos;
         *BitPos = 0;
      }
      FMMUdone += FMMUsize;
      if (context->slavelist[slave].FMMU[FMMUc].LogLength)
      {
         context->slavelist[slave].FMMU[FMMUc].PhysStartBit = 0;
         context->slavelist[slave].FMMU[FMMUc].FMMUtype = 1;
         context->slavelist[slave].FMMU[FMMUc].FMMUactive = 1;
         /* program FMMU for input */
         Nexx__FPWR(context->port, configadr, ECT_REG_FMMU0 + (sizeof(Nex_fmmut) * FMMUc),
            sizeof(Nex_fmmut), &(context->slavelist[slave].FMMU[FMMUc]), Nex_TIMEOUTRET3);
         /* add one for an input FMMU */
         context->grouplist[group].inputsWKC++;
      }
      if (!context->slavelist[slave].inputs)
      {
         context->slavelist[slave].inputs =
            (uint8 *)(pIOmap)+etohl(context->slavelist[slave].FMMU[FMMUc].LogStart);
         context->slavelist[slave].Istartbit =
            context->slavelist[slave].FMMU[FMMUc].LogStartbit;
         Nex_PRINT("    Inputs %p startbit %d\n",
            context->slavelist[slave].inputs,
            context->slavelist[slave].Istartbit);
      }
      FMMUc++;
   }
   context->slavelist[slave].FMMUunused = FMMUc;
}

static void Nexx__config_create_output_mappings(Nexx__contextt *context, void *pIOmap, 
   uint8 group, int16 slave, uint32 * LogAddr, uint8 * BitPos)
{
   int BitCount = 0;
   int ByteCount = 0;
   int FMMUsize = 0;
   int FMMUdone = 0;
   uint8 SMc = 0;
   uint16 EndAddr;
   uint16 SMlength;
   uint16 configadr;
   uint8 FMMUc;

   Nex_PRINT("  OUTPUT MAPPING\n");

   FMMUc = context->slavelist[slave].FMMUunused;
   configadr = context->slavelist[slave].configadr;

   /* search for SM that contribute to the output mapping */
   while ((SMc < (Nex_MAXSM - 1)) && (FMMUdone < ((context->slavelist[slave].Obits + 7) / 8)))
   {
      Nex_PRINT("    FMMU %d\n", FMMUc);
      while ((SMc < (Nex_MAXSM - 1)) && (context->slavelist[slave].SMtype[SMc] != 3))
      {
         SMc++;
      }
      Nex_PRINT("      SM%d\n", SMc);
      context->slavelist[slave].FMMU[FMMUc].PhysStart =
         context->slavelist[slave].SM[SMc].StartAddr;
      SMlength = etohs(context->slavelist[slave].SM[SMc].SMlength);
      ByteCount += SMlength;
      BitCount += SMlength * 8;
      EndAddr = etohs(context->slavelist[slave].SM[SMc].StartAddr) + SMlength;
      while ((BitCount < context->slavelist[slave].Obits) && (SMc < (Nex_MAXSM - 1))) /* more SM for output */
      {
         SMc++;
         while ((SMc < (Nex_MAXSM - 1)) && (context->slavelist[slave].SMtype[SMc] != 3))
         {
            SMc++;
         }
         /* if addresses from more SM connect use one FMMU otherwise break up in mutiple FMMU */
         if (etohs(context->slavelist[slave].SM[SMc].StartAddr) > EndAddr)
         {
            break;
         }
         Nex_PRINT("      SM%d\n", SMc);
         SMlength = etohs(context->slavelist[slave].SM[SMc].SMlength);
         ByteCount += SMlength;
         BitCount += SMlength * 8;
         EndAddr = etohs(context->slavelist[slave].SM[SMc].StartAddr) + SMlength;
      }

      /* bit oriented slave */
      if (!context->slavelist[slave].Obytes)
      {
         context->slavelist[slave].FMMU[FMMUc].LogStart = htoel(*LogAddr);
         context->slavelist[slave].FMMU[FMMUc].LogStartbit = *BitPos;
         *BitPos += context->slavelist[slave].Obits - 1;
         if (*BitPos > 7)
         {
            *LogAddr += 1;
            *BitPos -= 8;
         }
         FMMUsize = *LogAddr - etohl(context->slavelist[slave].FMMU[FMMUc].LogStart) + 1;
         context->slavelist[slave].FMMU[FMMUc].LogLength = htoes(FMMUsize);
         context->slavelist[slave].FMMU[FMMUc].LogEndbit = *BitPos;
         *BitPos += 1;
         if (*BitPos > 7)
         {
            *LogAddr += 1;
            *BitPos -= 8;
         }
      }
      /* byte oriented slave */
      else
      {
         if (*BitPos)
         {
            *LogAddr += 1;
            *BitPos = 0;
         }
         context->slavelist[slave].FMMU[FMMUc].LogStart = htoel(*LogAddr);
         context->slavelist[slave].FMMU[FMMUc].LogStartbit = *BitPos;
         *BitPos = 7;
         FMMUsize = ByteCount;
         if ((FMMUsize + FMMUdone)> (int)context->slavelist[slave].Obytes)
         {
            FMMUsize = context->slavelist[slave].Obytes - FMMUdone;
         }
         *LogAddr += FMMUsize;
         context->slavelist[slave].FMMU[FMMUc].LogLength = htoes(FMMUsize);
         context->slavelist[slave].FMMU[FMMUc].LogEndbit = *BitPos;
         *BitPos = 0;
      }
      FMMUdone += FMMUsize;
      context->slavelist[slave].FMMU[FMMUc].PhysStartBit = 0;
      context->slavelist[slave].FMMU[FMMUc].FMMUtype = 2;
      context->slavelist[slave].FMMU[FMMUc].FMMUactive = 1;
      /* program FMMU for output */
      Nexx__FPWR(context->port, configadr, ECT_REG_FMMU0 + (sizeof(Nex_fmmut) * FMMUc),
         sizeof(Nex_fmmut), &(context->slavelist[slave].FMMU[FMMUc]), Nex_TIMEOUTRET3);
      context->grouplist[group].outputsWKC++;
      if (!context->slavelist[slave].outputs)
      {
         context->slavelist[slave].outputs =
            (uint8 *)(pIOmap)+etohl(context->slavelist[slave].FMMU[FMMUc].LogStart);
         context->slavelist[slave].Ostartbit =
            context->slavelist[slave].FMMU[FMMUc].LogStartbit;
         Nex_PRINT("    slave %d Outputs %p startbit %d\n",
            slave,
            context->slavelist[slave].outputs,
            context->slavelist[slave].Ostartbit);
      }
      FMMUc++;
   }
   context->slavelist[slave].FMMUunused = FMMUc;
}

/** Map all PDOs in one group of slaves to IOmap with Outputs/Inputs
* in sequential order (legacy SOEM way).
*
 *
 * @param[in]  context    = context struct
 * @param[out] pIOmap     = pointer to IOmap
 * @param[in]  group      = group to map, 0 = all groups
 * @return IOmap size
 */
int Nexx__config_map_group(Nexx__contextt *context, void *pIOmap, uint8 group)
{
   uint16 slave, configadr;
   uint8 BitPos;
   uint32 LogAddr = 0;
   uint32 oLogAddr = 0;
   uint32 diff;
   uint16 currentsegment = 0;
   uint32 segmentsize = 0;

   if ((*(context->slavecount) > 0) && (group < context->maxgroup))
   {
      Nex_PRINT("Nex_config_map_group IOmap:%p group:%d\n", pIOmap, group);
      LogAddr = context->grouplist[group].logstartaddr;
      oLogAddr = LogAddr;
      BitPos = 0;
      context->grouplist[group].nsegments = 0;
      context->grouplist[group].outputsWKC = 0;
      context->grouplist[group].inputsWKC = 0;

      /* Find mappings and program syncmanagers */
      Nexx__config_find_mappings(context, group);

      /* do output mapping of slave and program FMMUs */
      for (slave = 1; slave <= *(context->slavecount); slave++)
      {
         configadr = context->slavelist[slave].configadr;

         if (!group || (group == context->slavelist[slave].group))
         {
            /* create output mapping */
            if (context->slavelist[slave].Obits)
            {
               Nexx__config_create_output_mappings (context, pIOmap, group, slave, &LogAddr, &BitPos);
               diff = LogAddr - oLogAddr;
               oLogAddr = LogAddr;
               if ((segmentsize + diff) > (Nex_MAXLRWDATA - Nex_FIRSTDCDATAGRAM))
               {
                  context->grouplist[group].IOsegment[currentsegment] = segmentsize;
                  if (currentsegment < (Nex_MAXIOSEGMENTS - 1))
                  {
                     currentsegment++;
                     segmentsize = diff;
                  }
               }
               else
               {
                  segmentsize += diff;
               }
            }
         }
      }
      if (BitPos)
      {
         LogAddr++;
         oLogAddr = LogAddr;
         BitPos = 0;
         if ((segmentsize + 1) > (Nex_MAXLRWDATA - Nex_FIRSTDCDATAGRAM))
         {
            context->grouplist[group].IOsegment[currentsegment] = segmentsize;
            if (currentsegment < (Nex_MAXIOSEGMENTS - 1))
            {
               currentsegment++;
               segmentsize = 1;
            }
         }
         else
         {
            segmentsize += 1;
         }
      }
      context->grouplist[group].outputs = pIOmap;
      context->grouplist[group].Obytes = LogAddr;
      context->grouplist[group].nsegments = currentsegment + 1;
      context->grouplist[group].Isegment = currentsegment;
      context->grouplist[group].Ioffset = segmentsize;
      if (!group)
      {
         context->slavelist[0].outputs = pIOmap;
         context->slavelist[0].Obytes = LogAddr; /* store output bytes in master record */
      }

      /* do input mapping of slave and program FMMUs */
      for (slave = 1; slave <= *(context->slavecount); slave++)
      {
         configadr = context->slavelist[slave].configadr;
         if (!group || (group == context->slavelist[slave].group))
         {
            /* create input mapping */
            if (context->slavelist[slave].Ibits)
            {
 
               Nexx__config_create_input_mappings(context, pIOmap, group, slave, &LogAddr, &BitPos);
               diff = LogAddr - oLogAddr;
               oLogAddr = LogAddr;
               if ((segmentsize + diff) > (Nex_MAXLRWDATA - Nex_FIRSTDCDATAGRAM))
               {
                  context->grouplist[group].IOsegment[currentsegment] = segmentsize;
                  if (currentsegment < (Nex_MAXIOSEGMENTS - 1))
                  {
                     currentsegment++;
                     segmentsize = diff;
                  }
               }
               else
               {
                  segmentsize += diff;
               }
            }

            Nexx__eeprom2pdi(context, slave); /* set Eeprom control to PDI */
            Nexx__FPWRw(context->port, configadr, ECT_REG_ALCTL, htoes(Nex_STATE_SAFE_OP) , Nex_TIMEOUTRET3); /* set safeop status */

            if (context->slavelist[slave].blockLRW)
            {
               context->grouplist[group].blockLRW++;
            }
            context->grouplist[group].Ebuscurrent += context->slavelist[slave].Ebuscurrent;
         }
      }
      if (BitPos)
      {
         LogAddr++;
         oLogAddr = LogAddr;
         BitPos = 0;
         if ((segmentsize + 1) > (Nex_MAXLRWDATA - Nex_FIRSTDCDATAGRAM))
         {
            context->grouplist[group].IOsegment[currentsegment] = segmentsize;
            if (currentsegment < (Nex_MAXIOSEGMENTS - 1))
            {
               currentsegment++;
               segmentsize = 1;
            }
         }
         else
         {
            segmentsize += 1;
         }
      }
      context->grouplist[group].IOsegment[currentsegment] = segmentsize;
      context->grouplist[group].nsegments = currentsegment + 1;
      context->grouplist[group].inputs = (uint8 *)(pIOmap) + context->grouplist[group].Obytes;
      context->grouplist[group].Ibytes = LogAddr - context->grouplist[group].Obytes;
      if (!group)
      {
         context->slavelist[0].inputs = (uint8 *)(pIOmap) + context->slavelist[0].Obytes;
         context->slavelist[0].Ibytes = LogAddr - context->slavelist[0].Obytes; /* store input bytes in master record */
      }

      Nex_PRINT("IOmapSize %d\n", LogAddr - context->grouplist[group].logstartaddr);

      return (LogAddr - context->grouplist[group].logstartaddr);
   }

   return 0;
}

/** Map all PDOs in one group of slaves to IOmap with Outputs/Inputs
 * overlapping. NOTE: Must use this for TI ESC when using LRW.
 *
 * @param[in]  context    = context struct
 * @param[out] pIOmap     = pointer to IOmap
 * @param[in]  group      = group to map, 0 = all groups
 * @return IOmap size
 */
int Nexx__config_overlap_map_group(Nexx__contextt *context, void *pIOmap, uint8 group)
{
   uint16 slave, configadr;
   uint8 BitPos;
   uint32 mLogAddr = 0;
   uint32 siLogAddr = 0;
   uint32 soLogAddr = 0;
   uint32 tempLogAddr;
   uint32 diff;
   uint16 currentsegment = 0;
   uint32 segmentsize = 0;

   if ((*(context->slavecount) > 0) && (group < context->maxgroup))
   {
      Nex_PRINT("Nex_config_map_group IOmap:%p group:%d\n", pIOmap, group);
      mLogAddr = context->grouplist[group].logstartaddr;
      siLogAddr = mLogAddr;
      soLogAddr = mLogAddr;
      BitPos = 0;
      context->grouplist[group].nsegments = 0;
      context->grouplist[group].outputsWKC = 0;
      context->grouplist[group].inputsWKC = 0;

      /* Find mappings and program syncmanagers */
      Nexx__config_find_mappings(context, group);
      
      /* do IO mapping of slave and program FMMUs */
      for (slave = 1; slave <= *(context->slavecount); slave++)
      {
         configadr = context->slavelist[slave].configadr;
         siLogAddr = soLogAddr = mLogAddr;

         if (!group || (group == context->slavelist[slave].group))
         {
            /* create output mapping */
            if (context->slavelist[slave].Obits)
            {
               
               Nexx__config_create_output_mappings(context, pIOmap, group, 
                  slave, &soLogAddr, &BitPos);
               if (BitPos)
               {
                  soLogAddr++;
                  BitPos = 0;
               }
            }

            /* create input mapping */
            if (context->slavelist[slave].Ibits)
            {
               Nexx__config_create_input_mappings(context, pIOmap, group, 
                  slave, &siLogAddr, &BitPos);
               if (BitPos)
               {
                  siLogAddr++;
                  BitPos = 0;
               }
            }

            tempLogAddr = (siLogAddr > soLogAddr) ?  siLogAddr : soLogAddr;
            diff = tempLogAddr - mLogAddr;
            mLogAddr = tempLogAddr;

            if ((segmentsize + diff) > (Nex_MAXLRWDATA - Nex_FIRSTDCDATAGRAM))
            {
               context->grouplist[group].IOsegment[currentsegment] = segmentsize;
               if (currentsegment < (Nex_MAXIOSEGMENTS - 1))
               {
                  currentsegment++;
                  segmentsize = diff;
               }
            }
            else
            {
               segmentsize += diff;
            }

            Nexx__eeprom2pdi(context, slave); /* set Eeprom control to PDI */
            Nexx__FPWRw(context->port, configadr, ECT_REG_ALCTL, htoes(Nex_STATE_SAFE_OP), Nex_TIMEOUTRET3); /* set safeop status */

            if (context->slavelist[slave].blockLRW)
            {
               context->grouplist[group].blockLRW++;
            }
            context->grouplist[group].Ebuscurrent += context->slavelist[slave].Ebuscurrent;

         }
      }

      context->grouplist[group].IOsegment[currentsegment] = segmentsize;
      context->grouplist[group].nsegments = currentsegment + 1;
      context->grouplist[group].Isegment = 0;
      context->grouplist[group].Ioffset = 0;

      context->grouplist[group].Obytes = soLogAddr;
      context->grouplist[group].Ibytes = siLogAddr;
      context->grouplist[group].outputs = pIOmap;
      context->grouplist[group].inputs = (uint8 *)pIOmap + context->grouplist[group].Obytes;

      /* Move calculated inputs with OBytes offset*/
      for (slave = 1; slave <= *(context->slavecount); slave++)
      {
         context->slavelist[slave].inputs += context->grouplist[group].Obytes;
      }

      if (!group)
      {
         context->slavelist[0].outputs = pIOmap;
         context->slavelist[0].Obytes = soLogAddr; /* store output bytes in master record */
         context->slavelist[0].inputs = (uint8 *)pIOmap + context->slavelist[0].Obytes;
         context->slavelist[0].Ibytes = siLogAddr;
      }

      Nex_PRINT("IOmapSize %d\n", context->grouplist[group].Obytes + context->grouplist[group].Ibytes);

      return (context->grouplist[group].Obytes + context->grouplist[group].Ibytes);
   }

   return 0;
}


/** Recover slave.
 *
 * @param[in] context = context struct
 * @param[in] slave   = slave to recover
 * @param[in] timeout = local timeout f.e. Nex_TIMEOUTRET3
 * @return >0 if successful
 */
int Nexx__recover_slave(Nexx__contextt *context, uint16 slave, int timeout)
{
   int rval;
   int wkc;
   uint16 ADPh, configadr, readadr;

   rval = 0;
   configadr = context->slavelist[slave].configadr;
   ADPh = (uint16)(1 - slave);
   /* check if we found another slave than the requested */
   readadr = 0xfffe;
   wkc = Nexx__APRD(context->port, ADPh, ECT_REG_STADR, sizeof(readadr), &readadr, timeout);
   /* correct slave found, finished */
   if(readadr == configadr)
   {
       return 1;
   }
   /* only try if no config address*/
   if( (wkc > 0) && (readadr == 0))
   {
      /* clear possible slaves at Nex_TEMPNODE */
      Nexx__FPWRw(context->port, Nex_TEMPNODE, ECT_REG_STADR, htoes(0) , 0);
      /* set temporary node address of slave */
      if(Nexx__APWRw(context->port, ADPh, ECT_REG_STADR, htoes(Nex_TEMPNODE) , timeout) <= 0)
      {
         Nexx__FPWRw(context->port, Nex_TEMPNODE, ECT_REG_STADR, htoes(0) , 0);
         return 0; /* slave fails to respond */
      }

      context->slavelist[slave].configadr = Nex_TEMPNODE; /* temporary config address */
      Nexx__eeprom2master(context, slave); /* set Eeprom control to master */

      /* check if slave is the same as configured before */
      if ((Nexx__FPRDw(context->port, Nex_TEMPNODE, ECT_REG_ALIAS, timeout) ==
             context->slavelist[slave].aliasadr) &&
          (Nexx__readeeprom(context, slave, ECT_SII_ID, Nex_TIMEOUTEEP) ==
             context->slavelist[slave].eep_id) &&
          (Nexx__readeeprom(context, slave, ECT_SII_MANUF, Nex_TIMEOUTEEP) ==
             context->slavelist[slave].eep_man) &&
          (Nexx__readeeprom(context, slave, ECT_SII_REV, Nex_TIMEOUTEEP) ==
             context->slavelist[slave].eep_rev))
      {
         rval = Nexx__FPWRw(context->port, Nex_TEMPNODE, ECT_REG_STADR, htoes(configadr) , timeout);
         context->slavelist[slave].configadr = configadr;
      }
      else
      {
         /* slave is not the expected one, remove config address*/
         Nexx__FPWRw(context->port, Nex_TEMPNODE, ECT_REG_STADR, htoes(0) , timeout);
         context->slavelist[slave].configadr = configadr;
      }
   }

   return rval;
}

/** Reconfigure slave.
 *
 * @param[in] context = context struct
 * @param[in] slave   = slave to reconfigure
 * @param[in] timeout = local timeout f.e. Nex_TIMEOUTRET3
 * @return Slave state
 */
int Nexx__reconfig_slave(Nexx__contextt *context, uint16 slave, int timeout)
{
   int state, nSM, FMMUc;
   uint16 configadr;

   configadr = context->slavelist[slave].configadr;
   if (Nexx__FPWRw(context->port, configadr, ECT_REG_ALCTL, htoes(Nex_STATE_INIT) , timeout) <= 0)
   {
      return 0;
   }
   state = 0;
   Nexx__eeprom2pdi(context, slave); /* set Eeprom control to PDI */
   /* check state change init */
   state = Nexx__statecheck(context, slave, Nex_STATE_INIT, Nex_TIMEOUTSTATE);
   if(state == Nex_STATE_INIT)
   {
      /* program all enabled SM */
      for( nSM = 0 ; nSM < Nex_MAXSM ; nSM++ )
      {
         if (context->slavelist[slave].SM[nSM].StartAddr)
         {
            Nexx__FPWR(context->port, configadr, (uint16)(ECT_REG_SM0 + (nSM * sizeof(Nex_smt))),
               sizeof(Nex_smt), &context->slavelist[slave].SM[nSM], timeout);
         }
      }
      Nexx__FPWRw(context->port, configadr, ECT_REG_ALCTL, htoes(Nex_STATE_PRE_OP) , timeout);
      state = Nexx__statecheck(context, slave, Nex_STATE_PRE_OP, Nex_TIMEOUTSTATE); /* check state change pre-op */
      if( state == Nex_STATE_PRE_OP)
      {
         /* execute special slave configuration hook Pre-Op to Safe-OP */
         if(context->slavelist[slave].PO2SOconfig) /* only if registered */
         {
            context->slavelist[slave].PO2SOconfig(slave);
         }
         Nexx__FPWRw(context->port, configadr, ECT_REG_ALCTL, htoes(Nex_STATE_SAFE_OP) , timeout); /* set safeop status */
         state = Nexx__statecheck(context, slave, Nex_STATE_SAFE_OP, Nex_TIMEOUTSTATE); /* check state change safe-op */
         /* program configured FMMU */
         for( FMMUc = 0 ; FMMUc < context->slavelist[slave].FMMUunused ; FMMUc++ )
         {
            Nexx__FPWR(context->port, configadr, (uint16)(ECT_REG_FMMU0 + (sizeof(Nex_fmmut) * FMMUc)),
               sizeof(Nex_fmmut), &context->slavelist[slave].FMMU[FMMUc], timeout);
         }
      }
   }

   return state;
}


/** Enumerate and init all slaves.
 *
 * @param[in] usetable     = TRUE when using configtable to init slaves, FALSE otherwise
 * @return Workcounter of slave discover datagram = number of slaves found
 * @see Nexx__config_init
 */
int Nex_config_init(void)
{
   return Nexx__config_init(&Nexx__context);
}

/** Map all PDOs in one group of slaves to IOmap with Outputs/Inputs
 * in sequential order (legacy SOEM way).
 *
 * @param[out] pIOmap     = pointer to IOmap
 * @param[in]  group      = group to map, 0 = all groups
 * @return IOmap size
 * @see Nexx__config_map_group
 */
int Nex_config_map_group(void *pIOmap, uint8 group)
{
   return Nexx__config_map_group(&Nexx__context, pIOmap, group);
}

/** Map all PDOs in one group of slaves to IOmap with Outputs/Inputs
* overlapping. NOTE: Must use this for TI ESC when using LRW.
*
* @param[out] pIOmap     = pointer to IOmap
* @param[in]  group      = group to map, 0 = all groups
* @return IOmap size
* @see Nexx__config_overlap_map_group
*/
int Nex_config_overlap_map_group(void *pIOmap, uint8 group)
{
   return Nexx__config_overlap_map_group(&Nexx__context, pIOmap, group);
}

/** Map all PDOs from slaves to IOmap with Outputs/Inputs
 * in sequential order (legacy SOEM way).
 *
 * @param[out] pIOmap     = pointer to IOmap
 * @return IOmap size
 */
int Nex_config_map(void *pIOmap)
{
   return Nex_config_map_group(pIOmap, 0);
}

/** Map all PDOs from slaves to IOmap with Outputs/Inputs
* overlapping. NOTE: Must use this for TI ESC when using LRW.
*
* @param[out] pIOmap     = pointer to IOmap
* @return IOmap size
*/
int Nex_config_overlap_map(void *pIOmap)
{
   return Nex_config_overlap_map_group(pIOmap, 0);
}

/** Enumerate / map and init all slaves.
 *
 * @param[in] usetable    = TRUE when using configtable to init slaves, FALSE otherwise
 * @param[out] pIOmap     = pointer to IOmap
 * @return Workcounter of slave discover datagram = number of slaves found
 */
int Nex_config(void *pIOmap)
{
   int wkc;
   wkc = Nex_config_init();
   if (wkc)
   {
      Nex_config_map(pIOmap);
   }
   return wkc;
}

/** Enumerate / map and init all slaves.
*
* @param[in] usetable    = TRUE when using configtable to init slaves, FALSE otherwise
* @param[out] pIOmap     = pointer to IOmap
* @return Workcounter of slave discover datagram = number of slaves found
*/
int Nex_config_overlap(void *pIOmap)
{
   int wkc;
   wkc = Nex_config_init();
   if (wkc)
   {
      Nex_config_overlap_map(pIOmap);
   }
   return wkc;
}

/** Recover slave.
 *
 * @param[in] slave   = slave to recover
 * @param[in] timeout = local timeout f.e. Nex_TIMEOUTRET3
 * @return >0 if successful
 * @see Nexx__recover_slave
 */
int Nex_recover_slave(uint16 slave, int timeout)
{
   return Nexx__recover_slave(&Nexx__context, slave, timeout);
}

/** Reconfigure slave.
 *
 * @param[in] slave   = slave to reconfigure
 * @param[in] timeout = local timeout f.e. Nex_TIMEOUTRET3
 * @return Slave state
 * @see Nexx__reconfig_slave
 */
int Nex_reconfig_slave(uint16 slave, int timeout)
{
   return Nexx__reconfig_slave(&Nexx__context, slave, timeout);
}

