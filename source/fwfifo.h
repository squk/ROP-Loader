#ifndef FWFIFO_H
#define FWFIFO_H

#include <nds.h>

typedef enum {
	FwRead = 1,
	FwProgram,
	FwWrite,
	FwPageErase,
	FwSectorErase,
	FwReadSr,
	FwFoo
} FifoFwCommands;

typedef union fwFifoData
{
	struct fw_page
	{
		u32 fifo_cmd;
		u32 offset;
		u32 size;
		u8* buffer;
	} page;
	u8 cmd;
	u32 response;
} FwFifoData;

#endif /* end of include guard: FWFIFO_H */
