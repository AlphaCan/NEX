
#include <stdio.h>
#include <string.h>


#include "osal.h"
#include "ethercat.h"

#define Nex_TIMEOUTMON 500

char IOmap[4096];
OSAL_THREAD_HANDLE thread1;
int expectedWKC;
boolean needlf;
volatile int wkc;
volatile int rtcnt;
boolean inOP;
uint8 currentgroup = 0;

/* most basic RT thread for process data, just does IO transfer */
void CALLBACK RTthread(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1,  DWORD_PTR dw2)
{
    IOmap[0]++;

    Nex_send_processdata();
    wkc = Nex_receive_processdata(Nex_TIMEOUTRET);
    rtcnt++;
    /* do RT control stuff here */
}



int DM3E556(uint16 slave)
{
    int retval;
    uint8 u8val;
    uint16 u16val;

    retval = 0;

    u8val = 0;
    retval += Nex_SDOwrite(slave, 0x1c12, 0x00, FALSE, sizeof(u8val), &u8val, Nex_TIMEOUTRXM);
    u16val = 0x1603;
    retval += Nex_SDOwrite(slave, 0x1c12, 0x01, FALSE, sizeof(u16val), &u16val, Nex_TIMEOUTRXM);
    u8val = 1;
    retval += Nex_SDOwrite(slave, 0x1c12, 0x00, FALSE, sizeof(u8val), &u8val, Nex_TIMEOUTRXM);

    u8val = 0;
    retval += Nex_SDOwrite(slave, 0x1c13, 0x00, FALSE, sizeof(u8val), &u8val, Nex_TIMEOUTRXM);
    u16val = 0x1a03;
    retval += Nex_SDOwrite(slave, 0x1c13, 0x01, FALSE, sizeof(u16val), &u16val, Nex_TIMEOUTRXM);
    u8val = 1;
    retval += Nex_SDOwrite(slave, 0x1c13, 0x00, FALSE, sizeof(u8val), &u8val, Nex_TIMEOUTRXM);

    u8val = 8;
    retval += Nex_SDOwrite(slave, 0x6060, 0x00, FALSE, sizeof(u8val), &u8val, Nex_TIMEOUTRXM);

 

    while(EcatError) printf("%s", Nex_elist2string());

    printf("DM3E556 slave %d set, retval = %d\n", slave, retval);
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
   if (Nex_init(ifname))
   {
      printf("Nex_init on %s succeeded.\n",ifname);
      /* find and auto-config slaves */


       if ( Nex_config_init() > 0 )
       {
         printf("%d slaves found and configured.\n",Nex_slavecount);

         if((Nex_slavecount >= 1))
         {
             for(slc = 1; slc <= Nex_slavecount; slc++)
             {
                 
                 // Copley Controls EAP, using Nex_slave[].name is not very reliable
                 if((Nex_slave[slc].eep_man == 0x00004321) && (Nex_slave[slc].eep_id == 0x00008100))
                 {
                     printf("Found %s at position %d\n", Nex_slave[slc].name, slc);
                     // link slave specific setup to preop->safeop hook
                     Nex_slave[slc].PO2SOconfig = &DM3E556;
                 }
             }
         }


         Nex_config_map(&IOmap);

         Nex_configdc();

         printf("Slaves mapped, state to SAFE_OP.\n");
         /* wait for all slaves to reach SAFE_OP state */
         Nex_statecheck(0, Nex_STATE_SAFE_OP,  Nex_TIMEOUTSTATE * 4);

         oloop = Nex_slave[0].Obytes;
         if ((oloop == 0) && (Nex_slave[0].Obits > 0)) oloop = 1;
         if (oloop > 8) oloop = 8;
         iloop = Nex_slave[0].Ibytes;
         if ((iloop == 0) && (Nex_slave[0].Ibits > 0)) iloop = 1;
         if (iloop > 8) iloop = 8;

         printf("segments : %d : %d %d %d %d\n",Nex_group[0].nsegments ,Nex_group[0].IOsegment[0],Nex_group[0].IOsegment[1],Nex_group[0].IOsegment[2],Nex_group[0].IOsegment[3]);

         printf("Request operational state for all slaves\n");
         expectedWKC = (Nex_group[0].outputsWKC * 2) + Nex_group[0].inputsWKC;
         printf("Calculated workcounter %d\n", expectedWKC);
         Nex_slave[0].state = Nex_STATE_OPERATIONAL;
         /* send one valid process data to make outputs in slaves happy*/
         Nex_send_processdata();
         Nex_receive_processdata(Nex_TIMEOUTRET);

         /* start RT thread as periodic MM timer */
         mmResult = timeSetEvent(1, 0, RTthread, 0, TIME_PERIODIC);

         /* request OP state for all slaves */
         Nex_writestate(0);
         chk = 40;
         /* wait for all slaves to reach OP state */
         do
         {
            Nex_statecheck(0, Nex_STATE_OPERATIONAL, 50000);
         }
         while (chk-- && (Nex_slave[0].state != Nex_STATE_OPERATIONAL));
         if (Nex_slave[0].state == Nex_STATE_OPERATIONAL )
         {
            printf("Operational state reached for all slaves.\n");
            wkc_count = 0;
            inOP = TRUE;


            /* cyclic loop, reads data from RT thread */
            for(i = 1; i <= 500; i++)
            {
                    if(wkc >= expectedWKC)
                    {
                        printf("Processdata cycle %4d, WKC %d , O:", rtcnt, wkc);

                        for(j = 0 ; j < oloop; j++)
                        {
                            printf(" %2.2x", *(Nex_slave[0].outputs + j));
                        }

                        printf(" I:");
                        for(j = 0 ; j < iloop; j++)
                        {
                            printf(" %2.2x", *(Nex_slave[0].inputs + j));
                        }
                        printf(" T:%lld\r",Nex_DCtime);
                        needlf = TRUE;
                    }
                    osal_usleep(50000);

            }
            inOP = FALSE;
         }
         else
         {
                printf("Not all slaves reached operational state.\n");
                Nex_readstate();
                for(i = 1; i<=Nex_slavecount ; i++)
                {
                    if(Nex_slave[i].state != Nex_STATE_OPERATIONAL)
                    {
                        printf("Slave %d State=0x%2.2x StatusCode=0x%4.4x : %s\n",
                            i, Nex_slave[i].state, Nex_slave[i].ALstatuscode, Nex_ALstatuscode2string(Nex_slave[i].ALstatuscode));
                    }
                }
         }

         /* stop RT thread */
         timeKillEvent(mmResult);

         printf("\nRequest init state for all slaves\n");
         Nex_slave[0].state = Nex_STATE_INIT;
         /* request INIT state for all slaves */
         Nex_writestate(0);
        }
        else
        {
            printf("No slaves found!\n");
        }
        printf("End simple test, close socket\n");
        /* stop SOEM, close socket */
        Nex_close();
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
        if( inOP && ((wkc < expectedWKC) || Nex_group[currentgroup].docheckstate))
        {
            if (needlf)
            {
               needlf = FALSE;
               printf("\n");
            }
            /* one ore more slaves are not responding */
            Nex_group[currentgroup].docheckstate = FALSE;
            Nex_readstate();
            for (slave = 1; slave <= Nex_slavecount; slave++)
            {
               if ((Nex_slave[slave].group == currentgroup) && (Nex_slave[slave].state != Nex_STATE_OPERATIONAL))
               {
                  Nex_group[currentgroup].docheckstate = TRUE;
                  if (Nex_slave[slave].state == (Nex_STATE_SAFE_OP + Nex_STATE_ERROR))
                  {
                     printf("ERROR : slave %d is in SAFE_OP + ERROR, attempting ack.\n", slave);
                     Nex_slave[slave].state = (Nex_STATE_SAFE_OP + Nex_STATE_ACK);
                     Nex_writestate(slave);
                  }
                  else if(Nex_slave[slave].state == Nex_STATE_SAFE_OP)
                  {
                     printf("WARNING : slave %d is in SAFE_OP, change to OPERATIONAL.\n", slave);
                     Nex_slave[slave].state = Nex_STATE_OPERATIONAL;
                     Nex_writestate(slave);
                  }
                  else if(Nex_slave[slave].state > Nex_STATE_NONE)
                  {
                     if (Nex_reconfig_slave(slave, Nex_TIMEOUTMON))
                     {
                        Nex_slave[slave].islost = FALSE;
                        printf("MESSAGE : slave %d reconfigured\n",slave);
                     }
                  }
                  else if(!Nex_slave[slave].islost)
                  {
                     /* re-check state */
                     Nex_statecheck(slave, Nex_STATE_OPERATIONAL, Nex_TIMEOUTRET);
                     if (Nex_slave[slave].state == Nex_STATE_NONE)
                     {
                        Nex_slave[slave].islost = TRUE;
                        printf("ERROR : slave %d lost\n",slave);
                     }
                  }
               }
               if (Nex_slave[slave].islost)
               {
                  if(Nex_slave[slave].state == Nex_STATE_NONE)
                  {
                     if (Nex_recover_slave(slave, Nex_TIMEOUTMON))
                     {
                        Nex_slave[slave].islost = FALSE;
                        printf("MESSAGE : slave %d recovered\n",slave);
                     }
                  }
                  else
                  {
                     Nex_slave[slave].islost = FALSE;
                     printf("MESSAGE : slave %d found\n",slave);
                  }
               }
            }
            if(!Nex_group[currentgroup].docheckstate)
               printf("OK : all slaves resumed OPERATIONAL.\n");
        }
        osal_usleep(10000);
    }

//    return 0;
}


char* Getifname(pcap_if_t *alldevs,char inum)
{
	pcap_if_t *d;
	int i_open = 0;
	for (d = alldevs, i_open = 0; i_open < inum - 1; d = d->next, i_open++);
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

	Nex_adaptert * adapter = NULL;
	printf("EtherCAT Master Simple Test\n");

	if (1)
	{
		/* create thread to handle slave error handling in OP */
		osal_thread_create(&thread1, 128000, &ecatcheck, (void*)&ctime);
	
		mastersetup(Getifname(alldevs,4));
	}
	else
	{
		printf("Usage: simple_test ifname1\n");
		/* Print the list */
		printf("Available adapters\n");
		adapter = Nex_find_adapters();
		while (adapter != NULL)
		{
			printf("Description : %s, Device to use for wpcap: %s\n", adapter->desc, adapter->name);
			adapter = adapter->next;
		}
	}

	printf("End program\n");
	return (0);
}
