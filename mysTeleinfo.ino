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
 */

// uncomment for debugging this sketch
#define DEBUG_ENABLED

// uncomment for having mySensors radio enabled
#define HAVE_NRF24_RADIO

// uncomment for having TeleInfo code
#define HAVE_TELEINFO

static const char * const thisSketchName    = "mysTeleinfo";
static const char * const thisSketchVersion = "1.4-2017";

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

#include <Time.h>
uint8_t thisday = 0;

#define CHILD_ID_ADCO       0
MyMessage msgVAR1( 0, V_VAR1 );

#define CHILD_ID_OPTARIF    1
MyMessage msgVAR2( 0, V_VAR2 );

#define CHILD_ID_ISOUSC     2
MyMessage msgCURRENT( 0, V_CURRENT );

#define CHILD_ID_PTEC       3
MyMessage msgVAR3( 0, V_VAR3 );

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
MyMessage msgVAR5( 0, V_VAR5 );

// used by HCHP/EJP/BBR
#define CHILD_ID_HHPHC      50
MyMessage msgVAR4( 0, V_VAR4 );

MyMessage msgWATT( 0, V_WATT );       // actually VA
MyMessage msgKWH( 0, V_KWH );         // actually WH

// send the full string as 'label=value'
#define CHILD_ID_THREAD     60

// teleInfo
// - rxPin:  D4 (PCINT2 - Port D)
// - ledPin: D5
// - hcPin:  D6
// - hpPin:  D7
teleInfo TI( 4, 5, 6, 7 );

uint8_t tarif = 0;
teleInfo_t last;                // last sent teleinfo datas
teleInfo_t currentTI;           // last received teleinfo datas

static const char * const ti_teleInfo = "[teleInfo] ";
static const char * const ti_changed  = " changed from ";
static const char * const ti_to       = " to ";

/*
 * Compare 2 valeurs
 * Si il y a eu un changement, le sauvegarde et l'envoie à la gateway
 */
void compareTI( const char *label, uint32_t value, uint32_t &last, MyMessage &msg, int SENSOR_ID, uint8_t *plast = NULL )
{
    if( 0 ){
        Serial.print( "(uint32_t) label=" ); Serial.println( label );
    }
    if( last == value ){
        if( !plast || *plast == thisday ) return;
    }
#ifdef DEBUG_ENABLED
    Serial.print( ti_teleInfo ); Serial.print( label );
    Serial.print( ti_changed );  Serial.print( last );
    Serial.print( ti_to );       Serial.println( value );
#endif
    if( plast ){
        *plast = thisday;
    }
    last = value;
#ifdef HAVE_NRF24_RADIO
    send( msg.setSensor( SENSOR_ID ).set( last ));
#endif
}

void compareTI( const char *label, uint8_t value, uint8_t &last, MyMessage &msg, int SENSOR_ID, uint8_t *plast = NULL )
{
    if( 0 ){
        Serial.print( "(uint8_t) label=" ); Serial.println( label );
    }
    if( last == value ){
        if( !plast || *plast == thisday ) return;
    }
#ifdef DEBUG_ENABLED
    Serial.print( ti_teleInfo ); Serial.print( label );
    Serial.print( ti_changed );  Serial.print( last );
    Serial.print( ti_to );       Serial.println( value );
#endif
    if( plast ){
        *plast = thisday;
    }
    last = value;
#ifdef HAVE_NRF24_RADIO
    send( msg.setSensor( SENSOR_ID ).set( last ) );
#endif
}

void compareTI( const char *label, char value, char &last, MyMessage &msg, int SENSOR_ID, uint8_t *plast = NULL )
{
    if( 0 ){
        Serial.print( "(char &) label=" ); Serial.println( label );
    }
    if( last == value ){
        if( !plast || *plast == thisday ) return;
    }
#ifdef DEBUG_ENABLED
    Serial.print( ti_teleInfo ); Serial.print( label );
    Serial.print( ti_changed );  Serial.print( last );
    Serial.print( ti_to );       Serial.println( value );
#endif
    if( plast ){
        *plast = thisday;
    }
    last = value;
#ifdef HAVE_NRF24_RADIO
    send( msg.setSensor( SENSOR_ID ).set( last ) );
#endif
}

void compareTI( const char *label, char *value, char *last, MyMessage &msg, int SENSOR_ID, uint8_t *plast = NULL )
{
    if( 0 ){
        Serial.print( "(char *) label=" ); Serial.println( label );
    }
    if( strcmp( last, value ) == 0 ){
        if( !plast || *plast == thisday ) return;
    }
#ifdef DEBUG_ENABLED
    Serial.print( ti_teleInfo ); Serial.print( label );
    Serial.print( ti_changed );  Serial.print( last );
    Serial.print( ti_to );       Serial.println( value );
#endif
    if( plast ){
        *plast = thisday;
    }
    memset( last, 0, TI_BUFSIZE );
    strcpy( last, value );
#ifdef HAVE_NRF24_RADIO
    send( msg.setSensor( SENSOR_ID ).set( last ) );
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
        send( msgVAR1.setSensor( CHILD_ID_THREAD ).set( str ) );
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
    memset( &last, '\0', sizeof( teleInfo_t ));
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
        present( CHILD_ID_ADCO,    S_POWER );
        present( CHILD_ID_OPTARIF, S_POWER );
        present( CHILD_ID_ISOUSC,  S_POWER );
        present( CHILD_ID_PTEC,    S_POWER );
        present( CHILD_ID_IINST,   S_POWER );
        present( CHILD_ID_ADPS,    S_POWER );
        present( CHILD_ID_IMAX,    S_POWER );
        present( CHILD_ID_PAPP,    S_POWER );
        present( CHILD_ID_THREAD,  S_POWER );

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
// we accept commands:
// - to child_sensor_id, with command, with type, with payload   <var>                                   <value>
//   CHILD_ID_ADCO       set (1),      V_VAR1     <var>=<value>  hTC for honorThreadCb (case sensitive)  "1"|"0"
//   <all_sensors>       req (2),      V_VAR1     <var>
bool receive_teleinfo( const MyMessage &msg )
{
#ifdef HAVE_NRF24_RADIO
    if( msg.sensor == CHILD_ID_ADCO && mGetCommand( msg ) == C_SET ){
        TI.set_from_str( msg.getString());
        return( true );
    }
    if( mGetCommand( msg ) == C_REQ ){
        if( msg.sensor == CHILD_ID_ADCO ){
            send( msgVAR1.setSensor( msg.sensor ).set( last.ADCO ));
            return( true );
        } else if( msg.sensor == CHILD_ID_OPTARIF ){
            send( msgVAR2.setSensor( msg.sensor ).set( last.OPTARIF ));
            return( true );
        } else if( msg.sensor == CHILD_ID_ISOUSC ){
            send( msgCURRENT.setSensor( msg.sensor ).set( last.ISOUSC ));
            return( true );
        } else if( msg.sensor == CHILD_ID_OPTARIF ){
            send( msgVAR2.setSensor( msg.sensor ).set( last.OPTARIF ));
            return( true );
        } else if( msg.sensor == CHILD_ID_PTEC ){
            send( msgVAR2.setSensor( msg.sensor ).set( last.PTEC ));
            return( true );
        } else if( msg.sensor == CHILD_ID_IINST ){
            send( msgCURRENT.setSensor( msg.sensor ).set( last.IINST ));
            return( true );
        } else if( msg.sensor == CHILD_ID_ADPS ){
            send( msgCURRENT.setSensor( msg.sensor ).set( last.ADPS ));
            return( true );
        } else if( msg.sensor == CHILD_ID_IMAX ){
            send( msgCURRENT.setSensor( msg.sensor ).set( last.IMAX ));
            return( true );
        } else if( msg.sensor == CHILD_ID_PAPP ){
            send( msgWATT.setSensor( msg.sensor ).set( last.PAPP ));
            return( true );
        } else if( msg.sensor == CHILD_ID_HC_HC ){
            send( msgKWH.setSensor( msg.sensor ).set( last.HC_HC ));
            return( true );
        } else if( msg.sensor == CHILD_ID_HC_HP ){
            send( msgKWH.setSensor( msg.sensor ).set( last.HC_HP ));
            return( true );
        } else if( msg.sensor == CHILD_ID_HHPHC ){
            send( msgVAR4.setSensor( msg.sensor ).set( last.HHPHC ));
            return( true );
        }
    }
#endif // HAVE_NRF24_RADIO
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
}

void setup()  
{
#ifdef DEBUG_ENABLED
    Serial.begin( 115200 );
    Serial.println( F( "[setup]" ));
#endif
}

void loop()
{
#ifdef DEBUG_ENABLED
    Serial.println( F( "[loop]" ));
#endif
#ifdef HAVE_TELEINFO
    time_t t = now();
    thisday = day( t );
#ifdef DEBUG_ENABLED
    Serial.print( F( "[loop] thisday=" )); Serial.println( thisday );
#endif
    /* Read customer tele-information
     *  only sending modified data to the gateway
     */
    if( tarif > 0 && TI.get( &currentTI )){
        compareTI( TI_ADCO,    currentTI.ADCO,      last.ADCO,      msgVAR1,    CHILD_ID_ADCO,     &last.last_adco );     // char *
        compareTI( TI_OPTARIF, currentTI.OPTARIF,   last.OPTARIF,   msgVAR2,    CHILD_ID_OPTARIF,  &last.last_optarif );  // char *
        compareTI( TI_ISOUSC,  currentTI.ISOUSC,    last.ISOUSC,    msgCURRENT, CHILD_ID_ISOUSC,   &last.last_isousc );   // uint8_t
        compareTI( TI_PTEC,    currentTI.PTEC,      last.PTEC,      msgVAR3,    CHILD_ID_PTEC );
        compareTI( TI_IINST,   currentTI.IINST,     last.IINST,     msgCURRENT, CHILD_ID_IINST );
        compareTI( TI_ADPS,    currentTI.ADPS,      last.ADPS,      msgCURRENT, CHILD_ID_ADPS );
        compareTI( TI_IMAX,    currentTI.IMAX,      last.IMAX,      msgCURRENT, CHILD_ID_IMAX,     &last.last_imax );
        compareTI( TI_PAPP,    currentTI.PAPP,      last.PAPP,      msgWATT,    CHILD_ID_PAPP );
        switch( tarif ){
            case TI_TARIF_BASE:
                compareTI( TI_BASE,    currentTI.BASE,      last.BASE,      msgKWH,     CHILD_ID_BASE );
                break;
            case TI_TARIF_HCHP:
                compareTI( TI_HCHC,    currentTI.HC_HC,     last.HC_HC,     msgKWH,     CHILD_ID_HC_HC );
                compareTI( TI_HCHP,    currentTI.HC_HP,     last.HC_HP,     msgKWH,     CHILD_ID_HC_HP );
                compareTI( TI_HHPHC,   currentTI.HHPHC,     last.HHPHC,     msgVAR4,    CHILD_ID_HHPHC,     &last.last_hhphc );   // char &
                break;
            case TI_TARIF_EJP:
                compareTI( TI_EJPHN,   currentTI.EJP_HN,    last.EJP_HN,    msgKWH,     CHILD_ID_EJP_HN );
                compareTI( TI_EJPHPM,  currentTI.EJP_HPM,   last.EJP_HPM,   msgKWH,     CHILD_ID_EJP_HPM );
                compareTI( TI_PEJP,    currentTI.PEJP,      last.PEJP,      msgKWH,     CHILD_ID_PEJP );
                compareTI( TI_HHPHC,   currentTI.HHPHC,     last.HHPHC,     msgVAR4,    CHILD_ID_HHPHC );
                break;
            case TI_TARIF_TEMPO:
                compareTI( TI_BBRHCJB, currentTI.BBR_HC_JB, last.BBR_HC_JB, msgKWH,     CHILD_ID_BBR_HC_JB );
                compareTI( TI_BBRHPJB, currentTI.BBR_HP_JB, last.BBR_HP_JB, msgKWH,     CHILD_ID_BBR_HP_JB );
                compareTI( TI_BBRHCJW, currentTI.BBR_HC_JW, last.BBR_HC_JW, msgKWH,     CHILD_ID_BBR_HC_JW );
                compareTI( TI_BBRHPJW, currentTI.BBR_HP_JW, last.BBR_HP_JW, msgKWH,     CHILD_ID_BBR_HP_JW );
                compareTI( TI_BBRHCJR, currentTI.BBR_HC_JR, last.BBR_HC_JR, msgKWH,     CHILD_ID_BBR_HC_JR );
                compareTI( TI_BBRHPJR, currentTI.BBR_HP_JR, last.BBR_HP_JR, msgKWH,     CHILD_ID_BBR_HP_JR );
                compareTI( TI_DEMAIN,  currentTI.DEMAIN,    last.DEMAIN,    msgVAR5,    CHILD_ID_DEMAIN );
                compareTI( TI_HHPHC,   currentTI.HHPHC,     last.HHPHC,     msgVAR4,    CHILD_ID_HHPHC );
                break;
        }
    }
#endif
    wait( 500 );
}

/*
 * Accepting from the gateway commands or request targeting the teleinfo
 */
void receive( const MyMessage &msg )
{
#ifdef HAVE_TELEINFO
    // receive_teleinfo() is expected to return true if it has consumed the message
    if( receive_teleinfo( msg ))
        return;
#endif // HAVE_TELEINFO
}

