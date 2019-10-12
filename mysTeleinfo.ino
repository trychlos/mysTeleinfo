/*
 * MysTeleinfo
 * Copyright (C) 2017,2018,2019 Pierre Wieser <pwieser@trychlos.org>
 *
 * Description:
 * Manages in one MySensors-compatible board EdF teleinformation.
 *
 * Radio module:
 * Is implemented with a NRF24L01+ radio module
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * NOTE: this sketch has been written for MySensor v 2.1 library.
 * 
 * pwi 2017- 3-25 creation
 * pwi 2017- 5-27 v2.0 use configurable pwiTimer class
 * pwi 2019- 5-25 v2.1-2019
 *                  update to linky
 *                  all ids are changed : main and management are first, teleinfo start at 100
 * Sketch uses 23052 bytes (75%) of program storage space. Maximum is 30720 bytes.
  Global variables use 1899 bytes (92%) of dynamic memory, leaving 149 bytes for local variables. Maximum is 2048 bytes.
 *
 * pwi 2019- 9-22 v3.0-2019
 *                  update to TIC standard, based on LkyRx_06 by MicroQuettas
 *                  use standard SoftwareSerial library
 * Sketch uses 25284 bytes (82%) of program storage space. Maximum is 30720 bytes.
Global variables use 1022 bytes (49%) of dynamic memory, leaving 1026 bytes for local variables. Maximum is 2048 bytes.
 */

// uncomment for debugging this sketch
#define SKETCH_DEBUG

static char const sketchName[] PROGMEM    = "mysTeleinfo";
static char const sketchVersion[] PROGMEM = "3.0-2019";

/* **************************************************************************************
 * MySensors gateway
 * 
 *  Please note that activating MySensors DEBUG flag (below) may prevent
 *  the Teleinfo to get rightly its informations (but why ??)
 */
#define MY_NODE_ID        1
//#define MY_DEBUG
#define MY_REPEATER_FEATURE
#define MY_RADIO_NRF24
#define MY_RF24_PA_LEVEL RF24_PA_HIGH
//#include <pwi_myhmac.h>
#include <pwi_myrf24.h>
#include <MySensors.h>

MyMessage msg;

enum {
    CHILD_MAIN = 1,
    CHILD_TI   = 100
};

/*
 * Declare our classes
 */
#include <pwiCommon.h>

#include <pwiTimer.h>
pwiTimer autodump_timer;
pwiTimer dup_timer;

#include "eeprom.h"
sEeprom eeprom;

/* **************************************************************************************
 * TeleInformation Client (TIC) declarations
 * Adapted from https://github.com/jaysee/teleInfo
 * Adapted from LkyRx_06 by MicroQuettas
 * 
 * TIC is :
 * - continuously read
 * - compared with last sent values, and possibly sent, on max frequency callback (default=10s)
 * - fully sent on unchanged timeout (default=24h)
 * 
 * Note that, according to the documentation, standard TIC frames are sent every 1s.
 * 
 * teleInfo
 * - rxPin:  D4 (PCINT2 - Port D)
 * - ledPin: D5
 * - hcPin:  D6
 * - hpPin:  D7
 * 
 */
#include "Linky.h"
Linky linky( CHILD_TI, 4, 5, 6, 7 );

/* **************************************************************************************
 *  CHILD_MAIN Sensor
 */

void mainPresentation()
{
#ifdef SKETCH_DEBUG
    Serial.println( F( "mainPresentation()" ));
#endif
    //                                   1234567890123456789012345
    present( CHILD_MAIN+1, S_CUSTOM, F( "Min timer" ));
    present( CHILD_MAIN+2, S_CUSTOM, F( "Max timer" ));
    present( CHILD_MAIN+3, S_CUSTOM, F( "Duplication arm" ));
    present( CHILD_MAIN+4, S_CUSTOM, F( "Duplication thread" ));
    present( CHILD_MAIN+5, S_CUSTOM, F( "Autodump delay" ));
    present( CHILD_MAIN+6, S_CUSTOM, F( "Dup period" ));
}

void mainSetup()
{
#ifdef SKETCH_DEBUG
    Serial.println( F( "mainSetup()" ));
#endif
    autodump_timer.setup( "AutoDump", eeprom.auto_dump_ms, false, ( pwiTimerCb ) mainAutoDumpCb );
    autodump_timer.start();
    dup_timer.setup( "Dup", eeprom.dup_ms, false, ( pwiTimerCb ) mainDupCb );
    dup_timer.start();
}

void mainAutoDumpCb( void*empty )
{
    dumpData();
}

void mainAutoDumpSend()
{
    msg.clear();
    send( msg.setSensor( CHILD_MAIN+5 ).setType( V_VAR1 ).set( eeprom.auto_dump_ms ));
}

void mainAutoDumpSet( unsigned long ulong )
{
    eeprom.auto_dump_ms = ulong;
    eepromWrite( eeprom, saveState );
    autodump_timer.setDelay( ulong );
    autodump_timer.restart();
}

void mainDuplicateArmSend()
{
    msg.clear();
    send( msg.setSensor( CHILD_MAIN+4 ).setType( V_VAR1 ).set( eeprom.dup_thread ));
}

void mainDuplicateArmSet( bool duplicate )
{
    eeprom.dup_thread = duplicate;
    eepromWrite( eeprom, saveState );
}

/*
 * Due to the standard transmission frequency (9600 bps, which leds to about 1 char/millis)
 * we cannot just duplicate each and every trame to the controller.
 * So we duplicate the next trame at each dup_timer period.
 */
void mainDupCb( void*empty )
{
    linky.setDup( eeprom.dup_thread && eeprom.dup_ms > 0 );
}

void mainDupSend()
{
    msg.clear();
    send( msg.setSensor( CHILD_MAIN+6 ).setType( V_VAR1 ).set( eeprom.dup_ms ));
}

void mainDupSet( unsigned long ulong )
{
    eeprom.dup_ms = ulong;
    eepromWrite( eeprom, saveState );
    dup_timer.setDelay( ulong );
    dup_timer.restart();
}

void mainMaxFrequencySend()
{
    msg.clear();
    send( msg.setSensor( CHILD_MAIN+1 ).setType( V_VAR1 ).set( eeprom.min_period_ms ));
}

void mainMaxFrequencySet( unsigned long ulong )
{
    eeprom.min_period_ms = ulong;
    eepromWrite( eeprom, saveState );
}

void mainUnchangedSend()
{
    msg.clear();
    send( msg.setSensor( CHILD_MAIN+2 ).setType( V_VAR1 ).set( eeprom.max_period_ms ));
}

void mainUnchangedSet( unsigned long ulong )
{
    eeprom.max_period_ms = ulong;
    eepromWrite( eeprom, saveState );
}

/* **************************************************************************************
 *  MAIN CODE
 */
void presentation()
{
#ifdef SKETCH_DEBUG
    Serial.println( F( "presentation()" ));
#endif

    mainPresentation();
    linky.present();
}

void setup()  
{
#ifdef SKETCH_DEBUG
    Serial.begin( 115200 );
    Serial.println( F( "setup()" ));
#endif
    sendSketchInfo( PGMSTR( sketchName ), PGMSTR( sketchVersion ));

    // library version
    msg.clear();
    mSetCommand( msg, C_INTERNAL );
    sendAsIs( msg.setSensor( 255 ).setType( I_VERSION ).set( MYSENSORS_LIBRARY_VERSION ));

    //eepromReset( eeprom, saveState );
    eepromRead( eeprom, loadState, saveState );
    eepromDump( eeprom );

    mainSetup();
    linky.setup( eeprom.min_period_ms, eeprom.max_period_ms );
}

void loop()
{
    pwiTimer::Loop();
    linky.loop();
}

void receive( const MyMessage &message )
{
    uint8_t cmd = message.getCommand();

    char payload[MAX_PAYLOAD+1];
    memset( payload, '\0', sizeof( payload ));
    message.getString( payload );

#ifdef SKETCH_DEBUG
    Serial.print( F( "receive() sensor=" ));
    Serial.print( message.sensor );
    Serial.print( F( ", type=" ));
    Serial.print( message.type );
    Serial.print( F( ", cmd=" ));
    Serial.print( cmd );
    Serial.print( F( ", payload='" ));
    Serial.print( payload );
    Serial.println( F( "'" ));
#endif

    // all received messages should be V_CUSTOM
    if( message.type != V_CUSTOM ){
#ifdef SKETCH_DEBUG
        Serial.println( F( "receive() message should be V_CUSTOM, cancelled" ));
#endif
        return;
    }

    if( message.sensor == CHILD_MAIN && cmd == C_REQ ){
        uint8_t ureq = strlen( payload ) > 0 ? atoi( payload ) : 0;
        switch( ureq ){
            case 1:
                eepromReset( eeprom, saveState );
                break;
            case 2:
                dumpData();
                break;
        }
        return;
    }

    if( cmd == C_SET ){
        unsigned long ulong = strlen( payload ) > 0 ? atol( payload ) : 0;
        uint8_t ureq = strlen( payload ) > 0 ? atoi ( payload ) : 0;
        switch( message.sensor ){
            case CHILD_MAIN+1:
                mainMaxFrequencySet( ulong );
                mainMaxFrequencySend();
                break;
            case CHILD_MAIN+2:
                mainUnchangedSet( ulong );
                mainUnchangedSend();
                break;
            case CHILD_MAIN+3:
                mainDuplicateArmSet( ureq==1 );
                mainDuplicateArmSend();
                break;
            case CHILD_MAIN+5:
                mainAutoDumpSet( ulong );
                mainAutoDumpSend();
                break;
            case CHILD_MAIN+6:
                mainDupSet( ulong );
                mainDupSend();
                break;
        }
    }
}

void dumpData()
{
    mainMaxFrequencySend();
    mainUnchangedSend();
    mainDuplicateArmSend();
    mainAutoDumpSend();
    linky.send( true );
}

