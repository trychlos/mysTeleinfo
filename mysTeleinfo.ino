/*
   MysTeleinfo
   Copyright (C) 2017,2018,2019 Pierre Wieser <pwieser@trychlos.org>

   Description:
   Manages in one MySensors-compatible board EdF teleinformation.

   Radio module:
   Is implemented with a NRF24L01+ radio module

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as published by the Free Software Foundation.

   NOTE: this sketch has been written for MySensor v 2.1 library.
 
   pwi 2017- 3-25 creation
   pwi 2017- 5-27 v2.0 use configurable pwiTimer class
   pwi 2019- 5-25 v2.1-2019
                  update to linky
                  all ids are changed : main and management are first, teleinfo start at 100
   pwi 2019- 9-22 v3.0-2019
                  update to TIC standard, based on LkyRx_06 by MicroQuettas
                  use standard SoftwareSerial library
   pwi 2025- 9-30 v4.0-2025
                  review the whole children identifiers and types (align on mysCellar, adapt to HA)
                  provides a minimal set of managed labels

 Sketch uses 23554 bytes (76%) of program storage space. Maximum is 30720 bytes.
 Global variables use 1057 bytes (51%) of dynamic memory, leaving 991 bytes for local variables. Maximum is 2048 bytes.
*/

// uncomment for debugging this sketch
#define SKETCH_DEBUG

static char const sketchName[] PROGMEM    = "mysTeleinfo";
static char const sketchVersion[] PROGMEM = "4.0-2025";

/* **********************************************************************************************************
   **********************************************************************************************************
 * MySensors gateway
 * 
 *  Please note that activating MySensors DEBUG flag (below) may prevent
 *  the Teleinfo to get rightly its informations (but why ??)
 */
#define MY_NODE_ID        1
//#define MY_DEBUG
#define MY_REPEATER_FEATURE
#define MY_RADIO_RF24
#define MY_RF24_PA_LEVEL RF24_PA_HIGH
//#include <pwi_myhmac.h>
#include <pwi_myrf24.h>
#include <MySensors.h>

MyMessage msg;

/*
 * Declare our classes
 */
#include <pwiCommon.h>

#include <pwiTimer.h>
pwiTimer autodump_timer;

#include "childids.h"
#include "eeprom.h"
sEeprom eeprom;

/* **********************************************************************************************************
   **********************************************************************************************************
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
bool linky_initial_sent = false;

/* **********************************************************************************************************
   **********************************************************************************************************
 *  CHILD_MAIN Sensor
 */

bool main_initial_sents = false;
bool main_log_initial_sent = false;

void mainPresentation()
{
#ifdef SKETCH_DEBUG
    Serial.println( F( "mainPresentation()" ));
#endif
    //                                                  1234567890123456789012345
    present( CHILD_MAIN_LOG,              S_INFO,   F( "Board logs" ));
    present( CHILD_MAIN_ACTION_RESET,     S_BINARY, F( "Action: reset eeprom" ));
    present( CHILD_MAIN_ACTION_DUMP,      S_BINARY, F( "Action: dump eeprom" ));
    present( CHILD_MAIN_PARM_DUMP_PERIOD, S_INFO,   F( "Parm: eeprom dump period" ));
    present( CHILD_MAIN_PARM_MIN_PERIOD,  S_INFO,   F( "Parm: report min period" ));
    present( CHILD_MAIN_PARM_MAX_PERIOD,  S_INFO,   F( "Parm: report max period" ));
}

void mainSetup()
{
#ifdef SKETCH_DEBUG
    Serial.println( F( "mainSetup()" ));
#endif
    autodump_timer.setup( "AutoDump", eeprom.auto_dump_ms, false, ( pwiTimerCb ) mainAutoDumpCb );
    autodump_timer.start();
    mainActionResetSend();
    mainActionDumpSend();
    mainAutoDumpSend();
    mainMinPeriodSend();
    mainMaxPeriodSend();
    main_initial_sents = true;
}

/* called from main loop() function
 * make sure we send an initial log value (e.g. 'Ready' ) to the controller at startup after all other children
 */
void mainInitialLoop( void )
{
    if( main_initial_sents && linky_initial_sent && !main_log_initial_sent ){
        mainLogSend(( char * ) "Node ready" );
        main_log_initial_sent = true;
    }
}

void mainActionDumpDo()
{
    dumpData();
}

void mainActionDumpSend()
{
    uint8_t sensor_id = CHILD_MAIN_ACTION_DUMP;
    uint8_t msg_type = V_STATUS;
    uint8_t payload = 0;
#ifdef SKETCH_DEBUG
    Serial.print( F( "[mainActionDumpSend] sensor=" ));
    Serial.print( sensor_id );
    Serial.print( F( ", type=" ));
    Serial.print( msg_type );
    Serial.print( F( ", payload=" ));
    Serial.println( payload );
#endif
    msg.clear();
    send( msg.setSensor( sensor_id ).setType( msg_type ).set( payload ));
}

void mainActionResetDo()
{
    eepromReset( eeprom, saveState );
}

void mainActionResetSend()
{
    uint8_t sensor_id = CHILD_MAIN_ACTION_RESET;
    uint8_t msg_type = V_STATUS;
    uint8_t payload = 0;
#ifdef SKETCH_DEBUG
    Serial.print( F( "[mainActionResetSend] sensor=" ));
    Serial.print( sensor_id );
    Serial.print( F( ", type=" ));
    Serial.print( msg_type );
    Serial.print( F( ", payload=" ));
    Serial.println( payload );
#endif
    msg.clear();
    send( msg.setSensor( sensor_id ).setType( msg_type ).set( payload ));
}

void mainAutoDumpCb( void*empty )
{
    dumpData();
}

void mainAutoDumpSend()
{
    msg.clear();
    send( msg.setSensor( CHILD_MAIN_ACTION_DUMP ).setType( V_TEXT ).set( eeprom.auto_dump_ms ));
}

void mainAutoDumpSet( unsigned long ulong )
{
    eeprom.auto_dump_ms = ulong;
    eepromWrite( eeprom, saveState );
    autodump_timer.setDelay( ulong );
    autodump_timer.restart();
}

void mainLogSend( char *log )
{
    msg.clear();
    send( msg.setSensor( CHILD_MAIN_LOG ).setType( V_TEXT ).set( log ));
}

void mainMaxPeriodSend()
{
    uint8_t sensor_id = CHILD_MAIN_PARM_MAX_PERIOD;
    uint8_t msg_type = V_TEXT;
    unsigned long payload = eeprom.max_period_ms;
#ifdef SKETCH_DEBUG
    Serial.print( F( "[mainAutoSaveSend] sensor=" ));
    Serial.print( sensor_id );
    Serial.print( F( ", type=" ));
    Serial.print( msg_type );
    Serial.print( F( ", payload=" ));
    Serial.println( payload );
#endif
    msg.clear();
    send( msg.setSensor( sensor_id ).setType( msg_type ).set( payload ));
}

void mainMaxPeriodSet( unsigned long ulong )
{
    eeprom.max_period_ms = ulong;
    eepromWrite( eeprom, saveState );
}

void mainMinPeriodSend()
{
    uint8_t sensor_id = CHILD_MAIN_PARM_MIN_PERIOD;
    uint8_t msg_type = V_TEXT;
    unsigned long payload = eeprom.min_period_ms;
#ifdef SKETCH_DEBUG
    Serial.print( F( "[mainAutoSaveSend] sensor=" ));
    Serial.print( sensor_id );
    Serial.print( F( ", type=" ));
    Serial.print( msg_type );
    Serial.print( F( ", payload=" ));
    Serial.println( payload );
#endif
    msg.clear();
    send( msg.setSensor( sensor_id ).setType( msg_type ).set( payload ));
}

void mainMinPeriodSet( unsigned long ulong )
{
    eeprom.min_period_ms = ulong;
    eepromWrite( eeprom, saveState );
}

/* **********************************************************************************************************
 * **********************************************************************************************************
 *  MAIN CODE
 * **********************************************************************************************************
 * ********************************************************************************************************** */

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

    //eepromReset( eeprom, saveState );
    eepromRead( eeprom, loadState, saveState );
    eepromDump( eeprom );

    mainSetup();
    linky.setup( eeprom.min_period_ms, eeprom.max_period_ms );
    linky_initial_sent = true;
}

void loop()
{
    mainInitialLoop();
    pwiTimer::Loop();
    if( main_log_initial_sent ){
        linky.loop();
    }
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

    if( cmd == C_SET ){
        uint8_t ureq = strlen( payload ) > 0 ? atoi( payload ) : 0;
        unsigned long ulong = strlen( payload ) ? atol( payload ) : 0;
        switch( message.sensor ){
            case CHILD_MAIN_ACTION_RESET:
                if( message.type == V_STATUS && ureq == 1 ){
                    mainActionResetDo();
                    mainActionResetSend();
                    mainLogSend(( char * ) "eeprom reset done" );
                }
                break;
            case CHILD_MAIN_ACTION_DUMP:
                if( message.type == V_STATUS && ureq == 1 ){
                    mainActionDumpDo();
                    mainActionDumpSend();
                    mainLogSend(( char * ) "eeprom dump done" );
                }
                break;
            case CHILD_MAIN_PARM_DUMP_PERIOD:
                if( message.type == V_TEXT && strlen( payload )){
                    mainAutoDumpSet( ulong );
                    mainAutoDumpSend();
                }
                break;
            case CHILD_MAIN_PARM_MAX_PERIOD:
                if( message.type == V_TEXT && strlen( payload )){
                    mainMaxPeriodSet( ulong );
                    mainMaxPeriodSend();
                }
                break;
            case CHILD_MAIN_PARM_MIN_PERIOD:
                if( message.type == V_TEXT && strlen( payload )){
                    mainMinPeriodSet( ulong );
                    mainMinPeriodSend();
                }
                break;
        }
    } // end of cmd == C_SET
}

void dumpData()
{
    mainMaxPeriodSend();
    mainMinPeriodSend();
    mainAutoDumpSend();
    linky.send( true );
}

