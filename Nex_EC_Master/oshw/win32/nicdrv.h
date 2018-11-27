/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

/** \file
 * \brief
 * Headerfile for nicdrv.c
 */

#ifndef _nicdrvh_
#define _nicdrvh_

#ifdef __cplusplus
extern "C"
{
#endif

#define HAVE_REMOTE

#include <pcap.h>
#include <Packet32.h>

/** pointer structure to Tx and Rx stacks */
typedef struct
{
   /** socket connection used */
   pcap_t      **sock;
   /** tx buffer */
   Nex_bufT     (*txbuf)[Nex_MAXBUF];
   /** tx buffer lengths */
   int         (*txbuflength)[Nex_MAXBUF];
   /** temporary receive buffer */
   Nex_bufT     *tempbuf;
   /** rx buffers */
   Nex_bufT     (*rxbuf)[Nex_MAXBUF];
   /** rx buffer status fields */
   int         (*rxbufstat)[Nex_MAXBUF];
   /** received MAC source address (middle word) */
   int         (*rxsa)[Nex_MAXBUF];
} Nex_stackT;

/** pointer structure to buffers for redundant port */
typedef struct
{
   Nex_stackT   stack;
   pcap_t      *sockhandle;
   /** rx buffers */
   Nex_bufT rxbuf[Nex_MAXBUF];
   /** rx buffer status */
   int rxbufstat[Nex_MAXBUF];
   /** rx MAC source address */
   int rxsa[Nex_MAXBUF];
   /** temporary rx buffer */
   Nex_bufT tempinbuf;
} Nexx__redportt;

/** pointer structure to buffers, vars and mutexes for port instantiation */
typedef struct
{
   Nex_stackT   stack;
   pcap_t      *sockhandle;
   /** rx buffers */
   Nex_bufT rxbuf[Nex_MAXBUF];
   /** rx buffer status */
   int rxbufstat[Nex_MAXBUF];
   /** rx MAC source address */
   int rxsa[Nex_MAXBUF];
   /** temporary rx buffer */
   Nex_bufT tempinbuf;
   /** temporary rx buffer status */
   int tempinbufs;
   /** transmit buffers */
   Nex_bufT txbuf[Nex_MAXBUF];
   /** transmit buffer lenghts */
   int txbuflength[Nex_MAXBUF];
   /** temporary tx buffer */
   Nex_bufT txbuf2;
   /** temporary tx buffer length */
   int txbuflength2;
   /** last used frame index */
   int lastidx;
   /** current redundancy state */
   int redstate;
   /** pointer to redundancy port and buffers */
   Nexx__redportt *redport;
   CRITICAL_SECTION getindex_mutex;
   CRITICAL_SECTION tx_mutex;
   CRITICAL_SECTION rx_mutex;
} Nexx__portt;

extern const uint16 priMAC[3];
extern const uint16 secMAC[3];


extern Nexx__portt     Nexx__port;
extern Nexx__redportt  Nexx__redport;

int Nex_setupnic(const char * ifname, int secondary);
int Nex_closenic(void);
void Nex_setbufstat(int idx, int bufstat);
int Nex_getindex(void);
int Nex_outframe(int idx, int sock);
int Nex_outframe_red(int idx);
int Nex_waitinframe(int idx, int timeout);
int Nex_srconfirm(int idx,int timeout);


void Nex_setupheader(void *p);
int Nexx__setupnic(Nexx__portt *port, const char * ifname, int secondary);
int Nexx__closenic(Nexx__portt *port);
void Nexx__setbufstat(Nexx__portt *port, int idx, int bufstat);
int Nexx__getindex(Nexx__portt *port);
int Nexx__outframe(Nexx__portt *port, int idx, int sock);
int Nexx__outframe_red(Nexx__portt *port, int idx);
int Nexx__waitinframe(Nexx__portt *port, int idx, int timeout);
int Nexx__srconfirm(Nexx__portt *port, int idx,int timeout);

#ifdef __cplusplus
}
#endif

#endif
