/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

/** \file
 * \brief
 * Headerfile for ethercatcoe.c
 */

#ifndef _ethercatcoe_
#define _ethercatcoe_

#ifdef __cplusplus
extern "C"
{
#endif

/** max entries in Object Description list */
#define Nex_MAXODLIST   1024

/** max entries in Object Entry list */
#define Nex_MAXOELIST   256

/* Storage for object description list */
typedef struct
{
   /** slave number */
   uint16  Slave;
   /** number of entries in list */
   uint16  Entries;
   /** array of indexes */
   uint16  Index[Nex_MAXODLIST];
   /** array of datatypes, see EtherCAT specification */
   uint16  DataType[Nex_MAXODLIST];
   /** array of object codes, see EtherCAT specification */
   uint8   ObjectCode[Nex_MAXODLIST];
   /** number of subindexes for each index */
   uint8   MaxSub[Nex_MAXODLIST];
   /** textual description of each index */
   char    Name[Nex_MAXODLIST][Nex_MAXNAME+1];
} Nex_ODlistt;

/* storage for object list entry information */
typedef struct
{
   /** number of entries in list */
   uint16 Entries;
   /** array of value infos, see EtherCAT specification */
   uint8  ValueInfo[Nex_MAXOELIST];
   /** array of value infos, see EtherCAT specification */
   uint16 DataType[Nex_MAXOELIST];
   /** array of bit lengths, see EtherCAT specification */
   uint16 BitLength[Nex_MAXOELIST];
   /** array of object access bits, see EtherCAT specification */
   uint16 ObjAccess[Nex_MAXOELIST];
   /** textual description of each index */
   char   Name[Nex_MAXOELIST][Nex_MAXNAME+1];
} Nex_OElistt;


void Nex_SDOerror(uint16 Slave, uint16 Index, uint8 SubIdx, int32 AbortCode);
int Nex_SDOread(uint16 slave, uint16 index, uint8 subindex,
                      boolean CA, int *psize, void *p, int timeout);
int Nex_SDOwrite(uint16 Slave, uint16 Index, uint8 SubIndex,
    boolean CA, int psize, void *p, int Timeout);
int Nex_RxPDO(uint16 Slave, uint16 RxPDOnumber , int psize, void *p);
int Nex_TxPDO(uint16 slave, uint16 TxPDOnumber , int *psize, void *p, int timeout);
int Nex_readPDOmap(uint16 Slave, int *Osize, int *Isize);
int Nex_readPDOmapCA(uint16 Slave, int Thread_n, int *Osize, int *Isize);
int Nex_readODlist(uint16 Slave, Nex_ODlistt *pODlist);
int Nex_readODdescription(uint16 Item, Nex_ODlistt *pODlist);
int Nex_readOEsingle(uint16 Item, uint8 SubI, Nex_ODlistt *pODlist, Nex_OElistt *pOElist);
int Nex_readOE(uint16 Item, Nex_ODlistt *pODlist, Nex_OElistt *pOElist);


void Nexx__SDOerror(Nexx__contextt *context, uint16 Slave, uint16 Index, uint8 SubIdx, int32 AbortCode);
int Nexx__SDOread(Nexx__contextt *context, uint16 slave, uint16 index, uint8 subindex,
                      boolean CA, int *psize, void *p, int timeout);
int Nexx__SDOwrite(Nexx__contextt *context, uint16 Slave, uint16 Index, uint8 SubIndex,
    boolean CA, int psize, void *p, int Timeout);
int Nexx__RxPDO(Nexx__contextt *context, uint16 Slave, uint16 RxPDOnumber , int psize, void *p);
int Nexx__TxPDO(Nexx__contextt *context, uint16 slave, uint16 TxPDOnumber , int *psize, void *p, int timeout);
int Nexx__readPDOmap(Nexx__contextt *context, uint16 Slave, int *Osize, int *Isize);
int Nexx__readPDOmapCA(Nexx__contextt *context, uint16 Slave, int Thread_n, int *Osize, int *Isize);
int Nexx__readODlist(Nexx__contextt *context, uint16 Slave, Nex_ODlistt *pODlist);
int Nexx__readODdescription(Nexx__contextt *context, uint16 Item, Nex_ODlistt *pODlist);
int Nexx__readOEsingle(Nexx__contextt *context, uint16 Item, uint8 SubI, Nex_ODlistt *pODlist, Nex_OElistt *pOElist);
int Nexx__readOE(Nexx__contextt *context, uint16 Item, Nex_ODlistt *pODlist, Nex_OElistt *pOElist);

#ifdef __cplusplus
}
#endif

#endif
