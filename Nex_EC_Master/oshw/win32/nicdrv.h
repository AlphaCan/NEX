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
   nex_bufT     (*txbuf)[NEX_MAXBUF];
   /** tx buffer lengths */
   int         (*txbuflength)[NEX_MAXBUF];
   /** temporary receive buffer */
   nex_bufT     *tempbuf;
   /** rx buffers */
   nex_bufT     (*rxbuf)[NEX_MAXBUF];
   /** rx buffer status fields */
   int         (*rxbufstat)[NEX_MAXBUF];
   /** received MAC source address (middle word) */
   int         (*rxsa)[NEX_MAXBUF];
} nex_stackT;

/** pointer structure to buffers for redundant port */
typedef struct
{
   nex_stackT   stack;
   pcap_t      *sockhandle;
   /** rx buffers */
   nex_bufT rxbuf[NEX_MAXBUF];
   /** rx buffer status */
   int rxbufstat[NEX_MAXBUF];
   /** rx MAC source address */
   int rxsa[NEX_MAXBUF];
   /** temporary rx buffer */
   nex_bufT tempinbuf;
} nexx_redportt;

/** pointer structure to buffers, vars and mutexes for port instantiation */
typedef struct
{
   nex_stackT   stack;
   pcap_t      *sockhandle;
   /** rx buffers */
   nex_bufT rxbuf[NEX_MAXBUF];
   /** rx buffer status */
   int rxbufstat[NEX_MAXBUF];
   /** rx MAC source address */
   int rxsa[NEX_MAXBUF];
   /** temporary rx buffer */
   nex_bufT tempinbuf;
   /** temporary rx buffer status */
   int tempinbufs;
   /** transmit buffers */
   nex_bufT txbuf[NEX_MAXBUF];
   /** transmit buffer lenghts */
   int txbuflength[NEX_MAXBUF];
   /** temporary tx buffer */
   nex_bufT txbuf2;
   /** temporary tx buffer length */
   int txbuflength2;
   /** last used frame index */
   int lastidx;
   /** current redundancy state */
   int redstate;
   /** pointer to redundancy port and buffers */
   nexx_redportt *redport;
   CRITICAL_SECTION getindex_mutex;
   CRITICAL_SECTION tx_mutex;
   CRITICAL_SECTION rx_mutex;
} nexx_portt;

extern const uint16 priMAC[3];
extern const uint16 secMAC[3];

#ifdef NEX_VER1
extern nexx_portt     nexx_port;
extern nexx_redportt  nexx_redport;

int nex_setupnic(const char * ifname, int secondary);
int nex_closenic(void);
void nex_setbufstat(int idx, int bufstat);
int nex_getindex(void);
int nex_outframe(int idx, int sock);
int nex_outframe_red(int idx);
int nex_waitinframe(int idx, int timeout);
int nex_srconfirm(int idx,int timeout);
#endif

void nex_setupheader(void *p);
int nexx_setupnic(nexx_portt *port, const char * ifname, int secondary);
int nexx_closenic(nexx_portt *port);
void nexx_setbufstat(nexx_portt *port, int idx, int bufstat);
int nexx_getindex(nexx_portt *port);
int nexx_outframe(nexx_portt *port, int idx, int sock);
int nexx_outframe_red(nexx_portt *port, int idx);
int nexx_waitinframe(nexx_portt *port, int idx, int timeout);
int nexx_srconfirm(nexx_portt *port, int idx,int timeout);

#ifdef __cplusplus
}
#endif

#endif
