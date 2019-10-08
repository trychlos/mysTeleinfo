#ifndef __EEPROM_H__
#define __EEPROM_H__

#include <Arduino.h>

/* **************************************************************************************
 *  EEPROM description
 *  This is the data structure saved in the EEPROM.
 * 
 *  MySensors leave us with 256 bytes to save configuration data in the EEPROM.
 * 
 * pwi 2019- 6- 1 v1 creation
 */
#define EEPROM_VERSION    2

typedef uint8_t pEepromRead( uint8_t );
typedef void    pEepromWrite( uint8_t, uint8_t );

typedef struct {
    /* a 'PWI' null-terminated string which marks the structure as initialized */
    char          mark[4];
    uint8_t       version;
    /* communication */
    unsigned long min_period_ms;
    unsigned long max_period_ms;
    bool          dup_thread;
    unsigned long auto_dump_ms;
    unsigned long dup_ms;
}
  sEeprom;

void eepromDump( sEeprom &data );
void eepromRead( sEeprom &data, pEepromRead pfnRead, pEepromWrite pfnWrite );
void eepromReset( sEeprom &data, pEepromWrite pfn );
void eepromWrite( sEeprom &data, pEepromWrite pfn );

#endif // __EEPROM_H__

