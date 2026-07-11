/*
 * Local override of StarterWare's board.c / eeprom.c EEPROM logic.
 *
 * Antminer L3+ uses AM3352 with a BeagleBone-Black-like form factor,
 * but its board has no AT24 EEPROM on I2C0 at the standard address,
 * so reading it would hang or return garbage. Stub all board-info
 * functions and avoid touching the I2C bus.
 */

#include "board.h"

static char gBoardName[]    = "Antminer L3+";
static char gBoardVersion[] = "1.0";
static unsigned int gBoardId = BOARD_UNKNOWN;

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

void BoardInfoRead(unsigned char *data)
{
    /* No EEPROM on Antminer L3+. Fill with zeros so any consumer that
     * still runs sees an empty (and safe) record. */
    unsigned int i;
    for (i = 0; i < MAX_DATA; i++)
    {
        data[i] = 0;
    }
}

unsigned int BoardInfoInit(void)
{
    return gBoardId;
}