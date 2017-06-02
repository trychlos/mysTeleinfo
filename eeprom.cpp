#include "Arduino.h"
#include "eeprom.h"

/**
 * eeprom_dump:
 * @sdata: the sEeprom data structure to be dumped.
 *
 * Dump the sEeprom struct content.
 */
void eeprom_dump( sEeprom &sdata )
{
    Serial.print( F( "[eeprom_dump] mark='" )); Serial.print( sdata.mark ); Serial.println( "'" );
    Serial.print( F( "[eeprom_dump] read_period=" )); Serial.print( sdata.read_period_ms ); Serial.println( "ms" );
    Serial.print( F( "[eeprom_dump] max_frequency=" )); Serial.print( sdata.max_frequency_ms ); Serial.println( "ms" );
    Serial.print( F( "[eeprom_dump] unchanged_timeout=" )); Serial.print( sdata.unchanged_timeout_ms ); Serial.println( "ms" );
}

