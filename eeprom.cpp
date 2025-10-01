#include "eeprom.h"

/* **********************************************************************************************************
 *  EEPROM management
 *  
 * pwi 2019- 6- 1 v1 creation
 * pwi 2025-10- 1 v3 remove dup_thread
 */

// uncomment for debugging eeprom functions
#define EEPROM_DEBUG

static const char pwiLabel[] PROGMEM = "PWI";

/**
 * eepromDump:
 */
void eepromDump( sEeprom &data )
{
#ifdef EEPROM_DEBUG
    Serial.print( F( "[eepromDump] mark='" ));         Serial.print( data.mark ); Serial.println( F( "'" ));
    Serial.print( F( "[eepromDump] version=" ));       Serial.println( data.version );
    Serial.print( F( "[eepromDump] min_period_ms=" )); Serial.println( data.min_period_ms );
    Serial.print( F( "[eepromDump] max_period_ms=" )); Serial.println( data.max_period_ms );
    Serial.print( F( "[eepromDump] auto_dump_ms=" ));  Serial.println( data.auto_dump_ms );
#endif
}

/**
 * eepromRead:
 */
void eepromRead( sEeprom &data, pEepromRead pfnRead, pEepromWrite pfnWrite )
{
    for( uint8_t i=0 ; i<sizeof( sEeprom ); ++i ){
        (( uint8_t * ) &data )[i] = pfnRead( i );
    }
    // initialize with default values if mark not found or incorrect version
    if( data.mark[0] != 'P' || data.mark[1] != 'W' || data.mark[2] != 'I' || data.mark[3] != 0 || data.version != EEPROM_VERSION ){
        eepromReset( data, pfnWrite );
    }
}

/**
 * eepromReset:
 */
void eepromReset( sEeprom &data, pEepromWrite pfnWrite )
{
#ifdef EEPROM_DEBUG
    Serial.println( F( "[eepromReset]" ));
#endif
    memset( &data, '\0', sizeof( sEeprom ));
    strcpy_P( data.mark, pwiLabel );
    data.version = EEPROM_VERSION;
  
    data.min_period_ms = 10000;     // 10s
    data.max_period_ms = 3600000;   // 1h
    data.auto_dump_ms = 86400000;   // 24h
  
    eepromWrite( data, pfnWrite );
}

/**
 * eepromWrite:
 */
void eepromWrite( sEeprom &data, pEepromWrite pfnWrite )
{
#ifdef EEPROM_DEBUG
    Serial.println( F( "[eepromWrite]" ));
#endif
    for( uint8_t i=0 ; i<sizeof( sEeprom ); ++i ){
        pfnWrite( i, (( uint8_t * ) &data )[i] );
    }
}

