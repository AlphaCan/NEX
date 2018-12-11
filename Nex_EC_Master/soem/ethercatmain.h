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
#define NEX_MAXELIST       64
/** max. length of readable name in slavelist and Object Description List */
#define NEX_MAXNAME        40
/** max. number of slaves in array */
#define NEX_MAXSLAVE       200
/** max. number of groups */
#define NEX_MAXGROUP       2
/** max. number of IO segments per group */
#define NEX_MAXIOSEGMENTS  64
/** max. mailbox size */
#define NEX_MAXMBX         1486
/** max. eeprom PDO entries */
#define NEX_MAXEEPDO       0x200
/** max. SM used */
#define NEX_MAXSM          8
/** max. FMMU used */
#define NEX_MAXFMMU        4
/** max. Adapter */
#define NEX_MAXLEN_ADAPTERNAME    128
/** define maximum number of concurrent threads in mapping */
#define NEX_MAX_MAPT           1

typedef struct nex_adapter nex_adaptert;
struct nex_adapter
{
   char   name[NEX_MAXLEN_ADAPTERNAME];
   char   desc[NEX_MAXLEN_ADAPTERNAME];
   nex_adaptert *next;
};

/** record for FMMU */
PACKED_BEGIN
typedef struct PACKED nex_fmmu
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
}  nex_fmmut;
PACKED_END

/** record for sync manager */
PACKED_BEGIN
typedef struct PACKED nex_sm
{
   uint16  StartAddr;
   uint16  SMlength;
   uint32  SMflags;
} nex_smt;
PACKED_END

PACKED_BEGIN
typedef struct PACKED nex_state_status
{
   uint16  State;
   uint16  Unused;
   uint16  ALstatuscode;
} nex_state_status;
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

#define NEX_SMENABLEMASK      0xfffeffff

/** for list of ethercat slaves detected */
typedef struct nex_slave
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
   nex_smt           SM[NEX_MAXSM];
   /** SM type 0=unused 1=MbxWr 2=MbxRd 3=Outputs 4=Inputs */
   uint8            SMtype[NEX_MAXSM];
   /** FMMU structure */
   nex_fmmut         FMMU[NEX_MAXFMMU];
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
   char             name[NEX_MAXNAME + 1];
} nex_slavet;

/** for list of ethercat slave groups */
typedef struct nex_group
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
   uint32           IOsegment[NEX_MAXIOSEGMENTS];
} nex_groupt;

/** SII FMMU structure */
typedef struct nex_eepromFMMU
{
   uint16  Startpos;
   uint8   nFMMU;
   uint8   FMMU0;
   uint8   FMMU1;
   uint8   FMMU2;
   uint8   FMMU3;
} nex_eepromFMMUt;

/** SII SM structure */
typedef struct nex_eepromSM
{
   uint16  Startpos;
   uint8   nSM;
   uint16  PhStart;
   uint16  Plength;
   uint8   Creg;
   uint8   Sreg;       /* dont care */
   uint8   Activate;
   uint8   PDIctrl;      /* dont care */
} nex_eepromSMt;

/** record to store rxPDO and txPDO table from eeprom */
typedef struct nex_eepromPDO
{
   uint16  Startpos;
   uint16  Length;
   uint16  nPDO;
   uint16  Index[NEX_MAXEEPDO];
   uint16  SyncM[NEX_MAXEEPDO];
   uint16  BitSize[NEX_MAXEEPDO];
   uint16  SMbitsize[NEX_MAXSM];
} nex_eepromPDOt;

/** mailbox buffer array */
typedef uint8 nex_mbxbuft[NEX_MAXMBX + 1];

/** standard ethercat mailbox header */
PACKED_BEGIN
typedef struct PACKED nex_mbxheader
{
   uint16  length;
   uint16  address;
   uint8   priority;
   uint8   mbxtype;
} nex_mbxheadert;
PACKED_END

/** ALstatus and ALstatus code */
PACKED_BEGIN
typedef struct PACKED nex_alstatus
{
   uint16  alstatus;
   uint16  unused;
   uint16  alstatuscode;
} nex_alstatust;
PACKED_END

/** stack structure to store segmented LRD/LWR/LRW constructs */
typedef struct nex_idxstack
{
   uint8   pushed;
   uint8   pulled;
   uint8   idx[NEX_MAXBUF];
   void    *data[NEX_MAXBUF];
   uint16  length[NEX_MAXBUF];
} nex_idxstackT;

/** ringbuf for error storage */
typedef struct nex_ering
{
   int16     head;
   int16     tail;
   nex_errort Error[NEX_MAXELIST + 1];
} nex_eringt;

/** SyncManager Communication Type structure for CA */
PACKED_BEGIN
typedef struct PACKED nex_SMcommtype
{
   uint8   n;
   uint8   nu1;
   uint8   SMtype[NEX_MAXSM];
} nex_SMcommtypet;
PACKED_END

/** SDO assign structure for CA */
PACKED_BEGIN
typedef struct PACKED nex_PDOassign
{
   uint8   n;
   uint8   nu1;
   uint16  index[256];
} nex_PDOassignt;
PACKED_END

/** SDO description structure for CA */
PACKED_BEGIN
typedef struct PACKED nex_PDOdesc
{
   uint8   n;
   uint8   nu1;
   uint32  PDO[256];
} nex_PDOdesct;
PACKED_END

/** Context structure , referenced by all ecx functions*/
typedef struct nexx_context
{
   /** port reference, may include red_port */
   nexx_portt      *port;
   /** slavelist reference */
   nex_slavet      *slavelist;
   /** number of slaves found in configuration */
   int            *slavecount;
   /** maximum number of slaves allowed in slavelist */
   int            maxslave;
   /** grouplist reference */
   nex_groupt      *grouplist;
   /** maximum number of groups allowed in grouplist */
   int            maxgroup;
   /** internal, reference to eeprom cache buffer */
   uint8          *esibuf;
   /** internal, reference to eeprom cache map */
   uint32         *esimap;
   /** internal, current slave for eeprom cache */
   uint16         esislave;
   /** internal, reference to error list */
   nex_eringt      *elist;
   /** internal, reference to processdata stack buffer info */
   nex_idxstackT   *idxstack;
   /** reference to ecaterror state */
   boolean        *ecaterror;
   /** internal, position of DC datagram in process data packet */
   uint16         DCtO;
   /** internal, length of DC datagram */
   uint16         DCl;
   /** reference to last DC time from slaves */
   int64          *DCtime;
   /** internal, SM buffer */
   nex_SMcommtypet *SMcommtype;
   /** internal, PDO assign list */
   nex_PDOassignt  *PDOassign;
   /** internal, PDO description list */
   nex_PDOdesct    *PDOdesc;
   /** internal, SM list from eeprom */
   nex_eepromSMt   *eepSM;
   /** internal, FMMU list from eeprom */
   nex_eepromFMMUt *eepFMMU;
   /** registered FoE hook */
   int            (*FOEhook)(uint16 slave, int packetnumber, int datasize);
} nexx_contextt;

#ifdef NEX_VER1
/** global struct to hold default master context */
extern nexx_contextt  nexx_context;
/** main slave data structure array */
extern nex_slavet   nex_slave[NEX_MAXSLAVE];
/** number of slaves found by configuration function */
extern int         nex_slavecount;
/** slave group structure */
extern nex_groupt   nex_group[NEX_MAXGROUP];
extern boolean     EcatError;
extern int64       nex_DCtime;

void nex_pusherror(const nex_errort *Ec);
boolean nex_poperror(nex_errort *Ec);
boolean nex_iserror(void);
void nex_packeterror(uint16 Slave, uint16 Index, uint8 SubIdx, uint16 ErrorCode);
int nex_init(const char * ifname);
int nex_init_redundant(const char *ifname, char *if2name);
void nex_close(void);
uint8 nex_siigetbyte(uint16 slave, uint16 address);
int16 nex_siifind(uint16 slave, uint16 cat);
void nex_siistring(char *str, uint16 slave, uint16 Sn);
uint16 nex_siiFMMU(uint16 slave, nex_eepromFMMUt* FMMU);
uint16 nex_siiSM(uint16 slave, nex_eepromSMt* SM);
uint16 nex_siiSMnext(uint16 slave, nex_eepromSMt* SM, uint16 n);
int nex_siiPDO(uint16 slave, nex_eepromPDOt* PDO, uint8 t);
int nex_readstate(void);
int nex_writestate(uint16 slave);
uint16 nex_statecheck(uint16 slave, uint16 reqstate, int timeout);
int nex_mbxempty(uint16 slave, int timeout);
int nex_mbxsend(uint16 slave,nex_mbxbuft *mbx, int timeout);
int nex_mbxreceive(uint16 slave, nex_mbxbuft *mbx, int timeout);
void nex_esidump(uint16 slave, uint8 *esibuf);
uint32 nex_readeeprom(uint16 slave, uint16 eeproma, int timeout);
int nex_writeeeprom(uint16 slave, uint16 eeproma, uint16 data, int timeout);
int nex_eeprom2master(uint16 slave);
int nex_eeprom2pdi(uint16 slave);
uint64 nex_readeepromAP(uint16 aiadr, uint16 eeproma, int timeout);
int nex_writeeepromAP(uint16 aiadr, uint16 eeproma, uint16 data, int timeout);
uint64 nex_readeepromFP(uint16 configadr, uint16 eeproma, int timeout);
int nex_writeeepromFP(uint16 configadr, uint16 eeproma, uint16 data, int timeout);
void nex_readeeprom1(uint16 slave, uint16 eeproma);
uint32 nex_readeeprom2(uint16 slave, int timeout);
int nex_send_processdata_group(uint8 group);
int nex_send_overlap_processdata_group(uint8 group);
int nex_receive_processdata_group(uint8 group, int timeout);
int nex_send_processdata(void);
int nex_send_overlap_processdata(void);
int nex_receive_processdata(int timeout);
#endif

nex_adaptert * nex_find_adapters(void);
void nex_free_adapters(nex_adaptert * adapter);
uint8 nex_nextmbxcnt(uint8 cnt);
void nex_clearmbx(nex_mbxbuft *Mbx);
void nexx_pusherror(nexx_contextt *context, const nex_errort *Ec);
boolean nexx_poperror(nexx_contextt *context, nex_errort *Ec);
boolean nexx_iserror(nexx_contextt *context);
void nexx_packeterror(nexx_contextt *context, uint16 Slave, uint16 Index, uint8 SubIdx, uint16 ErrorCode);
int nexx_init(nexx_contextt *context, const char * ifname);
int nexx_init_redundant(nexx_contextt *context, nexx_redportt *redport, const char *ifname, char *if2name);
void nexx_close(nexx_contextt *context);
uint8 nexx_siigetbyte(nexx_contextt *context, uint16 slave, uint16 address);
int16 nexx_siifind(nexx_contextt *context, uint16 slave, uint16 cat);
void nexx_siistring(nexx_contextt *context, char *str, uint16 slave, uint16 Sn);
uint16 nexx_siiFMMU(nexx_contextt *context, uint16 slave, nex_eepromFMMUt* FMMU);
uint16 nexx_siiSM(nexx_contextt *context, uint16 slave, nex_eepromSMt* SM);
uint16 nexx_siiSMnext(nexx_contextt *context, uint16 slave, nex_eepromSMt* SM, uint16 n);
int nexx_siiPDO(nexx_contextt *context, uint16 slave, nex_eepromPDOt* PDO, uint8 t);
int nexx_readstate(nexx_contextt *context);
int nexx_writestate(nexx_contextt *context, uint16 slave);
uint16 nexx_statecheck(nexx_contextt *context, uint16 slave, uint16 reqstate, int timeout);
int nexx_mbxempty(nexx_contextt *context, uint16 slave, int timeout);
int nexx_mbxsend(nexx_contextt *context, uint16 slave,nex_mbxbuft *mbx, int timeout);
int nexx_mbxreceive(nexx_contextt *context, uint16 slave, nex_mbxbuft *mbx, int timeout);
void nexx_esidump(nexx_contextt *context, uint16 slave, uint8 *esibuf);
uint32 nexx_readeeprom(nexx_contextt *context, uint16 slave, uint16 eeproma, int timeout);
int nexx_writeeeprom(nexx_contextt *context, uint16 slave, uint16 eeproma, uint16 data, int timeout);
int nexx_eeprom2master(nexx_contextt *context, uint16 slave);
int nexx_eeprom2pdi(nexx_contextt *context, uint16 slave);
uint64 nexx_readeepromAP(nexx_contextt *context, uint16 aiadr, uint16 eeproma, int timeout);
int nexx_writeeepromAP(nexx_contextt *context, uint16 aiadr, uint16 eeproma, uint16 data, int timeout);
uint64 nexx_readeepromFP(nexx_contextt *context, uint16 configadr, uint16 eeproma, int timeout);
int nexx_writeeepromFP(nexx_contextt *context, uint16 configadr, uint16 eeproma, uint16 data, int timeout);
void nexx_readeeprom1(nexx_contextt *context, uint16 slave, uint16 eeproma);
uint32 nexx_readeeprom2(nexx_contextt *context, uint16 slave, int timeout);
int nexx_send_overlap_processdata_group(nexx_contextt *context, uint8 group);
int nexx_receive_processdata_group(nexx_contextt *context, uint8 group, int timeout);
int nexx_send_processdata(nexx_contextt *context);
int nexx_send_overlap_processdata(nexx_contextt *context);
int nexx_receive_processdata(nexx_contextt *context, int timeout);

#ifdef __cplusplus
}
#endif

#endif
