#ifndef __TELEINFO_EEPROM_H__
#define __TELEINFO_EEPROM_H__

/*
 * MySensors let 256  bytes usable
 * 
 * pos  type           size  content
 * ---  -------------  ----  ------------------------------
 *   0  char              4  mark
 *   4  unsigned long     4  read period (ms)
 *   8  unsigned long     4  send max frequency (ms)
 *  12  unsigned long     4  unchanged timeout (ms)
 *  16
 *  
 * Please note that this EEPROM code must be written in main .ino program
 * in order to take advantage of the MySensors library (which does not want
 * to be compiled from other source files).
 */

typedef struct {
    char mark[4];
    unsigned long read_period_ms;
    unsigned long max_frequency_ms;
    unsigned long unchanged_timeout_ms;
}
  sEeprom;

void eeprom_dump( sEeprom &sdata );
void eeprom_read( sEeprom &sdata );
void eeprom_reset( sEeprom &sdata );
void eeprom_write( sEeprom &sdata );

#endif // __TELEINFO_EEPROM_H__

