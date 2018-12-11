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
#define NEX_MAXODLIST   1024

/** max entries in Object Entry list */
#define NEX_MAXOELIST   256

/* Storage for object description list */
typedef struct
{
   /** slave number */
   uint16  Slave;
   /** number of entries in list */
   uint16  Entries;
   /** array of indexes */
   uint16  Index[NEX_MAXODLIST];
   /** array of datatypes, see EtherCAT specification */
   uint16  DataType[NEX_MAXODLIST];
   /** array of object codes, see EtherCAT specification */
   uint8   ObjectCode[NEX_MAXODLIST];
   /** number of subindexes for each index */
   uint8   MaxSub[NEX_MAXODLIST];
   /** textual description of each index */
   char    Name[NEX_MAXODLIST][NEX_MAXNAME+1];
} nex_ODlistt;

/* storage for object list entry information */
typedef struct
{
   /** number of entries in list */
   uint16 Entries;
   /** array of value infos, see EtherCAT specification */
   uint8  ValueInfo[NEX_MAXOELIST];
   /** array of value infos, see EtherCAT specification */
   uint16 DataType[NEX_MAXOELIST];
   /** array of bit lengths, see EtherCAT specification */
   uint16 BitLength[NEX_MAXOELIST];
   /** array of object access bits, see EtherCAT specification */
   uint16 ObjAccess[NEX_MAXOELIST];
   /** textual description of each index */
   char   Name[NEX_MAXOELIST][NEX_MAXNAME+1];
} nex_OElistt;

#ifdef NEX_VER1
void nex_SDOerror(uint16 Slave, uint16 Index, uint8 SubIdx, int32 AbortCode);
int nex_SDOread(uint16 slave, uint16 index, uint8 subindex,
                      boolean CA, int *psize, void *p, int timeout);
int nex_SDOwrite(uint16 Slave, uint16 Index, uint8 SubIndex,
    boolean CA, int psize, void *p, int Timeout);
int nex_RxPDO(uint16 Slave, uint16 RxPDOnumber , int psize, void *p);
int nex_TxPDO(uint16 slave, uint16 TxPDOnumber , int *psize, void *p, int timeout);
int nex_readPDOmap(uint16 Slave, int *Osize, int *Isize);
int nex_readPDOmapCA(uint16 Slave, int Thread_n, int *Osize, int *Isize);
int nex_readODlist(uint16 Slave, nex_ODlistt *pODlist);
int nex_readODdescription(uint16 Item, nex_ODlistt *pODlist);
int nex_readOEsingle(uint16 Item, uint8 SubI, nex_ODlistt *pODlist, nex_OElistt *pOElist);
int nex_readOE(uint16 Item, nex_ODlistt *pODlist, nex_OElistt *pOElist);
#endif

void nexx_SDOerror(nexx_contextt *context, uint16 Slave, uint16 Index, uint8 SubIdx, int32 AbortCode);
int nexx_SDOread(nexx_contextt *context, uint16 slave, uint16 index, uint8 subindex,
                      boolean CA, int *psize, void *p, int timeout);
int nexx_SDOwrite(nexx_contextt *context, uint16 Slave, uint16 Index, uint8 SubIndex,
    boolean CA, int psize, void *p, int Timeout);
int nexx_RxPDO(nexx_contextt *context, uint16 Slave, uint16 RxPDOnumber , int psize, void *p);
int nexx_TxPDO(nexx_contextt *context, uint16 slave, uint16 TxPDOnumber , int *psize, void *p, int timeout);
int nexx_readPDOmap(nexx_contextt *context, uint16 Slave, int *Osize, int *Isize);
int nexx_readPDOmapCA(nexx_contextt *context, uint16 Slave, int Thread_n, int *Osize, int *Isize);
int nexx_readODlist(nexx_contextt *context, uint16 Slave, nex_ODlistt *pODlist);
int nexx_readODdescription(nexx_contextt *context, uint16 Item, nex_ODlistt *pODlist);
int nexx_readOEsingle(nexx_contextt *context, uint16 Item, uint8 SubI, nex_ODlistt *pODlist, nex_OElistt *pOElist);
int nexx_readOE(nexx_contextt *context, uint16 Item, nex_ODlistt *pODlist, nex_OElistt *pOElist);

#ifdef __cplusplus
}
#endif

#endif
