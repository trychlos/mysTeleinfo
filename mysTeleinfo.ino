/*
 * MysTeleinfo
 * Copyright (C) 2017 Pierre Wieser <pwieser@trychlos.org>
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
 */

// uncomment for debugging this sketch
#define DEBUG_ENABLED

// uncomment for having mySensors radio enabled
#define HAVE_NRF24_RADIO

// uncomment for having TeleInfo code
#define HAVE_TELEINFO

#include "eeprom.h"
#include "pwi_timer.h"

static const char * const thisSketchName    = "mysTeleinfo";
static const char * const thisSketchVersion = "2.0-2017";

static unsigned long def_read_period_ms = 1000;               // read the teleinfo trames every sec.
static unsigned long def_max_frequency_ms = 30000;            // send changes to the controller at most every 30 sec.
static unsigned long def_unchanged_timeout_ms = 86400000;     // send all "stable" data once every day

static sEeprom st_eeprom;
static pwiTimer st_main_timer;
static pwiTimer st_changed_timer;
static pwiTimer st_unchanged_timer;

enum {
    CHILD_MAINTIMER = 70,                                     // teleinfo child id's are in 0..60
    CHILD_CHANGEDTIMER,
    CHILD_UNCHANGEDTIMER,
};

// Please note that activating MySensors DEBUG flag (below) may prevent
// the Teleinfo to get rightly its informations (but why ??)
//#define MY_DEBUG
#define MY_RADIO_NRF24
#define MY_RF24_CHANNEL 103
#define MY_SIGNING_SOFT
#define MY_SIGNING_SOFT_RANDOMSEED_PIN 7
#define MY_SOFT_HMAC_KEY 0xe5,0xc5,0x36,0xd8,0x4b,0x45,0x49,0x25,0xaa,0x54,0x3b,0xcc,0xf4,0xcb,0xbb,0xb7,0x77,0xa4,0x80,0xae,0x83,0x75,0x40,0xc2,0xb1,0xcb,0x72,0x50,0xaa,0x8a,0xf1,0x6d
// do not check the uplink if we choose to disable the radio
#ifndef HAVE_NRF24_RADIO
#define MY_TRANSPORT_WAIT_READY_MS 1000
#define MY_TRANSPORT_UPLINK_CHECK_DISABLED
#endif // HAVE_NRF24_RADIO
#include <MySensors.h>

MyMessage msg;

/* **************************************************************************************
 * MySensors gateway
 */
void presentation_my_sensors()  
{
#ifdef DEBUG_ENABLED
    Serial.println( F( "[presentation_my_sensors]" ));
#endif
#ifdef HAVE_NRF24_RADIO
    sendSketchInfo( thisSketchName, thisSketchVersion );
#endif // HAVE_NRF24_RADIO
}

/* **************************************************************************************
 * TeleInformation Client (TIC) declarations
 * Shamelessly copied from https://github.com/jaysee/teleInfo
 * + only use interrupts on Port B (PCINT0 vector)
 */
#ifdef HAVE_TELEINFO
#include "teleInfo.h"

#define CHILD_ID_ADCO       0
#define CHILD_ID_OPTARIF    1
#define CHILD_ID_ISOUSC     2
#define CHILD_ID_PTEC       3
#define CHILD_ID_IINST      4
#define CHILD_ID_ADPS       5
#define CHILD_ID_IMAX       6
#define CHILD_ID_PAPP       7

// infos tarif BASE
#define CHILD_ID_BASE       10

// infos tarif HC/HP
#define CHILD_ID_HC_HC      20
#define CHILD_ID_HC_HP      21

// infos EJP
#define CHILD_ID_EJP_HN     30
#define CHILD_ID_EJP_HPM    31
#define CHILD_ID_PEJP       32

// infos tarif BBR (tempo)
#define CHILD_ID_BBR_HC_JB  40
#define CHILD_ID_BBR_HP_JB  41
#define CHILD_ID_BBR_HC_JW  42
#define CHILD_ID_BBR_HP_JW  43
#define CHILD_ID_BBR_HC_JR  44
#define CHILD_ID_BBR_HP_JR  45
#define CHILD_ID_DEMAIN     46

// used by HCHP/EJP/BBR
#define CHILD_ID_HHPHC      50

// send the full string as 'label=value'
#define CHILD_ID_THREAD     60

// teleInfo
// - rxPin:  D4 (PCINT2 - Port D)
// - ledPin: D5
// - hcPin:  D6
// - hpPin:  D7
teleInfo TI( 4, 5, 6, 7 );

uint8_t tarif = 0;
teleInfo_t lastTI;              // last sent teleinfo datas
teleInfo_t currentTI;           // last received teleinfo datas

static const char * const ti_teleInfo = "[teleInfo] ";
static const char * const ti_changed  = " changed from ";
static const char * const ti_to       = " to ";

/*
 * Compare 2 valeurs
 * Si il y a eu un changement, le sauvegarde et l'envoie à la gateway
 */
void compareTI( const char *label, uint32_t value, uint32_t &last, uint8_t msgType, int SENSOR_ID )
{
    if( 0 ){
        Serial.print( "(uint32_t) label=" ); Serial.println( label );
    }
    if( last == value ) return;
#ifdef DEBUG_ENABLED
    Serial.print( ti_teleInfo ); Serial.print( label );
    Serial.print( ti_changed );  Serial.print( last );
    Serial.print( ti_to );       Serial.println( value );
#endif
    last = value;
#ifdef HAVE_NRF24_RADIO
    msg.clear();
    send( msg.setSensor( SENSOR_ID ).setType( msgType ).set( last ));
#endif
}

void compareTI( const char *label, uint8_t value, uint8_t &last, uint8_t msgType, int SENSOR_ID )
{
    if( 0 ){
        Serial.print( "(uint8_t) label=" ); Serial.println( label );
    }
    if( last == value ) return;
#ifdef DEBUG_ENABLED
    Serial.print( ti_teleInfo ); Serial.print( label );
    Serial.print( ti_changed );  Serial.print( last );
    Serial.print( ti_to );       Serial.println( value );
#endif
    last = value;
#ifdef HAVE_NRF24_RADIO
    msg.clear();
    send( msg.setSensor( SENSOR_ID ).setType( msgType ).set( last ) );
#endif
}

void compareTI( const char *label, char value, char &last, uint8_t msgType, int SENSOR_ID )
{
    if( 0 ){
        Serial.print( "(char &) label=" ); Serial.println( label );
    }
    if( last == value ) return;
#ifdef DEBUG_ENABLED
    Serial.print( ti_teleInfo ); Serial.print( label );
    Serial.print( ti_changed );  Serial.print( last );
    Serial.print( ti_to );       Serial.println( value );
#endif
    last = value;
#ifdef HAVE_NRF24_RADIO
    msg.clear();
    send( msg.setSensor( SENSOR_ID ).setType( msgType ).set( last ) );
#endif
}

void compareTI( const char *label, char *value, char *last, uint8_t msgType, int SENSOR_ID )
{
    if( 0 ){
        Serial.print( "(char *) label=" ); Serial.println( label );
    }
    if( strcmp( last, value ) == 0 ) return;
#ifdef DEBUG_ENABLED
    Serial.print( ti_teleInfo ); Serial.print( label );
    Serial.print( ti_changed );  Serial.print( last );
    Serial.print( ti_to );       Serial.println( value );
#endif
    memset( last, 0, TI_BUFSIZE );
    strcpy( last, value );
#ifdef HAVE_NRF24_RADIO
    msg.clear();
    send( msg.setSensor( SENSOR_ID ).setType( msgType ).set( last ) );
#endif
}

/*
 * A thread callback to send the full thread to the master gateway
 * as a text message 'label=value'
 * 
 * The max teleInfo trame length is just 12 bytes, while the max
 * mySensors payload is 25 bytes!
 */
void thread_cb( const char *label, const char *value )
{
    char str[1+MAX_PAYLOAD];
    
    if( strlen( label )){
        if( strlen( value )){
            sprintf( str, "%s=%s", label, value );
        } else {
            sprintf( str, "%s", label );
        }
#ifdef HAVE_NRF24_RADIO
        msg.clear();
        send( msg.setSensor( CHILD_ID_THREAD ).setType( V_VAR1 ).set( str ) );
#endif
    }
}

/*
 * Setup the TeleInformations Client (TIC) sensor if available
 * A first read let us take the configuration of our own counter
 * and present only the relevant informations to the gateway
 */
void presentation_teleinfo()  
{
    static const char * const thisfn = "[presentation_teleinfo]";
#ifdef DEBUG_ENABLED
    Serial.println( thisfn );
#endif
    memset( &lastTI, '\0', sizeof( teleInfo_t ));
    /* une première lecture va nous donner l'option tarifaire
     * on va s'en servir pour présenter uniquement les bons messages a la gateway
     * la première lecture peut être mauvaise: répéter jusqu'à avoir trouvé le tarif
     * (max 10 fois)
     * => d'abord, identifier le tarif pour pouvoir présenter les capteurs
     *    correctement.
     */
    uint8_t i = 0;
    while( tarif == 0 ){
        bool ret = TI.get( &currentTI );
        if( ret ){
            if( !strcmp( currentTI.OPTARIF, TI_BASE )){
                tarif = TI_TARIF_BASE;
            } else if( !strcmp( currentTI.OPTARIF, TI_OPTARIF_HC )){
                tarif = TI_TARIF_HCHP;
            } else if( !strcmp( currentTI.OPTARIF, TI_OPTARIF_EJP )){
                tarif = TI_TARIF_EJP;
            } else if( strstr( currentTI.OPTARIF, TI_OPTARIF_BBR ) != NULL ){
                tarif = TI_TARIF_TEMPO;
            }
        }
        if( tarif == 0 ){
            i += 1;
#ifdef DEBUG_ENABLED
            Serial.print( thisfn );
            Serial.print( F( " tarif not found, i=" ));
            Serial.println( i );
#endif
            if( i == 10 ){
                break;
            }
            wait( 100 );
        }
    }
#ifdef DEBUG_ENABLED
    Serial.print( thisfn );
    Serial.print( F( " Tarif is " ));
    Serial.println( tarifToStr( tarif ));
#endif
    if( tarif > 0 ){
#ifdef HAVE_NRF24_RADIO
        // communs a tout le monde
        present( CHILD_ID_ADCO,    S_POWER, "Identifiant du compteur" );
        present( CHILD_ID_OPTARIF, S_POWER, "Option tarifaire du contrat" );
        present( CHILD_ID_ISOUSC,  S_POWER, "Intensité souscrite" );
        present( CHILD_ID_PTEC,    S_POWER, "Option tarifaire courante" );
        present( CHILD_ID_IINST,   S_POWER, "Intensité instantanée" );
        present( CHILD_ID_ADPS,    S_POWER, "Alerte de dépassement de consommation" );
        present( CHILD_ID_IMAX,    S_POWER, "Intensité maxi consommée" );
        present( CHILD_ID_PAPP,    S_POWER, "Puissance apparente courante" );
        present( CHILD_ID_THREAD,  S_POWER, "Thread copy" );

        switch( tarif ){
            case TI_TARIF_BASE:
                present( CHILD_ID_BASE,      S_POWER );
                break;
            case TI_TARIF_HCHP:
                present( CHILD_ID_HC_HC,     S_POWER );
                present( CHILD_ID_HC_HP,     S_POWER );
                present( CHILD_ID_HHPHC,     S_POWER );
                break;
            case TI_TARIF_EJP:
                present( CHILD_ID_EJP_HN,    S_POWER );
                present( CHILD_ID_EJP_HPM,   S_POWER );
                present( CHILD_ID_PEJP,      S_POWER );
                present( CHILD_ID_HHPHC,     S_POWER );
                break;
            case TI_TARIF_TEMPO:
                present( CHILD_ID_BBR_HC_JB, S_POWER );
                present( CHILD_ID_BBR_HP_JB, S_POWER );
                present( CHILD_ID_BBR_HC_JW, S_POWER );
                present( CHILD_ID_BBR_HP_JW, S_POWER );
                present( CHILD_ID_BBR_HC_JR, S_POWER );
                present( CHILD_ID_BBR_HP_JR, S_POWER );
                present( CHILD_ID_DEMAIN,    S_POWER );
                present( CHILD_ID_HHPHC,     S_POWER );
                break;
        }
#endif // HAVE_NRF24_RADIO
        TI.set_thread_cb( thread_cb );
    }
}

// receive_teleinfo() is expected to return true if it has consumed the message
//
bool receive_teleinfo( const MyMessage &message )
{
    char payload[2*MAX_PAYLOAD+1];
    uint8_t cmd = message.getCommand();
    uint8_t req;
    unsigned long ulong;

    switch( cmd ){
        case C_SET:
            if( message.type == V_CUSTOM ){
                memset( payload, '\0', sizeof( payload ));
                message.getString( payload );
                req = strlen( payload ) > 0 ? atoi( payload ) : 0;
                switch( message.sensor ){
                    case CHILD_ID_ADCO:
                        switch( req ){
                            case 1:
                                eeprom_reset( st_eeprom );
                                return( true );
                        }
                        break;
                    case CHILD_ID_THREAD:
                        TI.set_honor_thread_cb( req > 0 );
                        return( true );
                }
            }
            break;
        case C_REQ:
            msg.clear();
#ifdef HAVE_NRF24_RADIO
            if( message.sensor == CHILD_ID_ADCO ){
                send( msg.setSensor( message.sensor ).setType( V_VAR1 ).set( lastTI.ADCO ));
                return( true );
            } else if( message.sensor == CHILD_ID_OPTARIF ){
                send( msg.setSensor( message.sensor ).setType( V_VAR1 ).set( lastTI.OPTARIF ));
                return( true );
            } else if( message.sensor == CHILD_ID_ISOUSC ){
                send( msg.setSensor( message.sensor ).setType( V_CURRENT ).set( lastTI.ISOUSC ));
                return( true );
            } else if( message.sensor == CHILD_ID_PTEC ){
                send( msg.setSensor( message.sensor ).setType( V_VAR1 ).set( lastTI.PTEC ));
                return( true );
            } else if( message.sensor == CHILD_ID_IINST ){
                send( msg.setSensor( message.sensor ).setType( V_CURRENT ).set( lastTI.IINST ));
                return( true );
            } else if( message.sensor == CHILD_ID_ADPS ){
                send( msg.setSensor( message.sensor ).setType( V_CURRENT ).set( lastTI.ADPS ));
                return( true );
            } else if( message.sensor == CHILD_ID_IMAX ){
                send( msg.setSensor( message.sensor ).setType( V_CURRENT ).set( lastTI.IMAX ));
                return( true );
            } else if( message.sensor == CHILD_ID_PAPP ){
                send( msg.setSensor( message.sensor ).setType( V_WATT ).set( lastTI.PAPP ));
                return( true );
            } else if( message.sensor == CHILD_ID_HC_HC ){
                send( msg.setSensor( message.sensor ).setType( V_KWH ).set( lastTI.h.cp.HC_HC ));
                return( true );
            } else if( message.sensor == CHILD_ID_HC_HP ){
                send( msg.setSensor( message.sensor ).setType( V_KWH ).set( lastTI.h.cp.HC_HP ));
                return( true );
            } else if( message.sensor == CHILD_ID_HHPHC ){
                send( msg.setSensor( message.sensor ).setType( V_VAR1 ).set( lastTI.h.HHPHC ));
                return( true );
            }
#endif // HAVE_NRF24_RADIO
            break;
    }

    return( false );
}
#endif // HAVE_TELEINFO

/* **************************************************************************************
 *  MAIN CODE
 */
void presentation()
{
#ifdef DEBUG_ENABLED
    Serial.println( "[presentation]" );
#endif
    presentation_my_sensors();
#ifdef HAVE_TELEINFO
    presentation_teleinfo();
#endif
#ifdef HAVE_NRF24_RADIO
    present( CHILD_MAINTIMER,      S_CUSTOM, "MainTimer" );
    present( CHILD_CHANGEDTIMER,   S_CUSTOM, "MaxFrequencyTimer" );
    present( CHILD_UNCHANGEDTIMER, S_CUSTOM, "UnchangedTimer" );
#endif
}

void setup()  
{
#ifdef DEBUG_ENABLED
    Serial.begin( 115200 );
    Serial.println( F( "[setup]" ));
#endif
    eeprom_read( st_eeprom );
    st_main_timer.start( "MainTimer", st_eeprom.read_period_ms, false, onMainTimerCb, NULL, false );
    st_changed_timer.start( "MaxFrequencyTimer", st_eeprom.max_frequency_ms, false, onChangedTimerCb );
    st_unchanged_timer.start( "UnchangedTimer", st_eeprom.unchanged_timeout_ms, false, onUnchangedTimerCb );
    
    send_var( CHILD_MAINTIMER, st_eeprom.read_period_ms );
    send_var( CHILD_CHANGEDTIMER, st_eeprom.max_frequency_ms );
    send_var( CHILD_UNCHANGEDTIMER, st_eeprom.unchanged_timeout_ms );
}

void loop()
{
    pwiTimer::Loop();
}

/*
 * This callback is set from the 'read_period_ms':
 *  just read the next trame, pulsing the LED accordingly, storing the datas
 */
void onMainTimerCb( void *empty )
{
#ifdef DEBUG_ENABLED
    Serial.println( F( "[onMainTimerCb]" ));
#endif
#ifdef HAVE_TELEINFO
    if( tarif > 0 ){
        TI.get( &currentTI );
    }
#endif
}

/*
 * This callback is set from the 'max_frequency_ms':
 *  send the changed from last read 'currentTI' and last sent 'last'
 */
void onChangedTimerCb( void *empty )
{
#ifdef DEBUG_ENABLED
    Serial.println( F( "[onChangedTimerCb]" ));
#endif
#ifdef HAVE_TELEINFO
    if( tarif > 0 ){
        compareTI( TI_ADCO,    currentTI.ADCO,      lastTI.ADCO,      V_VAR1,    CHILD_ID_ADCO );                       // char *
        compareTI( TI_OPTARIF, currentTI.OPTARIF,   lastTI.OPTARIF,   V_VAR1,    CHILD_ID_OPTARIF );                    // char *
        compareTI( TI_ISOUSC,  currentTI.ISOUSC,    lastTI.ISOUSC,    V_CURRENT, CHILD_ID_ISOUSC );                     // uint8_t
        compareTI( TI_PTEC,    currentTI.PTEC,      lastTI.PTEC,      V_VAR1,    CHILD_ID_PTEC );
        compareTI( TI_IINST,   currentTI.IINST,     lastTI.IINST,     V_CURRENT, CHILD_ID_IINST );
        compareTI( TI_ADPS,    currentTI.ADPS,      lastTI.ADPS,      V_CURRENT, CHILD_ID_ADPS );
        compareTI( TI_IMAX,    currentTI.IMAX,      lastTI.IMAX,      V_CURRENT, CHILD_ID_IMAX );
        compareTI( TI_PAPP,    currentTI.PAPP,      lastTI.PAPP,      V_WATT,    CHILD_ID_PAPP );
        switch( tarif ){
            case TI_TARIF_BASE:
                compareTI( TI_BASE,    currentTI.b.BASE,          lastTI.b.BASE,          V_KWH,     CHILD_ID_BASE );
                break;
            case TI_TARIF_HCHP:
                compareTI( TI_HHPHC,   currentTI.h.HHPHC,         lastTI.h.HHPHC,         V_VAR1,    CHILD_ID_HHPHC );  // char &
                compareTI( TI_HCHC,    currentTI.h.cp.HC_HC,      lastTI.h.cp.HC_HC,      V_KWH,     CHILD_ID_HC_HC );
                compareTI( TI_HCHP,    currentTI.h.cp.HC_HP,      lastTI.h.cp.HC_HP,      V_KWH,     CHILD_ID_HC_HP );
                break;
            case TI_TARIF_EJP:
                compareTI( TI_HHPHC,   currentTI.h.HHPHC,         lastTI.h.HHPHC,         V_VAR1,    CHILD_ID_HHPHC );
                compareTI( TI_EJPHN,   currentTI.h.ejp.EJP_HN,    lastTI.h.ejp.EJP_HN,    V_KWH,     CHILD_ID_EJP_HN );
                compareTI( TI_EJPHPM,  currentTI.h.ejp.EJP_HPM,   lastTI.h.ejp.EJP_HPM,   V_KWH,     CHILD_ID_EJP_HPM );
                compareTI( TI_PEJP,    currentTI.h.ejp.PEJP,      lastTI.h.ejp.PEJP,      V_KWH,     CHILD_ID_PEJP );
                break;
            case TI_TARIF_TEMPO:
                compareTI( TI_HHPHC,   currentTI.h.HHPHC,         lastTI.h.HHPHC,         V_VAR1,    CHILD_ID_HHPHC );
                compareTI( TI_BBRHCJB, currentTI.h.bbr.BBR_HC_JB, lastTI.h.bbr.BBR_HC_JB, V_KWH,     CHILD_ID_BBR_HC_JB );
                compareTI( TI_BBRHPJB, currentTI.h.bbr.BBR_HP_JB, lastTI.h.bbr.BBR_HP_JB, V_KWH,     CHILD_ID_BBR_HP_JB );
                compareTI( TI_BBRHCJW, currentTI.h.bbr.BBR_HC_JW, lastTI.h.bbr.BBR_HC_JW, V_KWH,     CHILD_ID_BBR_HC_JW );
                compareTI( TI_BBRHPJW, currentTI.h.bbr.BBR_HP_JW, lastTI.h.bbr.BBR_HP_JW, V_KWH,     CHILD_ID_BBR_HP_JW );
                compareTI( TI_BBRHCJR, currentTI.h.bbr.BBR_HC_JR, lastTI.h.bbr.BBR_HC_JR, V_KWH,     CHILD_ID_BBR_HC_JR );
                compareTI( TI_BBRHPJR, currentTI.h.bbr.BBR_HP_JR, lastTI.h.bbr.BBR_HP_JR, V_KWH,     CHILD_ID_BBR_HP_JR );
                compareTI( TI_DEMAIN,  currentTI.h.bbr.DEMAIN,    lastTI.h.bbr.DEMAIN,    V_VAR1,    CHILD_ID_DEMAIN );
                break;
        }
    }
#endif
}

/*
 * This callback is set from the 'unchanged_timeout_ms'
 * Send to the controller all data
 */
void onUnchangedTimerCb( void *empty )
{
    msg.clear();
    send( msg.setSensor( CHILD_ID_ADCO ).setType( V_VAR1 ).set( lastTI.ADCO ));
    msg.clear();
    send( msg.setSensor( CHILD_ID_OPTARIF ).setType( V_VAR1 ).set( lastTI.OPTARIF ));
    msg.clear();
    send( msg.setSensor( CHILD_ID_ISOUSC ).setType( V_CURRENT ).set( lastTI.ISOUSC ));
    msg.clear();
    send( msg.setSensor( CHILD_ID_PTEC ).setType( V_VAR1 ).set( lastTI.PTEC ));
    msg.clear();
    send( msg.setSensor( CHILD_ID_ADPS ).setType( V_CURRENT ).set( lastTI.ADPS ));
    msg.clear();
    send( msg.setSensor( CHILD_ID_IMAX ).setType( V_CURRENT ).set( lastTI.IMAX ));
    msg.clear();
    send( msg.setSensor( CHILD_ID_HHPHC ).setType( V_VAR1 ).set( lastTI.h.HHPHC ));
}

/*
 * Accepting from the gateway commands or request targeting the teleinfo
 */
void receive( const MyMessage &message )
{
#ifdef HAVE_TELEINFO
    if( receive_teleinfo( message )){
        return;
    }
#endif // HAVE_TELEINFO
    receive_others( message );
}

void receive_others( const MyMessage &message )
{
    char payload[2*MAX_PAYLOAD+1];
    uint8_t cmd = message.getCommand();
    uint8_t req;
    unsigned long ulong;

    if( cmd == C_SET && message.type == V_CUSTOM ){
        memset( payload, '\0', sizeof( payload ));
        message.getString( payload );
        req = strlen( payload ) > 0 ? atoi( payload ) : 0;
        switch( message.sensor ){
            case CHILD_MAINTIMER:
                switch( req ){
                    case 1:
                        ulong = strlen( payload ) > 2 ? atol( payload+2 ) : 0;
                        if( ulong > 0 ){
                            st_eeprom.read_period_ms = ulong;
                            eeprom_write( st_eeprom );
                            st_main_timer.setDelay( ulong );
                        }
                        break;
                }
                break;
            case CHILD_CHANGEDTIMER:
                switch( req ){
                    case 1:
                        ulong = strlen( payload ) > 2 ? atol( payload+2 ) : 0;
                        if( ulong > 0 ){
                            st_eeprom.max_frequency_ms = ulong;
                            eeprom_write( st_eeprom );
                            st_changed_timer.setDelay( ulong );
                        }
                        break;
                }
                break;
            case CHILD_UNCHANGEDTIMER:
                switch( req ){
                    case 1:
                        ulong = strlen( payload ) > 2 ? atol( payload+2 ) : 0;
                        if( ulong > 0 ){
                            st_eeprom.unchanged_timeout_ms = ulong;
                            eeprom_write( st_eeprom );
                            st_unchanged_timer.setDelay( ulong );
                        }
                        break;
                }
                break;
        }
    } else if( cmd == C_REQ ){
        switch( message.sensor ){
            case CHILD_MAINTIMER:
                send_var( msg.sensor, st_eeprom.read_period_ms );
                break;
            case CHILD_CHANGEDTIMER:
                send_var( msg.sensor, st_eeprom.max_frequency_ms );
                break;
            case CHILD_UNCHANGEDTIMER:
                send_var( msg.sensor, st_eeprom.unchanged_timeout_ms );
                break;
        }
    }
}

void send_var( uint8_t child_id, unsigned long delay_ms )
{
    msg.clear();
    send( msg.setSensor( child_id ).setType( V_VAR1 ).set( delay_ms ));
}

/**
 * eeprom_read:
 * @sdata: the sEeprom data structure to be filled.
 *
 * Read the data from the EEPROM.
 */
void eeprom_read( sEeprom &sdata )
{
#ifdef DEBUG_ENABLED
    Serial.println( F( "[eeprom_read]" ));
#endif
    memset( &sdata, '\0', sizeof( sdata ));
    uint16_t i;
    for( i=0 ; i<sizeof( sdata ); ++i ){
        (( uint8_t * ) &sdata )[i] = loadState(( uint8_t ) i );
    }
    // initialize with default values if mark not found
    if( sdata.mark[0] != 'P' || sdata.mark[1] != 'W' || sdata.mark[2] != 'I' || sdata.mark[3] != 0 ){
        eeprom_reset( sdata );
    } else {
        eeprom_dump( sdata );
    }
}

/**
 * eeprom_reset:
 *
 * RAZ the user data of the EEPROM, resetting it to default values.
 */
void eeprom_reset( sEeprom &sdata )
{
#ifdef DEBUG_ENABLED
    Serial.print( F( "[eeprom_reset] begin=" )); Serial.println( millis());
#endif
    for( int i=0 ; i<256 ; ++i ){
        saveState(( uint8_t ) i, 0 );
    }
#ifdef DEBUG_ENABLED
    Serial.print( F( "[eeprom_reset] end=" )); Serial.println( millis());
#endif
    memset( &sdata, '\0', sizeof( sdata ));
    strcpy( sdata.mark, "PWI" );
    sdata.read_period_ms = def_read_period_ms;
    sdata.max_frequency_ms = def_max_frequency_ms;
    sdata.unchanged_timeout_ms = def_unchanged_timeout_ms;
    eeprom_write( sdata );
}

/**
 * eeprom_write:
 * @sdata: the sEeprom data structure to be written.
 *
 * Write the data to the EEPROM.
 */
void eeprom_write( sEeprom &sdata )
{
#ifdef EEPROM_DEBUG
    Serial.println( F( "[eeprom_write]" ));
#endif
    uint16_t i;
    for( i=0 ; i<sizeof( sdata ) ; ++i ){
        saveState( i, (( uint8_t * ) &sdata )[i] );
    }
    eeprom_dump( sdata );
}

