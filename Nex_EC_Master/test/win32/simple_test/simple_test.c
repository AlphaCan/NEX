/** \file
 * \brief Example code for Simple Open EtherCAT master
 *
 * Usage : simple_test [ifname1]
 * ifname is NIC interface, f.e. eth0
 *
 * This is a minimal test.
 *
 * (c)Arthur Ketels 2010 - 2011
 */

#include <stdio.h>
#include <string.h>
//#include <Mmsystem.h>

#include "osal.h"
#include "ethercat.h"

#define NEX_TIMEOUTMON 500

char IOmap[64];
OSAL_THREAD_HANDLE thread1;
int expectedWKC;
boolean needlf;
volatile int wkc;
volatile int rtcnt;
boolean inOP;
uint8 currentgroup = 0;
drivercontrol_t* DriverControl;
driverstatus_t*   DriverStatus;

drivercontrol_t* DriverControl = (drivercontrol_t*)(IOmap);
driverstatus_t*   DriverStatus = (driverstatus_t*)(IOmap + 8);

/* most basic RT thread for process data, just does IO transfer */
void CALLBACK RTthread(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1,  DWORD_PTR dw2)
{
 //   IOmap[0]++;
	
	
    nex_send_processdata();
    wkc = nex_receive_processdata(NEX_TIMEOUTRET);
    rtcnt++;
    /* do RT control stuff here */

    DriverControl->TargetPosition = DriverStatus->ActualPosition + 500;

}


int DM3E556(uint16 slave)
{
	int retval;
	uint8 u8val;
	uint16 u16val;
	uint32 u32val;
	uint16 SM2_PDO_OUT = 0x1600;
	uint16 SM3_PDO_INPUT = 0x1A00;
	retval = 0;

	u8val = 0;
	retval += nex_SDOwrite(slave, SM2_PDO_OUT, 0x00, FALSE, sizeof(u8val), &u8val, NEX_TIMEOUTRXM);
	u32val = 0x60400010;//keywords
	retval += nex_SDOwrite(slave, SM2_PDO_OUT, 0x01, FALSE, sizeof(u32val), &u32val, NEX_TIMEOUTRXM);
	u32val = 0x607A0020;
	retval += nex_SDOwrite(slave, SM2_PDO_OUT, 0x02, FALSE, sizeof(u32val), &u32val, NEX_TIMEOUTRXM);
	u32val = 0x60B80010;
	retval += nex_SDOwrite(slave, SM2_PDO_OUT, 0x03, FALSE, sizeof(u32val), &u32val, NEX_TIMEOUTRXM);
//	u32val = 0x60710010;//target Torque
//	retval += nex_SDOwrite(slave, SM2_PDO_OUT, 0x04, FALSE, sizeof(u32val), &u32val, NEX_TIMEOUTRXM);
	u8val = 3;//
	retval += nex_SDOwrite(slave, SM2_PDO_OUT, 0x00, FALSE, sizeof(u8val), &u8val, NEX_TIMEOUTRXM);

	u8val = 0;
	retval += nex_SDOwrite(slave, 0x1c12, 0x00, FALSE, sizeof(u8val), &u8val, NEX_TIMEOUTRXM);
	u16val = SM2_PDO_OUT;//
	retval += nex_SDOwrite(slave, 0x1c12, 0x01, FALSE, sizeof(u16val), &u16val, NEX_TIMEOUTRXM);
	u8val = 1;
	retval += nex_SDOwrite(slave, 0x1c12, 0x00, FALSE, sizeof(u8val), &u8val, NEX_TIMEOUTRXM);

	u8val = 0;
	retval += nex_SDOwrite(slave, SM3_PDO_INPUT, 0x00, FALSE, sizeof(u8val), &u8val, NEX_TIMEOUTRXM);
	u32val = 0x603F0010;
	retval += nex_SDOwrite(slave, SM3_PDO_INPUT, 0x01, FALSE, sizeof(u32val), &u32val, NEX_TIMEOUTRXM);
	u32val = 0x60410010;
	retval += nex_SDOwrite(slave, SM3_PDO_INPUT, 0x02, FALSE, sizeof(u32val), &u32val, NEX_TIMEOUTRXM);
	u32val = 0x60610008;
	retval += nex_SDOwrite(slave, SM3_PDO_INPUT, 0x03, FALSE, sizeof(u32val), &u32val, NEX_TIMEOUTRXM);
	u32val = 0x60640020;
	retval += nex_SDOwrite(slave, SM3_PDO_INPUT, 0x04, FALSE, sizeof(u32val), &u32val, NEX_TIMEOUTRXM);
	u32val = 0x60B90010;
	retval += nex_SDOwrite(slave, SM3_PDO_INPUT, 0x05, FALSE, sizeof(u32val), &u32val, NEX_TIMEOUTRXM);
	u32val = 0x60BA0020;
	retval += nex_SDOwrite(slave, SM3_PDO_INPUT, 0x06, FALSE, sizeof(u32val), &u32val, NEX_TIMEOUTRXM);
	u32val = 0x60FD0020;
	retval += nex_SDOwrite(slave, SM3_PDO_INPUT, 0x07, FALSE, sizeof(u32val), &u32val, NEX_TIMEOUTRXM);
	u8val = 7;
	retval += nex_SDOwrite(slave, SM3_PDO_INPUT, 0x00, FALSE, sizeof(u8val), &u8val, NEX_TIMEOUTRXM);

	u8val = 0;
	retval += nex_SDOwrite(slave, 0x1c13, 0x00, FALSE, sizeof(u8val), &u8val, NEX_TIMEOUTRXM);
	u16val = SM3_PDO_INPUT;//
	retval += nex_SDOwrite(slave, 0x1c13, 0x01, FALSE, sizeof(u16val), &u16val, NEX_TIMEOUTRXM);
	u8val = 1;
	retval += nex_SDOwrite(slave, 0x1c13, 0x00, FALSE, sizeof(u8val), &u8val, NEX_TIMEOUTRXM);

	u8val = 8;//operation model
	retval += nex_SDOwrite(slave, 0x6060, 0x00, FALSE, sizeof(u8val), &u8val, NEX_TIMEOUTRXM);

    while(EcatError) printf("%s", nex_elist2string());

    printf("AEP slave %d set, retval = %d\n", slave, retval);
    return 1;
}

void mastersetup(char *ifname)
{
    int i, j, oloop, iloop, wkc_count, chk, slc;
    UINT mmResult;

    needlf = FALSE;
    inOP = FALSE;

   printf("Starting simple test\n");

   /* initialise SOEM, bind socket to ifname */
   if (nex_init(ifname))
   {
      printf("nex_init on %s succeeded.\n",ifname);
      /* find and auto-config slaves */


       if ( nex_config_init() > 0 )
       {
         printf("%d slaves found and configured.\n",nex_slavecount);

         if((nex_slavecount >= 1))
         {
             for(slc = 1; slc <= nex_slavecount; slc++)
             {
                 
                 // Copley Controls EAP, using nex_slave[].name is not very reliable
                 
                     printf("Found %s at position %d\n", nex_slave[slc].name, slc);
                     // link slave specific setup to preop->safeop hook
                     nex_slave[slc].PO2SOconfig = &DM3E556;
               
             }
         }


         nex_config_map(&IOmap);

         nex_configdc();

         printf("Slaves mapped, state to SAFE_OP.\n");
         /* wait for all slaves to reach SAFE_OP state */
         nex_statecheck(0, NEX_STATE_SAFE_OP,  NEX_TIMEOUTSTATE * 4);

         oloop = nex_slave[0].Obytes;
         if ((oloop == 0) && (nex_slave[0].Obits > 0)) oloop = 1;
         if (oloop > 8) oloop = 8;
         iloop = nex_slave[0].Ibytes;
         if ((iloop == 0) && (nex_slave[0].Ibits > 0)) iloop = 1;
         if (iloop > 8) iloop = 8;

         printf("segments : %d : %d %d %d %d\n",nex_group[0].nsegments ,nex_group[0].IOsegment[0],nex_group[0].IOsegment[1],nex_group[0].IOsegment[2],nex_group[0].IOsegment[3]);

         printf("Request operational state for all slaves\n");
         expectedWKC = (nex_group[0].outputsWKC * 2) + nex_group[0].inputsWKC;
         printf("Calculated workcounter %d\n", expectedWKC);
         nex_slave[0].state = NEX_STATE_OPERATIONAL;
         /* send one valid process data to make outputs in slaves happy*/
         nex_send_processdata();
         nex_receive_processdata(NEX_TIMEOUTRET);

		 DriverControl = (drivercontrol_t*)(nex_slave[0].outputs);
		 DriverStatus = (driverstatus_t*)(nex_slave[0].inputs);

         /* start RT thread as periodic MM timer */
         mmResult = timeSetEvent(4, 0, RTthread, 0, TIME_PERIODIC);

		 /* request OP state for all slaves */
         nex_writestate(0);
         chk = 40;
         /* wait for all slaves to reach OP state */
         do
         {
            nex_statecheck(0, NEX_STATE_OPERATIONAL, 50000);
         }
         while (chk-- && (nex_slave[0].state != NEX_STATE_OPERATIONAL));
         if (nex_slave[0].state == NEX_STATE_OPERATIONAL )
         {
            printf("Operational state reached for all slaves.\n");
            wkc_count = 0;
            inOP = TRUE;

			

			DriverControl->KeyWords = 0x80;
			osal_usleep(8000);

			DriverControl->KeyWords = 0x00;
			osal_usleep(8000);

			DriverControl->KeyWords = 6;
			osal_usleep(8000);
			DriverControl->KeyWords = 7;
			osal_usleep(8000);
			DriverControl->KeyWords = 15;
			osal_usleep(8000);

            /* cyclic loop, reads data from RT thread */
            for(i = 1; i <= 500; i++)
            {
                    if(wkc >= expectedWKC)
                    {
                        printf("Processdata cycle %4d, WKC %d , O:", rtcnt, wkc);

                        for(j = 0 ; j < oloop; j++)
                        {
                            printf(" %2.2x", *(nex_slave[0].outputs + j));
                        }

                        printf(" I:");
                        for(j = 0 ; j < iloop; j++)
                        {
                            printf(" %2.2x", *(nex_slave[0].inputs + j));
                        }
                        printf(" T:%lld\r",nex_DCtime);
                        needlf = TRUE;
                    }
					
                    osal_usleep(50000);

            }
            inOP = FALSE;
         }
         else
         {
                printf("Not all slaves reached operational state.\n");
                nex_readstate();
                for(i = 1; i<=nex_slavecount ; i++)
                {
                    if(nex_slave[i].state != NEX_STATE_OPERATIONAL)
                    {
                        printf("Slave %d State=0x%2.2x StatusCode=0x%4.4x : %s\n",
                            i, nex_slave[i].state, nex_slave[i].ALstatuscode, nex_ALstatuscode2string(nex_slave[i].ALstatuscode));
                    }
                }
         }

         /* stop RT thread */
         timeKillEvent(mmResult);

         printf("\nRequest init state for all slaves\n");
         nex_slave[0].state = NEX_STATE_INIT;
         /* request INIT state for all slaves */
         nex_writestate(0);
        }
        else
        {
            printf("No slaves found!\n");
        }
        printf("End simple test, close socket\n");
        /* stop SOEM, close socket */
        nex_close();
    }
    else
    {
        printf("No socket connection on %s\nExcecute as root\n",ifname);
    }
}

//DWORD WINAPI ecatcheck( LPVOID lpParam )
OSAL_THREAD_FUNC ecatcheck(void *lpParam)
{
    int slave;

    while(1)
    {
        if( inOP && ((wkc < expectedWKC) || nex_group[currentgroup].docheckstate))
        {
            if (needlf)
            {
               needlf = FALSE;
               printf("\n");
            }
            /* one ore more slaves are not responding */
            nex_group[currentgroup].docheckstate = FALSE;
            nex_readstate();
            for (slave = 1; slave <= nex_slavecount; slave++)
            {
               if ((nex_slave[slave].group == currentgroup) && (nex_slave[slave].state != NEX_STATE_OPERATIONAL))
               {
                  nex_group[currentgroup].docheckstate = TRUE;
                  if (nex_slave[slave].state == (NEX_STATE_SAFE_OP + NEX_STATE_ERROR))
                  {
                     printf("ERROR : slave %d is in SAFE_OP + ERROR, attempting ack.\n", slave);
                     nex_slave[slave].state = (NEX_STATE_SAFE_OP + NEX_STATE_ACK);
                     nex_writestate(slave);
                  }
                  else if(nex_slave[slave].state == NEX_STATE_SAFE_OP)
                  {
                     printf("WARNING : slave %d is in SAFE_OP, change to OPERATIONAL.\n", slave);
                     nex_slave[slave].state = NEX_STATE_OPERATIONAL;
                     nex_writestate(slave);
                  }
                  else if(nex_slave[slave].state > NEX_STATE_NONE)
                  {
                     if (nex_reconfig_slave(slave, NEX_TIMEOUTMON))
                     {
                        nex_slave[slave].islost = FALSE;
                        printf("MESSAGE : slave %d reconfigured\n",slave);
                     }
                  }
                  else if(!nex_slave[slave].islost)
                  {
                     /* re-check state */
                     nex_statecheck(slave, NEX_STATE_OPERATIONAL, NEX_TIMEOUTRET);
                     if (nex_slave[slave].state == NEX_STATE_NONE)
                     {
                        nex_slave[slave].islost = TRUE;
                        printf("ERROR : slave %d lost\n",slave);
                     }
                  }
               }
               if (nex_slave[slave].islost)
               {
                  if(nex_slave[slave].state == NEX_STATE_NONE)
                  {
                     if (nex_recover_slave(slave, NEX_TIMEOUTMON))
                     {
                        nex_slave[slave].islost = FALSE;
                        printf("MESSAGE : slave %d recovered\n",slave);
                     }
                  }
                  else
                  {
                     nex_slave[slave].islost = FALSE;
                     printf("MESSAGE : slave %d found\n",slave);
                  }
               }
            }
            if(!nex_group[currentgroup].docheckstate)
               printf("OK : all slaves resumed OPERATIONAL.\n");
        }
        osal_usleep(10000);
    }

//    return 0;
}


char* Getifname(pcap_if_t *alldevs, char inum)
{
	pcap_if_t *d;
	int i_open = 0;
	for (d = alldevs, i_open = 0; i_open < inum - 1; d = d->next, i_open++)
	{
		printf("1\n");
	}
	return d->name;
}



int main(int argc, char *argv[])
{

	pcap_if_t *alldevs;
	char errbuf_open[PCAP_ERRBUF_SIZE];
	/* 获取本机设备列表 */
	if (pcap_findalldevs(&alldevs, errbuf_open) == -1)
	{
		fprintf(stderr, "Error in pcap_findalldevs: %s\n", errbuf_open);
		exit(1);
	}

	nex_adaptert * adapter = NULL;
	printf("EtherCAT Master Simple Test\n");

	if (1)
	{
		/* create thread to handle slave error handling in OP */
		osal_thread_create(&thread1, 128000, &ecatcheck, (void*)&ctime);

		mastersetup(Getifname(alldevs, 1));
	}
	else
	{
		printf("Usage: simple_test ifname1\n");
		/* Print the list */
		printf("Available adapters\n");
		adapter = nex_find_adapters();
		while (adapter != NULL)
		{
			printf("Description : %s, Device to use for wpcap: %s\n", adapter->desc, adapter->name);
			adapter = adapter->next;
		}
	}

	printf("End program\n");
	return (0);
}