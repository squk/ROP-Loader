#include <nds.h>
#include <dswifi7.h>
#include <maxmod7.h>
#include <nds/bios.h>
#include <stdlib.h>

#include "../../fwfifo.h"

static inline void SerialDrainData(void)
{
	u16 wasted = REG_SPIDATA; (void )wasted;
}

u16 spiFwReadSr(void)
{
	int oldIME = enterCriticalSection();
	SerialWaitBusy();
	REG_SPICNT  = SPI_ENABLE | SPI_CONTINUOUS | SPI_DEVICE_FIRMWARE | SPI_BYTE_MODE | SPI_BAUD_4MHz;
	REG_SPIDATA = FIRMWARE_RDSR;
	SerialWaitBusy();
	SerialDrainData();
	SerialWaitBusy();
	REG_SPICNT  = SPI_ENABLE | SPI_DEVICE_FIRMWARE | SPI_BYTE_MODE | SPI_BAUD_4MHz;
	REG_SPIDATA = 0;
	SerialWaitBusy();
	u16 status = REG_SPIDATA;
	leaveCriticalSection(oldIME);
	return status;
}

void spiFwWriteOrProgram(u16 cmd, u32 offset, u8 * buffer, int size)
{

	int oldIME = enterCriticalSection();
	SerialWaitBusy();
	REG_SPICNT  = SPI_ENABLE | SPI_DEVICE_FIRMWARE | SPI_BYTE_MODE | SPI_BAUD_4MHz;
	REG_SPIDATA = FIRMWARE_WREN;
	SerialWaitBusy();

	REG_SPICNT  = SPI_ENABLE | SPI_DEVICE_FIRMWARE | SPI_CONTINUOUS | SPI_BYTE_MODE | SPI_BAUD_4MHz;
	REG_SPIDATA = cmd;
	SerialWaitBusy();

	REG_SPIDATA = (offset>>16) & 0xFF;
	SerialWaitBusy();
	REG_SPIDATA = (offset>>8) & 0xFF;
	SerialWaitBusy();
	REG_SPIDATA = (offset>>0) & 0xFF;
	SerialWaitBusy();


	if ((cmd == FIRMWARE_PP || cmd == FIRMWARE_PW) && (size != 0))
	{
		u32 ctr = 0;
		do
		{
			REG_SPICNT  = (ctr == size-1) ? (SPI_ENABLE|SPI_DEVICE_FIRMWARE | SPI_BYTE_MODE | SPI_BAUD_4MHz) : (SPI_ENABLE | SPI_DEVICE_FIRMWARE | SPI_CONTINUOUS | SPI_BYTE_MODE | SPI_BAUD_4MHz);
			REG_SPIDATA = buffer[ctr];
			SerialWaitBusy();
		} while (++ctr != size);
	}

	while (spiFwReadSr() & 1) ;
	REG_SPICNT = 0;
	leaveCriticalSection(oldIME);
}

void spiFwFoo(u32 cmd)
{
	int oldIME = enterCriticalSection();
	SerialWaitBusy();
	REG_SPICNT  = SPI_ENABLE | SPI_DEVICE_FIRMWARE | SPI_BYTE_MODE | SPI_BAUD_4MHz;
	REG_SPIDATA = FIRMWARE_WREN;
	SerialWaitBusy();
	SerialDrainData();
	SerialWaitBusy();
	REG_SPICNT  = SPI_ENABLE | SPI_DEVICE_FIRMWARE | SPI_CONTINUOUS | SPI_BYTE_MODE | SPI_BAUD_4MHz;
	REG_SPIDATA = 1; /* FIXME - what the!? */
	SerialWaitBusy();
	SerialDrainData();
	SerialWaitBusy();
	REG_SPICNT  = SPI_ENABLE | SPI_DEVICE_FIRMWARE | SPI_BYTE_MODE | SPI_BAUD_4MHz;
	REG_SPIDATA = cmd;
	SerialWaitBusy();
	SerialDrainData();
	leaveCriticalSection(oldIME);
}

void spiFwFifoHandler(int num_bytes, void * userdata)
{
	FwFifoData msg;
	fifoGetDatamsg(FIFO_USER_01, sizeof(msg), (void*)&msg);
	if (msg.page.fifo_cmd == FwRead)
	{
		readFirmware(msg.page.offset, msg.page.buffer, msg.page.size);
		msg.response = 0;
		fifoSendDatamsg(FIFO_USER_01, sizeof(msg), (void*)&msg);
	}
	else if (msg.page.fifo_cmd == FwProgram)
	{
		spiFwWriteOrProgram(msg.page.fifo_cmd, msg.page.offset, msg.page.buffer, msg.page.size);
		msg.response = 0;
		fifoSendDatamsg(FIFO_USER_01, sizeof(msg), (void*)&msg);
	}
	else if (msg.page.fifo_cmd == FwWrite)
	{
		spiFwWriteOrProgram(FIRMWARE_PW, msg.page.offset, msg.page.buffer, msg.page.size);
		msg.response = 0;
		fifoSendDatamsg(FIFO_USER_01, sizeof(msg), (void*)&msg);
	}
	else if (msg.page.fifo_cmd == FwPageErase)
	{
		spiFwWriteOrProgram(FIRMWARE_PE, msg.page.offset, msg.page.buffer, msg.page.size);
		msg.response = 0;
		fifoSendDatamsg(FIFO_USER_01, sizeof(msg), (void*)&msg);
	}
	else if (msg.page.fifo_cmd == FwSectorErase)
	{
		spiFwWriteOrProgram(FIRMWARE_SE, msg.page.offset, msg.page.buffer, msg.page.size);
		msg.response = 0;
		fifoSendDatamsg(FIFO_USER_01, sizeof(msg), (void*)&msg);
	}
	else if (msg.page.fifo_cmd == FwReadSr)
	{
		*msg.page.buffer = spiFwReadSr();
		msg.response = 0;
		fifoSendDatamsg(FIFO_USER_01, sizeof(msg), (void*)&msg);
	}
	else if (msg.page.fifo_cmd == FwFoo)
	{
		spiFwFoo(*msg.page.buffer);
		msg.response = 0;
		fifoSendDatamsg(FIFO_USER_01, sizeof(msg), (void*)&msg);
	}
}

//---------------------------------------------------------------------------------
void VblankHandler(void) {
//---------------------------------------------------------------------------------
	Wifi_Update();
}

//---------------------------------------------------------------------------------
void VcountHandler() {
//---------------------------------------------------------------------------------
	inputGetAndSend();
}

volatile bool exitflag = false;

//---------------------------------------------------------------------------------
void powerButtonCB() {
//---------------------------------------------------------------------------------
	exitflag = true;
}

//---------------------------------------------------------------------------------
int main(int argc, char ** argv)
//---------------------------------------------------------------------------------
{
	readUserSettings();

	irqInit();
	fifoInit();
	initClockIRQ();

	SetYtrigger(80);

	installWifiFIFO();
	installSoundFIFO();

	installSystemFIFO();

	irqSet(IRQ_VCOUNT, VcountHandler);
	irqSet(IRQ_VBLANK, VblankHandler);

	setPowerButtonCB(powerButtonCB);

	fifoSetDatamsgHandler(FIFO_USER_01, spiFwFifoHandler, 0);

	while (!exitflag)
	{
		if ( 0 == (REG_KEYINPUT & (KEY_SELECT | KEY_START | KEY_L | KEY_R)))
		{
			exitflag = true;
		}
		swiIntrWait(1, IRQ_FIFO_NOT_EMPTY | IRQ_VBLANK);
	}

	return EXIT_SUCCESS;
}

