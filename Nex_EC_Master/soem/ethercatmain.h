/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

/** \file
 * \brief
 * Headerfile for ethercatmain.c
 */

#ifndef _ethercatmain_
#define _ethercatmain_


#ifdef __cplusplus
extern "C"
{
#endif

/** max. entries in EtherCAT error list */
#define Nex_MAXELIST       64
/** max. length of readable name in slavelist and Object Description List */
#define Nex_MAXNAME        40
/** max. number of slaves in array */
#define Nex_MAXSLAVE       200
/** max. number of groups */
#define Nex_MAXGROUP       2
/** max. number of IO segments per group */
#define Nex_MAXIOSEGMENTS  64
/** max. mailbox size */
#define Nex_MAXMBX         1486
/** max. eeprom PDO entries */
#define Nex_MAXEEPDO       0x200
/** max. SM used */
#define Nex_MAXSM          8
/** max. FMMU used */
#define Nex_MAXFMMU        4
/** max. Adapter */
#define Nex_MAXLEN_ADAPTERNAME    128
/** define maximum number of concurrent threads in mapping */
#define Nex_MAX_MAPT           1

typedef struct Nex_adapter Nex_adaptert;
struct Nex_adapter
{
   char   name[Nex_MAXLEN_ADAPTERNAME];
   char   desc[Nex_MAXLEN_ADAPTERNAME];
   Nex_adaptert *next;
};

/** record for FMMU */
PACKED_BEGIN
typedef struct PACKED Nex_fmmu
{
   uint32  LogStart;
   uint16  LogLength;
   uint8   LogStartbit;
   uint8   LogEndbit;
   uint16  PhysStart;
   uint8   PhysStartBit;
   uint8   FMMUtype;
   uint8   FMMUactive;
   uint8   unused1;
   uint16  unused2;
}  Nex_fmmut;
PACKED_END

/** record for sync manager */
PACKED_BEGIN
typedef struct PACKED Nex_sm
{
   uint16  StartAddr;
   uint16  SMlength;
   uint32  SMflags;
} Nex_smt;
PACKED_END

PACKED_BEGIN
typedef struct PACKED Nex_state_status
{
   uint16  State;
   uint16  Unused;
   uint16  ALstatuscode;
} Nex_state_status;
PACKED_END

#define ECT_MBXPROT_AOE      0x0001
#define ECT_MBXPROT_EOE      0x0002
#define ECT_MBXPROT_COE      0x0004
#define ECT_MBXPROT_FOE      0x0008
#define ECT_MBXPROT_SOE      0x0010
#define ECT_MBXPROT_VOE      0x0020

#define ECT_COEDET_SDO       0x01
#define ECT_COEDET_SDOINFO   0x02
#define ECT_COEDET_PDOASSIGN 0x04
#define ECT_COEDET_PDOCONFIG 0x08
#define ECT_COEDET_UPLOAD    0x10
#define ECT_COEDET_SDOCA     0x20

#define Nex_SMENABLEMASK      0xfffeffff

/** for list of ethercat slaves detected */
typedef struct Nex_slave
{
   /** state of slave */
   uint16           state;
   /** AL status code */
   uint16           ALstatuscode;
   /** Configured address */
   uint16           configadr;
   /** Alias address */
   uint16           aliasadr;
   /** Manufacturer from EEprom */
   uint32           eep_man;
   /** ID from EEprom */
   uint32           eep_id;
   /** revision from EEprom */
   uint32           eep_rev;
   /** Interface type */
   uint16           Itype;
   /** Device type */
   uint16           Dtype;
   /** output bits */
   uint16           Obits;
   /** output bytes, if Obits < 8 then Obytes = 0 */
   uint32           Obytes;
   /** output pointer in IOmap buffer */
   uint8            *outputs;
   /** startbit in first output byte */
   uint8            Ostartbit;
   /** input bits */
   uint16           Ibits;
   /** input bytes, if Ibits < 8 then Ibytes = 0 */
   uint32           Ibytes;
   /** input pointer in IOmap buffer */
   uint8            *inputs;
   /** startbit in first input byte */
   uint8            Istartbit;
   /** SM structure */
   Nex_smt           SM[Nex_MAXSM];
   /** SM type 0=unused 1=MbxWr 2=MbxRd 3=Outputs 4=Inputs */
   uint8            SMtype[Nex_MAXSM];
   /** FMMU structure */
   Nex_fmmut         FMMU[Nex_MAXFMMU];
   /** FMMU0 function */
   uint8            FMMU0func;
   /** FMMU1 function */
   uint8            FMMU1func;
   /** FMMU2 function */
   uint8            FMMU2func;
   /** FMMU3 function */
   uint8            FMMU3func;
   /** length of write mailbox in bytes, if no mailbox then 0 */
   uint16           mbx_l;
   /** mailbox write offset */
   uint16           mbx_wo;
   /** length of read mailbox in bytes */
   uint16           mbx_rl;
   /** mailbox read offset */
   uint16           mbx_ro;
   /** mailbox supported protocols */
   uint16           mbx_proto;
   /** Counter value of mailbox link layer protocol 1..7 */
   uint8            mbx_cnt;
   /** has DC capability */
   boolean          hasdc;
   /** Physical type; Ebus, EtherNet combinations */
   uint8            ptype;
   /** topology: 1 to 3 links */
   uint8            topology;
   /** active ports bitmap : ....3210 , set if respective port is active **/
   uint8            activeports;
   /** consumed ports bitmap : ....3210, used for internal delay measurement **/
   uint8            consumedports;
   /** slave number for parent, 0=master */
   uint16           parent;
   /** port number on parent this slave is connected to **/
   uint8            parentport;
   /** port number on this slave the parent is connected to **/
   uint8            entryport;
   /** DC receivetimes on port A */
   int32            DCrtA;
   /** DC receivetimes on port B */
   int32            DCrtB;
   /** DC receivetimes on port C */
   int32            DCrtC;
   /** DC receivetimes on port D */
   int32            DCrtD;
   /** propagation delay */
   int32            pdelay;
   /** next DC slave */
   uint16           DCnext;
   /** previous DC slave */
   uint16           DCprevious;
   /** DC cycle time in ns */
   int32            DCcycle;
   /** DC shift from clock modulus boundary */
   int32            DCshift;
   /** DC sync activation, 0=off, 1=on */
   uint8            DCactive;
   /** link to config table */
   uint16           configindex;
   /** link to SII config */
   uint16           SIIindex;
   /** 1 = 8 bytes per read, 0 = 4 bytes per read */
   uint8            eep_8byte;
   /** 0 = eeprom to master , 1 = eeprom to PDI */
   uint8            eep_pdi;
   /** CoE details */
   uint8            CoEdetails;
   /** FoE details */
   uint8            FoEdetails;
   /** EoE details */
   uint8            EoEdetails;
   /** SoE details */
   uint8            SoEdetails;
   /** E-bus current */
   int16            Ebuscurrent;
   /** if >0 block use of LRW in processdata */
   uint8            blockLRW;
   /** group */
   uint8            group;
   /** first unused FMMU */
   uint8            FMMUunused;
   /** Boolean for tracking whether the slave is (not) responding, not used/set by the SOEM library */
   boolean          islost;
   /** registered configuration function PO->SO */
   int              (*PO2SOconfig)(uint16 slave);
   /** readable name */
   char             name[Nex_MAXNAME + 1];
} Nex_slavet;

/** for list of ethercat slave groups */
typedef struct Nex_group
{
   /** logical start address for this group */
   uint32           logstartaddr;
   /** output bytes, if Obits < 8 then Obytes = 0 */
   uint32           Obytes;
   /** output pointer in IOmap buffer */
   uint8            *outputs;
   /** input bytes, if Ibits < 8 then Ibytes = 0 */
   uint32           Ibytes;
   /** input pointer in IOmap buffer */
   uint8            *inputs;
   /** has DC capabillity */
   boolean          hasdc;
   /** next DC slave */
   uint16           DCnext;
   /** E-bus current */
   int16            Ebuscurrent;
   /** if >0 block use of LRW in processdata */
   uint8            blockLRW;
   /** IO segegments used */
   uint16           nsegments;
   /** 1st input segment */
   uint16           Isegment;
   /** Offset in input segment */
   uint16           Ioffset;
   /** Expected workcounter outputs */
   uint16           outputsWKC;
   /** Expected workcounter inputs */
   uint16           inputsWKC;
   /** check slave states */
   boolean          docheckstate;
   /** IO segmentation list. Datagrams must not break SM in two. */
   uint32           IOsegment[Nex_MAXIOSEGMENTS];
} Nex_groupt;

/** SII FMMU structure */
typedef struct Nex_eepromFMMU
{
   uint16  Startpos;
   uint8   nFMMU;
   uint8   FMMU0;
   uint8   FMMU1;
   uint8   FMMU2;
   uint8   FMMU3;
} Nex_eepromFMMUt;

/** SII SM structure */
typedef struct Nex_eepromSM
{
   uint16  Startpos;
   uint8   nSM;
   uint16  PhStart;
   uint16  Plength;
   uint8   Creg;
   uint8   Sreg;       /* dont care */
   uint8   Activate;
   uint8   PDIctrl;      /* dont care */
} Nex_eepromSMt;

/** record to store rxPDO and txPDO table from eeprom */
typedef struct Nex_eepromPDO
{
   uint16  Startpos;
   uint16  Length;
   uint16  nPDO;
   uint16  Index[Nex_MAXEEPDO];
   uint16  SyncM[Nex_MAXEEPDO];
   uint16  BitSize[Nex_MAXEEPDO];
   uint16  SMbitsize[Nex_MAXSM];
} Nex_eepromPDOt;

/** mailbox buffer array */
typedef uint8 Nex_mbxbuft[Nex_MAXMBX + 1];

/** standard ethercat mailbox header */
PACKED_BEGIN
typedef struct PACKED Nex_mbxheader
{
   uint16  length;
   uint16  address;
   uint8   priority;
   uint8   mbxtype;
} Nex_mbxheadert;
PACKED_END

/** ALstatus and ALstatus code */
PACKED_BEGIN
typedef struct PACKED Nex_alstatus
{
   uint16  alstatus;
   uint16  unused;
   uint16  alstatuscode;
} Nex_alstatust;
PACKED_END

/** stack structure to store segmented LRD/LWR/LRW constructs */
typedef struct Nex_idxstack
{
   uint8   pushed;
   uint8   pulled;
   uint8   idx[Nex_MAXBUF];
   void    *data[Nex_MAXBUF];
   uint16  length[Nex_MAXBUF];
} Nex_idxstackT;

/** ringbuf for error storage */
typedef struct Nex_ering
{
   int16     head;
   int16     tail;
   Nex_errort Error[Nex_MAXELIST + 1];
} Nex_eringt;

/** SyncManager Communication Type structure for CA */
PACKED_BEGIN
typedef struct PACKED Nex_SMcommtype
{
   uint8   n;
   uint8   nu1;
   uint8   SMtype[Nex_MAXSM];
} Nex_SMcommtypet;
PACKED_END

/** SDO assign structure for CA */
PACKED_BEGIN
typedef struct PACKED Nex_PDOassign
{
   uint8   n;
   uint8   nu1;
   uint16  index[256];
} Nex_PDOassignt;
PACKED_END

/** SDO description structure for CA */
PACKED_BEGIN
typedef struct PACKED Nex_PDOdesc
{
   uint8   n;
   uint8   nu1;
   uint32  PDO[256];
} Nex_PDOdesct;
PACKED_END

/** Context structure , referenced by all Nexx_ functions*/
typedef struct Nexx__context
{
   /** port reference, may include red_port */
   Nexx__portt      *port;
   /** slavelist reference */
   Nex_slavet      *slavelist;
   /** number of slaves found in configuration */
   int            *slavecount;
   /** maximum number of slaves allowed in slavelist */
   int            maxslave;
   /** grouplist reference */
   Nex_groupt      *grouplist;
   /** maximum number of groups allowed in grouplist */
   int            maxgroup;
   /** internal, reference to eeprom cache buffer */
   uint8          *esibuf;
   /** internal, reference to eeprom cache map */
   uint32         *esimap;
   /** internal, current slave for eeprom cache */
   uint16         esislave;
   /** internal, reference to error list */
   Nex_eringt      *elist;
   /** internal, reference to processdata stack buffer info */
   Nex_idxstackT   *idxstack;
   /** reference to ecaterror state */
   boolean        *ecaterror;
   /** internal, position of DC datagram in process data packet */
   uint16         DCtO;
   /** internal, length of DC datagram */
   uint16         DCl;
   /** reference to last DC time from slaves */
   int64          *DCtime;
   /** internal, SM buffer */
   Nex_SMcommtypet *SMcommtype;
   /** internal, PDO assign list */
   Nex_PDOassignt  *PDOassign;
   /** internal, PDO description list */
   Nex_PDOdesct    *PDOdesc;
   /** internal, SM list from eeprom */
   Nex_eepromSMt   *eepSM;
   /** internal, FMMU list from eeprom */
   Nex_eepromFMMUt *eepFMMU;
   /** registered FoE hook */
   int            (*FOEhook)(uint16 slave, int packetnumber, int datasize);
} Nexx__contextt;


/** global struct to hold default master context */
extern Nexx__contextt  Nexx__context;
/** main slave data structure array */
extern Nex_slavet   Nex_slave[Nex_MAXSLAVE];
/** number of slaves found by configuration function */
extern int         Nex_slavecount;
/** slave group structure */
extern Nex_groupt   Nex_group[Nex_MAXGROUP];
extern boolean     EcatError;
extern int64       Nex_DCtime;

void Nex_pusherror(const Nex_errort *Ec);
boolean Nex_poperror(Nex_errort *Ec);
boolean Nex_iserror(void);
void Nex_packeterror(uint16 Slave, uint16 Index, uint8 SubIdx, uint16 ErrorCode);
int Nex_init(const char * ifname);
int Nex_init_redundant(const char *ifname, char *if2name);
void Nex_close(void);
uint8 Nex_siigetbyte(uint16 slave, uint16 address);
int16 Nex_siifind(uint16 slave, uint16 cat);
void Nex_siistring(char *str, uint16 slave, uint16 Sn);
uint16 Nex_siiFMMU(uint16 slave, Nex_eepromFMMUt* FMMU);
uint16 Nex_siiSM(uint16 slave, Nex_eepromSMt* SM);
uint16 Nex_siiSMnext(uint16 slave, Nex_eepromSMt* SM, uint16 n);
int Nex_siiPDO(uint16 slave, Nex_eepromPDOt* PDO, uint8 t);
int Nex_readstate(void);
int Nex_writestate(uint16 slave);
uint16 Nex_statecheck(uint16 slave, uint16 reqstate, int timeout);
int Nex_mbxempty(uint16 slave, int timeout);
int Nex_mbxsend(uint16 slave,Nex_mbxbuft *mbx, int timeout);
int Nex_mbxreceive(uint16 slave, Nex_mbxbuft *mbx, int timeout);
void Nex_esidump(uint16 slave, uint8 *esibuf);
uint32 Nex_readeeprom(uint16 slave, uint16 eeproma, int timeout);
int Nex_writeeeprom(uint16 slave, uint16 eeproma, uint16 data, int timeout);
int Nex_eeprom2master(uint16 slave);
int Nex_eeprom2pdi(uint16 slave);
uint64 Nex_readeepromAP(uint16 aiadr, uint16 eeproma, int timeout);
int Nex_writeeepromAP(uint16 aiadr, uint16 eeproma, uint16 data, int timeout);
uint64 Nex_readeepromFP(uint16 configadr, uint16 eeproma, int timeout);
int Nex_writeeepromFP(uint16 configadr, uint16 eeproma, uint16 data, int timeout);
void Nex_readeeprom1(uint16 slave, uint16 eeproma);
uint32 Nex_readeeprom2(uint16 slave, int timeout);
int Nex_send_processdata_group(uint8 group);
int Nex_send_overlap_processdata_group(uint8 group);
int Nex_receive_processdata_group(uint8 group, int timeout);
int Nex_send_processdata(void);
int Nex_send_overlap_processdata(void);
int Nex_receive_processdata(int timeout);


Nex_adaptert * Nex_find_adapters(void);
void Nex_free_adapters(Nex_adaptert * adapter);
uint8 Nex_nextmbxcnt(uint8 cnt);
void Nex_clearmbx(Nex_mbxbuft *Mbx);
void Nexx__pusherror(Nexx__contextt *context, const Nex_errort *Ec);
boolean Nexx__poperror(Nexx__contextt *context, Nex_errort *Ec);
boolean Nexx__iserror(Nexx__contextt *context);
void Nexx__packeterror(Nexx__contextt *context, uint16 Slave, uint16 Index, uint8 SubIdx, uint16 ErrorCode);
int Nexx__init(Nexx__contextt *context, const char * ifname);
int Nexx__init_redundant(Nexx__contextt *context, Nexx__redportt *redport, const char *ifname, char *if2name);
void Nexx__close(Nexx__contextt *context);
uint8 Nexx__siigetbyte(Nexx__contextt *context, uint16 slave, uint16 address);
int16 Nexx__siifind(Nexx__contextt *context, uint16 slave, uint16 cat);
void Nexx__siistring(Nexx__contextt *context, char *str, uint16 slave, uint16 Sn);
uint16 Nexx__siiFMMU(Nexx__contextt *context, uint16 slave, Nex_eepromFMMUt* FMMU);
uint16 Nexx__siiSM(Nexx__contextt *context, uint16 slave, Nex_eepromSMt* SM);
uint16 Nexx__siiSMnext(Nexx__contextt *context, uint16 slave, Nex_eepromSMt* SM, uint16 n);
int Nexx__siiPDO(Nexx__contextt *context, uint16 slave, Nex_eepromPDOt* PDO, uint8 t);
int Nexx__readstate(Nexx__contextt *context);
int Nexx__writestate(Nexx__contextt *context, uint16 slave);
uint16 Nexx__statecheck(Nexx__contextt *context, uint16 slave, uint16 reqstate, int timeout);
int Nexx__mbxempty(Nexx__contextt *context, uint16 slave, int timeout);
int Nexx__mbxsend(Nexx__contextt *context, uint16 slave,Nex_mbxbuft *mbx, int timeout);
int Nexx__mbxreceive(Nexx__contextt *context, uint16 slave, Nex_mbxbuft *mbx, int timeout);
void Nexx__esidump(Nexx__contextt *context, uint16 slave, uint8 *esibuf);
uint32 Nexx__readeeprom(Nexx__contextt *context, uint16 slave, uint16 eeproma, int timeout);
int Nexx__writeeeprom(Nexx__contextt *context, uint16 slave, uint16 eeproma, uint16 data, int timeout);
int Nexx__eeprom2master(Nexx__contextt *context, uint16 slave);
int Nexx__eeprom2pdi(Nexx__contextt *context, uint16 slave);
uint64 Nexx__readeepromAP(Nexx__contextt *context, uint16 aiadr, uint16 eeproma, int timeout);
int Nexx__writeeepromAP(Nexx__contextt *context, uint16 aiadr, uint16 eeproma, uint16 data, int timeout);
uint64 Nexx__readeepromFP(Nexx__contextt *context, uint16 configadr, uint16 eeproma, int timeout);
int Nexx__writeeepromFP(Nexx__contextt *context, uint16 configadr, uint16 eeproma, uint16 data, int timeout);
void Nexx__readeeprom1(Nexx__contextt *context, uint16 slave, uint16 eeproma);
uint32 Nexx__readeeprom2(Nexx__contextt *context, uint16 slave, int timeout);
int Nexx__send_overlap_processdata_group(Nexx__contextt *context, uint8 group);
int Nexx__receive_processdata_group(Nexx__contextt *context, uint8 group, int timeout);
int Nexx__send_processdata(Nexx__contextt *context);
int Nexx__send_overlap_processdata(Nexx__contextt *context);
int Nexx__receive_processdata(Nexx__contextt *context, int timeout);

#ifdef __cplusplus
}
#endif

#endif
