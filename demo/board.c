/*
 * Local override of StarterWare's board.c EEPROM logic.
 *
 * Antminer L3+ uses AM3352 with a BeagleBone-Black-like form factor,
 * but its board has no AT24 EEPROM on I2C0 at the standard address,
 * so reading it would hang or return garbage. Stub all board-info
 * functions and avoid touching the I2C bus.
 */

#include "board.h"
#include "string.h"

static char gBoardName[]    = "Antminer L3+";
static char gBoardVersion[] = "1.0";
static unsigned int gBoardId = 0xFFFFFFFFu;

unsigned char *BoardVersionGet(void)
{
    return (unsigned char *)gBoardVersion;
}

unsigned char *BoardNameGet(void)
{
    return (unsigned char *)gBoardName;
}

unsigned int BoardIdGet(void)
{
    return gBoardId;
}

unsigned int BoardInfoInit(void)
{
    return gBoardId;
}
