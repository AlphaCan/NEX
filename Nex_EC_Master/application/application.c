#include <stdio.h>
#include <string.h>
#include "application.h"
#include "ethercat.h"


void nex_PDO_Send_Receive(int wkc)
{
	nex_send_processdata();
	wkc = nex_receive_processdata(NEX_TIMEOUTRET);
}


int nex_MasterPDOmapping(uint16 slave)
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
	u32val = 0x607A0020;//targetposition
	retval += nex_SDOwrite(slave, SM2_PDO_OUT, 0x02, FALSE, sizeof(u32val), &u32val, NEX_TIMEOUTRXM);
	u32val = 0x60B80010;//touchproble
	retval += nex_SDOwrite(slave, SM2_PDO_OUT, 0x03, FALSE, sizeof(u32val), &u32val, NEX_TIMEOUTRXM);
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

	while (EcatError) debug_PRINT("%s", nex_elist2string());

	debug_PRINT("have %d slave set, retval = %d\n", slave, retval);
	return 0;
}


int nex_MasterInit(char *ifname)
{
	int slc;
	if (nex_init(ifname))
	{
		debug_PRINT("nex_init succeeded\n");
		if (nex_config_init())
		{
			debug_PRINT("%d slaves found and configured.\n", nex_slavecount);
			if ((nex_slavecount >= 1))
			{
				for (slc = 1; slc <= nex_slavecount; slc++)
				{
					// Copley Controls EAP, using nex_slave[].name is not very reliable
					debug_PRINT("Found %s at position %d\n", nex_slave[slc].name, slc);
					// link slave specific setup to preop->safeop hook
					nex_slave[slc].PO2SOconfig = &nex_MasterPDOmapping;
				}
			}
		}
		else
		{
			debug_PRINT("No slaves found!\n");
			return 2;
		}
	}
	else
	{
		debug_PRINT("nex_init failed\n");
		return 1;
	}
	debug_PRINT("master init succeeded\n");
	return	0;
}

int nex_RequestStatus(uint16 reqstate)
{
	nex_slave[0].state = reqstate;
	nex_writestate(0);
	nex_statecheck(0, reqstate, 50000);
	if (nex_slave[0].state != reqstate)
	{
		debug_PRINT("status request failed,current status:%d\n", nex_slave[0].state);
		return 1;
	}
	else
	{
		debug_PRINT("status request succeeded,current status : %d\n", nex_slave[0].state);
		return 0;
	}
}


int nex_MasterCheck(boolean inOP, int wkc, int expectedWKC)
{
	int slave;
	uint8 currentgroup = 0;
	if (inOP && ((wkc < expectedWKC) || nex_group[currentgroup].docheckstate))
	{
		// one ore more slaves are not responding 
		nex_group[currentgroup].docheckstate = FALSE;
		nex_readstate();
		for (slave = 1; slave <= nex_slavecount; slave++)
		{
			if ((nex_slave[slave].group == currentgroup) && (nex_slave[slave].state != NEX_STATE_OPERATIONAL))
			{
				nex_group[currentgroup].docheckstate = TRUE;
				if (nex_slave[slave].state == (NEX_STATE_SAFE_OP + NEX_STATE_ERROR))
				{
					debug_PRINT("ERROR : slave %d is in SAFE_OP + ERROR, attempting ack.\n", slave);
					nex_slave[slave].state = (NEX_STATE_SAFE_OP + NEX_STATE_ACK);
					nex_writestate(slave);
				}
				else if (nex_slave[slave].state == NEX_STATE_SAFE_OP)
				{
					debug_PRINT("WARNING : slave %d is in SAFE_OP, change to OPERATIONAL.\n", slave);
					nex_slave[slave].state = NEX_STATE_OPERATIONAL;
					nex_writestate(slave);
				}
				else if (nex_slave[slave].state > NEX_STATE_NONE)
				{
					if (nex_reconfig_slave(slave, 500))//timeout 500
					{
						nex_slave[slave].islost = FALSE;
						debug_PRINT("MESSAGE : slave %d reconfigured\n", slave);
					}
				}
				else if (!nex_slave[slave].islost)
				{
					// re-check state 
					nex_statecheck(slave, NEX_STATE_OPERATIONAL, NEX_TIMEOUTRET);
					if (nex_slave[slave].state == NEX_STATE_NONE)
					{
						nex_slave[slave].islost = TRUE;
						debug_PRINT("ERROR : slave %d lost\n", slave);
					}
				}
			}
			if (nex_slave[slave].islost)
			{
				if (nex_slave[slave].state == NEX_STATE_NONE)
				{
					if (nex_recover_slave(slave, 500))//timeout 500
					{
						nex_slave[slave].islost = FALSE;
						debug_PRINT("MESSAGE : slave %d recovered\n", slave);
					}
				}
				else
				{
					nex_slave[slave].islost = FALSE;
					debug_PRINT("MESSAGE : slave %d found\n", slave);
				}
			}
		}
		if (!nex_group[currentgroup].docheckstate)
			debug_PRINT("OK : all slaves resumed OPERATIONAL.\n");
	}
	return 0;
}









