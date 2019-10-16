/* **********************************************************************
 *             Objet decodeur de teleinformation client (TIC)
 *              format Linky "historique" ou anciens compteurs
 *              electroniques.
 *
 * Reference : ERDF-NOI-CPT_54E V1
 * V06 : MicroQuettas mars 2018
 *
 * pwi 2019- 9-22 adaptation to Enedis-NOI-CPT_54E v3 TIC standard
 ***********************************************************************/
#include <Arduino.h>
#include <core/MySensorsCore.h>
#include <pwiCommon.h>
#include "Linky.h"

// uncomment for debug this class
#define LINKY_DEBUG

/****************************** Macros ********************************/
#define SetBits(Data, Mask) \
Data |= Mask

#define ResetBits(Data, Mask) \
Data &= ~Mask

#define InsertBits(Data, Mask, Value) \
Data &= ~Mask;\
Data |= (Value & Mask)

#ifndef P1
#define P1(name) const char name[] PROGMEM
#endif

/***************************** Defines ***************************************/
#define CLy_Bds       9600                      /* Transmission speed in bds */
#define CLy_TxPin     8                         /* TX pin (unused but reserved) */

#define bLy_Rec      (0x01 << 0)                /* Receiving */
#define bLy_RxB      (0x01 << 1)                /* Receive in buffer B */
#define bLy_Cks      (0x01 << 2)                /* Check Cks */
#define bLy_GId      (0x01 << 3)                /* Group identification */
#define bLy_Dec      (0x01 << 4)                /* Decode */

#define Car_SP        0x20                      /* Char space */
#define Car_HT        0x09                      /* Horizontal tabulation */
#define Car_SOIG      0x0A                      /* start of information group, aka LF, aka \n */
#define Car_EOIG      0x0D                      /* end of information group, aka CR, aka \r */
#define Car_STX       0x02                      /* start of trame */
#define Car_ETX       0x03                      /* end of trame */

const char CLy_Sep[] = { Car_HT, '\0' };

#define CLy_MinLg     8                         /* Minimum useful message length */

#define bLy_adsc      (0x01 <<  0)      /* ADSC group available (has changed since last send) */
#define bLy_date      (0x01 <<  1)      /* DATE */
#define bLy_ngtf      (0x01 <<  2)      /* NGTF */
#define bLy_ltarf     (0x01 <<  3)      /* LTARF */
#define bLy_easf01    (0x01 <<  4)      /* EASF01 */
#define bLy_easf02    (0x01 <<  5)      /* EASF02 */
#define bLy_irms1     (0x01 <<  6)      /* IRMS1 */
#define bLy_urms1     (0x01 <<  7)      /* URMS1 */
#define bLy_pref      (0x01 <<  8)      /* PREF */
#define bLy_sinsts    (0x01 <<  9)      /* SINSTS */
#define bLy_smaxsn    (0x01 << 10)      /* SMAXSN */
#define bLy_smaxsnm1  (0x01 << 11)      /* SMAXSN-1 */
#define bLy_ccasn     (0x01 << 12)      /* CCASN */
#define bLy_ccasnm1   (0x01 << 13)      /* CCASN-1 */
#define bLy_prm       (0x01 << 14)      /* PRM */
#define bLy_ntarf     (0x01 << 15)      /* NATRF */
#define bLy_hchp      (0x01 << 16)      /* booleen HC/HP */

P1(PLy_adsc)      = "ADSC";
P1(PLy_date)      = "DATE";
P1(PLy_ngtf)      = "NGTF";
P1(PLy_ltarf)     = "LTARF";
P1(PLy_easf01)    = "EASF01";
P1(PLy_easf02)    = "EASF02";
P1(PLy_irms1)     = "IRMS1";
P1(PLy_urms1)     = "URMS1";
P1(PLy_pref)      = "PREF";
P1(PLy_sinsts)    = "SINSTS";
P1(PLy_smaxsn)    = "SMAXSN";
P1(PLy_smaxsnm1)  = "SMAXSN-1";
P1(PLy_ccasn)     = "CCASN";
P1(PLy_ccasnm1)   = "CCASN-1";
P1(PLy_prm)       = "PRM";
P1(PLy_ntarf)     = "NTARF";
P1(PLy_hchp)      = "HCHP";

P1(PLy_ngtf_HCHP) = "H PLEINE/CREUSE ";
P1(PLy_ltarf_HP)  = "  HEURE  PLEINE ";
P1(PLy_ltarf_HC)  = "  HEURE  CREUSE ";

// Frequency of the trame LED (slow if OK, fast else)
#define TRAMEOK_MS      3000
#define TRAMENOTOK_MS   1000

/*************** Constructor, methods and properties ******************/

/**
 * Linky::Linky:
 * @id: the starting identifier of this MySensors child.
 * @rxPin: the reception pin number.
 * @ledPin: the pin number to which the reception LED is attached.
 * @hcPin: the pin number to which the HC LED is attached.
 * @hpPin: the pin number to which the HP LED is attached.
 * 
 * Constructor.
 *
 * Public.
 */
Linky::Linky( uint8_t id, uint8_t rxPin, uint8_t ledPin, uint8_t hcPin, uint8_t hpPin ) : linkySerial( rxPin, CLy_TxPin)
{
    this->init();

    this->id = id;
    this->rxPin = rxPin;

    this->init_led( &this->ledPin, ledPin );
    this->init_led( &this->hcPin, hcPin );
    this->init_led( &this->hpPin, hpPin );
};

void Linky::init()
{
    this->ledPin = 0;
    this->hcPin = 0;
    this->hpPin = 0;
    this->_FR = 0;
    this->_DNFR = 0;
    this->_pRec = _BfA;                 /* Receive in A */
    this->_pDec = _BfB;                 /* Decode in B */
#ifdef LINKY_DEBUG
    //Serial.println( F( "init() set _pRec=_BfA, _pDec=_BfB" ));
#endif
    this->_iRec = 0;
    this->_iCks = 0;
    this->_GId = 0;
    this->stx_ms = 0;
    this->dup_thread = false;
};

// LED initialization
void Linky::init_led( uint8_t *dest, uint8_t pin )
{
    *dest = pin;

    if( pin > 0 ){
        this->ledOff( pin );
        pinMode( pin, OUTPUT );
    }
}

/**
 * Linky::ledOff:
 * @pin: the number of the pin to which the LED is attached.
 * 
 * Lights the LED off.
 *
 * Public.
 */
void Linky::ledOff( uint8_t pin )
{
    if( pin > 0 ){
        digitalWrite( pin, LOW );
    }
}

/**
 * Linky::ledOn:
 * @pin: the number of the pin to which the LED is attached.
 * 
 * Lights the LED on.
 *
 * Public.
 */
void Linky::ledOn( uint8_t pin )
{
    if( pin > 0 ){
        digitalWrite( pin, HIGH );
    }
}

/**
 * Linky::loop:
 * 
 * Manage all readings and interpretations :
 * - read and check information groups from soft serial
 * - extract them (and publish ?).
 *
 * Public.
 */
void Linky::loop()
{
    /* 1st part, last action : decode information */
    if( _FR & bLy_Dec ){
        ResetBits( _FR, bLy_Dec );
#ifdef LINKY_DEBUG
        Serial.print( _pDec );
#endif
        this->ig_decode();
#ifdef LINKY_DEBUG
        Serial.println();
#endif
    }

    /* 2th part, receiver processing - always run */
    this->ig_receive();
}

/**
 * Linky::present:
 * 
 * Présentation MySensors.
 *
 * Public.
 */
void Linky::present()
{
    uint8_t id = this->id;
#ifdef LINKY_DEBUG
    Serial.print( F( "Linky::present() id=" ));
    Serial.println( id );
#endif
    ::present( id,    S_POWER, PGMSTR( PLy_adsc ));
    ::present( id+ 1, S_POWER, PGMSTR( PLy_date ));
    ::present( id+ 2, S_POWER, PGMSTR( PLy_ngtf ));
    ::present( id+ 3, S_POWER, PGMSTR( PLy_ltarf ));
    ::present( id+ 4, S_POWER, PGMSTR( PLy_easf01 ));
    ::present( id+ 5, S_POWER, PGMSTR( PLy_easf02 ));
    ::present( id+ 6, S_POWER, PGMSTR( PLy_irms1 ));
    ::present( id+ 7, S_POWER, PGMSTR( PLy_urms1 ));
    ::present( id+ 8, S_POWER, PGMSTR( PLy_pref ));
    ::present( id+ 9, S_POWER, PGMSTR( PLy_sinsts ));
    ::present( id+10, S_POWER, PGMSTR( PLy_smaxsn ));
    ::present( id+11, S_POWER, PGMSTR( PLy_smaxsnm1 ));
    ::present( id+12, S_POWER, PGMSTR( PLy_ccasn ));
    ::present( id+13, S_POWER, PGMSTR( PLy_ccasnm1 ));
    ::present( id+14, S_POWER, PGMSTR( PLy_prm ));
    ::present( id+15, S_POWER, PGMSTR( PLy_ntarf ));
    ::present( id+16, S_POWER, PGMSTR( PLy_hchp ));
}

/**
 * Linky::send:
 * 
 * Unconditionally send the informations.
 *
 * Public.
 */
void Linky::send( bool all /*=false*/ )
{
    MyMessage msg;
    uint8_t id = this->id;

    if( all || ( this->_DNFR & bLy_adsc )){
        msg.clear();
        ::send( msg.setSensor( id ).setType( V_VAR1 ).set( this->tic.adsc ));
    }
    if( all || ( this->_DNFR & bLy_date )){
        msg.clear();
        ::send( msg.setSensor( id+1 ).setType( V_VAR1 ).set( this->tic.date ));
    }
    if( all || ( this->_DNFR & bLy_ngtf )){
        msg.clear();
        ::send( msg.setSensor( id+2 ).setType( V_VAR1 ).set( this->tic.ngtf ));
    }
    if( all || ( this->_DNFR & bLy_ltarf )){
        msg.clear();
        ::send( msg.setSensor( id+3 ).setType( V_VAR1 ).set( this->tic.ltarf ));
    }
    if( all || ( this->_DNFR & bLy_easf01 )){
        msg.clear();
        ::send( msg.setSensor( id+4 ).setType( V_KWH ).set( this->tic.easf01 ));
    }
    if( all || ( this->_DNFR & bLy_easf02 )){
        msg.clear();
        ::send( msg.setSensor( id+5 ).setType( V_KWH ).set( this->tic.easf02 ));
    }
    if( all || ( this->_DNFR & bLy_irms1 )){
        msg.clear();
        ::send( msg.setSensor( id+6 ).setType( V_CURRENT ).set( this->tic.irms1 ));
    }
    if( all || ( this->_DNFR & bLy_urms1 )){
        msg.clear();
        ::send( msg.setSensor( id+7 ).setType( V_VOLTAGE ).set( this->tic.urms1 ));
    }
    if( all || ( this->_DNFR & bLy_pref )){
        msg.clear();
        ::send( msg.setSensor( id+8 ).setType( V_WATT ).set( this->tic.pref ));
    }
    if( all || ( this->_DNFR & bLy_sinsts )){
        msg.clear();
        ::send( msg.setSensor( id+9 ).setType( V_WATT ).set( this->tic.sinsts ));
    }
    /*
    if( all || ( this->_DNFR & bLy_smaxsn )){
        msg.clear();
        ::send( msg.setSensor( id+10 ).setType( V_WATT ).set( this->tic.smaxsn ));
    }
    if( all || ( this->_DNFR & bLy_smaxsnm1 )){
        msg.clear();
        ::send( msg.setSensor( id+11 ).setType( V_WATT ).set( this->tic.smaxsnm1 ));
    }
    if( all || ( this->_DNFR & bLy_ccasn )){
        msg.clear();
        ::send( msg.setSensor( id+12 ).setType( V_WATT ).set( this->tic.ccasn ));
    }
    if( all || ( this->_DNFR & bLy_ccasnm1 )){
        msg.clear();
        ::send( msg.setSensor( id+13 ).setType( V_WATT ).set( this->tic.ccasnm1 ));
    }
    */
    if( all || ( this->_DNFR & bLy_prm )){
        msg.clear();
        ::send( msg.setSensor( id+14 ).setType( V_VAR1 ).set( this->tic.prm ));
    }
    if( all || ( this->_DNFR & bLy_ntarf )){
        msg.clear();
        ::send( msg.setSensor( id+15 ).setType( V_VAR1 ).set( this->tic.ntarf ));
    }
    if( all || ( this->_DNFR & bLy_hchp )){
        msg.clear();
        ::send( msg.setSensor( id+16 ).setType( V_VAR1 ).set( this->tic.hchp ));
    }
    this->_DNFR = 0;
}

/**
 * Linky::setDup:
 * @dup: whether the next trame should be duplicated.
 *
 * The duplication period is managed by the main program, which takes care of setting
 * this flag on and off.
 *
 * Public.
 */
void Linky::setDup( bool dup )
{
    this->dup_thread = dup;
}

/**
 * Linky::setup:
 * @min_period_ms: minimal period for sending changes (max frequency).
 * @max_period_ms: maximal period for sending data (unchanged timeout).
 * 
 * Initial configuration.
 * To be called at setup() time from the main program.
 *
 * Public.
 */
void Linky::setup( uint32_t min_period_ms, uint32_t max_period_ms )
{
#ifdef LINKY_DEBUG
    Serial.print( F( "Linky::setup() sizeof(tic_t)=" )); Serial.println( sizeof( tic_t ));
#endif

    /* Initialize the SoftwareSerial */
    uint8_t rxPin = this->rxPin;
    pinMode( rxPin, INPUT );
    pinMode( CLy_TxPin, OUTPUT );
    linkySerial.begin( CLy_Bds );

    /* setup the min_period (max frequency) and max_period (unchanged timeout) timers
     */
    this->min_period.setup( "MinPeriod", min_period_ms, false, Linky::MinPeriodCb, this );
    this->min_period.start();
    this->max_period.setup( "MaxPeriod", max_period_ms, false, Linky::MaxPeriodCb, this );
    this->max_period.start();

    /* setup the trame indicator timers
     *  the status LED  is visible on the front panel, we so manage it with a hardware timer
     *  light up delay is 0.1s (0 +0.1) -> set up with 150 ms
     */
    this->led_on_timer.setup( "LedOn", 150, true, Linky::TrameLedOnCb, this );
    this->led_status_timer.setup( "Status", TRAMEOK_MS, false, Linky::TrameLedStatusCb, this );
    this->timeout_timer.setup( "Timeout", 10000, true, Linky::TrameTimeoutCb, this );

    /* light on the trame led */
    this->trameLedSet( TRAMENOTOK_MS );
}

/**
 * Linky::checkHorodate:
 * @p: the string to be checked.
 * 
 * Returns: %TRUE if the string is a valid date.
 * 
 * Cf. Enedis-NOI-CPT_54E.pdf § 6.2.1.1. Format des horodates.
 *
 * Private.
 */
bool Linky::checkHorodate( const char *p )
{
    if( strlen( p ) != 13 ){
        return( false );
    }
    if( p[0]!='H' && p[0]!='h' && p[0]!='E' && p[0]!='e' ){
        return( false );
    }
    for( uint8_t i=1 ; p[i] ; ++i ){
        if( !isdigit( p[i] )){
            return( false );
        }
    }
    return( true );
}

/**
 * Linky::decData:
 * @dest: the tic_t destination member.
 * @mask: the corresponding bitmask of _DNFR.
 * 
 * Decode and store the received data.
 * 
 * Returns: %TRUE if the data has been actually updated.
 *
 * Private.
 */
bool Linky::decData( char *dest, uint32_t mask )
{
    _pDec = strtok( NULL, CLy_Sep );
    bool valid = true;
    bool hchp = false;

    // check data validity
    // ADSC = 12 digits
    if( mask == bLy_adsc ){
        if( strlen( _pDec ) != LINKY_ADSC_SIZE ){
            valid = false;
        } else {
            for( uint8_t i=0 ; _pDec[i] ; ++i ){
                if( !isdigit( _pDec[i] )){
                    valid = false;
                    break;
                }
            }
        }

    } else if( mask == bLy_date ){
        valid = this->checkHorodate( _pDec );

    } else if( mask == bLy_ngtf ){
        if( strcmp_P( _pDec, PLy_ngtf_HCHP ) != 0 ){
            valid = false;
        }

    } else if( mask == bLy_ltarf ){
        if( !strcmp_P( _pDec, PLy_ltarf_HP )){
            hchp = true;
        } else if( !strcmp_P( _pDec, PLy_ltarf_HC )){
            hchp = false;
        } else {
            valid = false;
        }

    } else if( mask == bLy_prm ){
        if( strlen( _pDec ) != LINKY_PRM_SIZE ){
            valid = false;
        } else {
            for( uint8_t i=0 ; _pDec[i] ; ++i ){
                if( !isdigit( _pDec[i] )){
                    valid = false;
                    break;
                }
            }
        }
    }

    if( valid ){
        if( strcmp( _pDec, dest ) != 0 ){
            strcpy( dest, _pDec );
            SetBits( _DNFR, mask );
    
            if( mask == bLy_ltarf ){
                if( hchp ){
                    this->ledOff( this->hcPin );
                    this->ledOn( this->hpPin );
                } else {
                    this->ledOff( this->hpPin );
                    this->ledOn( this->hcPin );
                }
                this->tic.hchp = hchp;
                SetBits( _DNFR, bLy_hchp );
            }
        }
    }

    return( _DNFR & mask );
}

bool Linky::decData( uint8_t *dest, uint32_t mask )
{
    _pDec = strtok( NULL, CLy_Sep );
    bool valid = true;
    uint8_t uint = ( uint8_t ) atoi( _pDec );

    if( mask == bLy_irms1 ){
        valid = ( uint != 0 );

    } else if( mask == bLy_pref ){
        valid = ( uint != 0 && uint != 1 );

    } else if( mask == bLy_ntarf ){
        valid = ( uint > 0 );
    }

    if( valid ){
        if( uint != *dest ){
            *dest = uint;
            SetBits( _DNFR, mask );
        }
    }

    return( _DNFR & mask );
}

bool Linky::decData( uint16_t *dest, uint32_t mask )
{
    _pDec = strtok( NULL, CLy_Sep );
    uint16_t uint = ( uint16_t ) atoi( _pDec );
    bool valid = true;

    if( mask == bLy_urms1 ){
        if( uint < 150 ){
            valid = false;
        } else {
            float fcent = ((( float ) uint / ( float ) *dest )) - 1.;
            fcent = abs( fcent );
            fcent *= 100.;
            if(( uint8_t ) fcent > 25 ){
                valid = false;
            }
        }

    } else if( mask == bLy_sinsts ){
        uint16_t estim = tic.irms1 * tic.urms1;
        float fcent = ((( float ) estim / ( float ) uint )) - 1.;
        fcent = abs( fcent );
        fcent *= 100.;
        if(( uint8_t ) fcent > 25 ){
            valid = false;
        }
    }

    if( valid ){
        if( uint != *dest ){
            *dest = uint;
            SetBits( _DNFR, mask );
        }
    }

    return( _DNFR & mask );
}

bool Linky::decData( uint32_t *dest, uint32_t mask )
{
    bool valid = true;
    _pDec = strtok( NULL, CLy_Sep );
    uint32_t ulong = atol( _pDec );

    if( mask == bLy_easf01 || mask == bLy_easf02 ){
        if( ulong < *dest ){
            valid = false;
        }
    }

    if( valid ){
        if( ulong != *dest ){
            *dest = ulong;
            SetBits( _DNFR, mask );
        }
    }

    return( _DNFR & mask );
}

bool Linky::decData( horodate_t *dest, uint32_t mask )
{
    _pDec = strtok( NULL, CLy_Sep );
    char *pval = strtok( NULL, CLy_Sep );

    bool valid = this->checkHorodate( _pDec );

    if( valid ){
        if( strcmp( _pDec, dest->date ) != 0 ){
            strncpy( dest->date, _pDec, LINKY_DATE_SIZE );
            uint16_t ulong = atol( pval );
            dest->value = ulong;
            SetBits( _DNFR, mask );
        }
    }

    return( _DNFR & mask );
}

/**
 * Linky::dupThread:
 * 
 * Duplicate the trames reception to the output thread is asked for.
 * A buffer is built with trimmed label and values, '|'-separated.
 *
 * Private.
 */
void Linky::dupThread()
{
    if( this->dup_thread ){

        char buffer[1+MAX_PAYLOAD];   // MySensors max payload is 25 bytes
        memset( buffer, '\0', sizeof( buffer ));
        uint8_t len = 0;
    
        // label
        char *p = strtok( buffer, CLy_Sep );
        String str = p;
        str.trim();
        strncpy( buffer, str.c_str(), MAX_PAYLOAD );
        len = str.length();
    
        // horodate/value
        p = strtok( NULL, CLy_Sep );
        if( p[0] && len<MAX_PAYLOAD-1 ){
            buffer[len] = '|';
            len += 1;
            str = p;
            str.trim();
            strncat( buffer, p, MAX_PAYLOAD-len );
        }
    
        // value
        p = strtok( NULL, CLy_Sep );
        if( p[0] && len<MAX_PAYLOAD-1 ){
            buffer[len] = '|';
            len += 1;
            str = p;
            str.trim();
            strncat( buffer, p, MAX_PAYLOAD-len );
        }

        MyMessage msg;
        msg.clear();
        ::send( msg.setSensor( 5 ).setType( V_VAR1 ).set( buffer ));
    }
}

/**
 * Linky::ig_checksum:
 * 
 * Checksum validation of the information group in the reception buffer.
 * 
 * Returns: %TRUE if checksum is OK.
 *
 * Private.
 */
bool Linky::ig_checksum()
{
    bool ok = true;
    uint16_t cks = 0;
    if( _iCks >= CLy_MinLg ){               /* Message is long enough */
        for( uint8_t i=0; i<_iCks; i++ ){
            cks += *(_pRec+i);
#ifdef LINKY_DEBUG
            /*
            Serial.print( F( "i=" ));
            Serial.print( i );
            Serial.print( F( ", ch=" ));
            Serial.print( *(_pRec + i));
            Serial.print( F( " (0x" ));
            Serial.print( *(_pRec + i), HEX );
            Serial.println( F( ")" ));
            */
#endif
        }
        cks &= 0x3f;
        cks += Car_SP;
        if( cks == *(_pRec+_iCks )){        /* checksum is ok */
#ifdef LINKY_DEBUG
            //Serial.print( _pRec );
            //Serial.println( F( " checksum OK" ));
#endif
            *(_pRec+_iCks-1) = '\0';        /* Terminate the string just before the last TAB before the Cks */

        /* checksum error, cancel the received buffer */
        } else {
            Serial.print( _pRec );
            Serial.print( F( " checksum error: computed=0x" ));
            Serial.print( cks, HEX );
            Serial.print( F(", received=0x"));
            Serial.println( *(_pRec+_iCks), HEX );
            ok = false;
        }   
    } else {
        Serial.print( _pRec );
        Serial.println( F( " not enough received data" ));
        ok = false;
    }

    return( ok );
}

/**
 * Linky::ig_decode:
 * 
 * Decode the information group available in the _pDec decode buffer.
 * Stores the information in the tic structure.
 * Set the 'new data' bit in _DNFR.
 *
 * Private.
 */
void Linky::ig_decode()
{
    bool found = false;
    this->dupThread();
    _pDec = strtok( _pDec, CLy_Sep );

    if( !strcmp_P( _pDec, PLy_adsc )){
        found = true;
        this->decData(( char * ) this->tic.adsc, bLy_adsc );
      
    } else if( !strcmp_P( _pDec, PLy_date )){
        found = true;
        this->decData(( char * ) this->tic.date, bLy_date );
      
    } else if( !strcmp_P( _pDec, PLy_ngtf )){
        found = true;
        this->decData(( char * ) this->tic.ngtf, bLy_ngtf );
      
    } else if( !strcmp_P( _pDec, PLy_ltarf )){
        found = true;
        this->decData(( char * ) this->tic.ltarf, bLy_ltarf );
      
    } else if( !strcmp_P( _pDec, PLy_easf01 )){
        found = true;
        this->decData( &this->tic.easf01, bLy_easf01 );
      
    } else if( !strcmp_P( _pDec, PLy_easf02 )){
        found = true;
        this->decData( &this->tic.easf02, bLy_easf02 );
      
    } else if( !strcmp_P( _pDec, PLy_irms1 )){
        found = true;
        this->decData( &this->tic.irms1, bLy_irms1 );
      
    } else if( !strcmp_P( _pDec, PLy_urms1 )){
        found = true;
        this->decData( &this->tic.urms1, bLy_urms1 );
      
    } else if( !strcmp_P( _pDec, PLy_pref )){
        found = true;
        this->decData( &this->tic.pref, bLy_pref );
      
    } else if( !strcmp_P( _pDec, PLy_sinsts )){
        found = true;
        this->decData( &this->tic.sinsts, bLy_sinsts );
      
    } else if( !strcmp_P( _pDec, PLy_smaxsn )){
        found = true;
        this->decData( &this->tic.smaxsn, bLy_smaxsn );
      
    } else if( !strcmp_P( _pDec, PLy_smaxsnm1 )){
        found = true;
        this->decData( &this->tic.smaxsnm1, bLy_smaxsnm1 );
      
    } else if( !strcmp_P( _pDec, PLy_ccasn )){
        found = true;
        this->decData( &this->tic.ccasn, bLy_ccasn );
      
    } else if( !strcmp_P( _pDec, PLy_ccasnm1 )){
        found = true;
        this->decData( &this->tic.ccasnm1, bLy_ccasnm1 );
      
    } else if( !strcmp_P( _pDec, PLy_prm )){
        found = true;
        this->decData(( char * ) this->tic.prm, bLy_prm );
      
    } else if( !strcmp_P( _pDec, PLy_ntarf )){
        found = true;
        this->decData( &this->tic.ntarf, bLy_ntarf );
    }

#ifdef LINKY_DEBUG
    if( !found ){
        Serial.print( F( " ignored" ));
    } else {
        Serial.print( F( " ok" ));
    }
#endif
}

/**
 * Linky::ig_receive:
 * 
 * Get the new information group from the serial input,
 *  and stores it in the current _pRec reception buffer.
 *  Validates the checksum.
 *  Switch the reception buffer ater checksum validation.
 *  
 * The method exits either at end of information group (and checksum valide), or if
 * there is no more available character in the serial input.
 *
 * Private.
 */
void Linky::ig_receive()
{
    while( linkySerial.available()){                   /* At least 1 char has been received */
        char c = linkySerial.read() & 0x7f;            /* Read char, exclude parity */
#ifdef LINKY_DEBUG
        //Serial.print( F( "Serial.read()  c=" ));
        //Serial.println( c, HEX );
#endif
        /* On going reception */
        if( _FR & bLy_Rec ){
            /* Received end of information group char, aka CR, aka \r, aka 0x0D */
            if( c == Car_EOIG ){   
                ResetBits(_FR, bLy_Rec);        /* Receiving complete */
                _iCks = _iRec-1;                /* Index of Cks in the message */
                *(_pRec+_iRec) = '\0';          /* Terminate the string */
#ifdef LINKY_DEBUG
                //Serial.print( F( "Found EOIG=" ));
                //Serial.println( _pRec );
#endif
                /* if checksum is OK, swap the buffers and decode the group */
                if( this->ig_checksum()){
                    SetBits( _FR, bLy_Dec );        /* Next step, decoding group information */
                    /* Swap reception and decode buffers */
                    if( _FR & bLy_RxB ){            /* Receiving in B, Decode in A, swap */
                        ResetBits(_FR, bLy_RxB);
                        _pRec = _BfA;               /* --> Receive in A */
                        _pDec = _BfB;               /* --> Decode in B */
#ifdef LINKY_DEBUG
                        //Serial.println( F( "receive() set _pRec=_BfA, _pDec=_BfB" ));
#endif
                    } else {                        /* Receiving in A, Decode in B, swap */
                        SetBits(_FR, bLy_RxB);
                        _pRec = _BfB;               /* --> Receive in B */
                        _pDec = _BfA;               /* --> Decode in A */
#ifdef LINKY_DEBUG
                        //Serial.println( F( "receive() set _pRec=_BfB, _pDec=_BfA" ));
#endif
                    }
                /* if checksum is not ok, keep the same buffer */
                } else {
                    _iRec = 0;
                }

            /* Other character during information group reception */
            } else {
                *(_pRec+_iRec) = c;             /* Store received character */
                _iRec += 1;
                if( _iRec >= LINKY_BUFSIZE-1 ){       /* Buffer overflow */
                    ResetBits(_FR, bLy_Rec);          /* Stop reception and do nothing */
                    *(_pRec+LINKY_BUFSIZE-1) = '\0';
                    Serial.print( _pRec );
                    Serial.print( F( " buffer overflow (" ));
                    Serial.print( LINKY_BUFSIZE );
                    Serial.println( F( " bytes)" ));
                }
            }

        /* Reception not yet started, trying to start it 
            At startup, wait until we have catch the start of the trame */
        } else if( this->stx_ms > 0 && c == Car_SOIG ){   /* Received start of information group */
            _iRec = 0;
            SetBits(_FR, bLy_Rec);   /* Start reception */

        /* start of trame */
        } else if( c == Car_STX ){
            this->stx_ms = millis();

        /* if end of trame 
            we assume that we get one trame every sec. at 9600 bauds */
        } else if( c == Car_ETX ){
            uint32_t now = millis();
            uint32_t delay = now - this->stx_ms;
#ifdef LINKY_DEBUG
            Serial.print( F( "EOT found, delay=" ));
            Serial.println( delay );
#endif
            this->dup_thread = false;
            this->trameLedSet( delay < 1500 ? TRAMEOK_MS : TRAMENOTOK_MS );
        }
    }
}

/**
 * Linky::trameLedSet:
 * @period_ms: blinking period of the trame LED:
 *  3 sec. if OK
 *  1 sec. if not OK.
 * 
 * Set the LED blink statut.
 * 
 * According to Enedis-NOI-CPT_54E v3, trame LED should blink:
 * - slowly if OK:
 *    freq = 1/3 Hz
 *    on 0,1 s
 *    off 2,9 s
 * -fast if not OK
 *    freq = 1 Hz
 *    on 0,1 s
 *    off 0,9 s
 *
 * This is managed with timers :
 * - one timer lights on the LED, depending of the desired frequency (3s if OK, 1s else)
 * - one timer lights off the LED, 0,1s after
 *
 * Private.
 */
void Linky::trameLedSet( uint32_t period_ms )
{
#ifdef LINKY_DEBUG
    Serial.print( F( "trameLedSet() period_ms=" ));
    Serial.println( period_ms );
#endif
    if( this->led_status_timer.getDelay() != period_ms ){

        this->led_status_timer.setDelay( period_ms );
        this->led_status_timer.restart();
    }

    if( period_ms == TRAMEOK_MS ){
        this->timeout_timer.restart();
    } else {
        this->timeout_timer.stop();
    }
}

/**
 * Linky::MaxPeriodCb:
 * @user_data: a pointer to the Linky instance.
 * 
 * On max period, it is time to send all unchanged data.
 *
 * Static private.
 */
void Linky::MaxPeriodCb( void *user_data )
{
    Linky *instance = ( Linky *) user_data;
    instance->send( true );
}

/**
 * Linky::MinPeriodCb:
 * @user_data: a pointer to the Linky instance.
 * 
 * On min period, it is time to send changed data.
 *
 * Static private.
 */
void Linky::MinPeriodCb( void *user_data )
{
    Linky *instance = ( Linky *) user_data;
    instance->send( false );
}

/**
 * Linky::TrameTimeoutCb:
 * @data: a pointer to the Linky instance.
 * 
 * Monitor the TIC reception.
 * The LED status must be set to NOTOK when no trame has been received after a 10s timeout.
 * This timer is configured to run continuously, with a 10s period.
 * It is (re) started each time a new trame has been received.
 *
 * Static private.
 */
void Linky::TrameTimeoutCb( void *data )
{
#ifdef LINKY_DEBUG
    Serial.println( F( "Linky::TrameTimeoutCb()" ));
#endif
    Linky *instance = ( Linky *) data;
    instance->trameLedSet( TRAMENOTOK_MS );
}

/**
 * Linky::TrameLedOnCb:
 * @data: a pointer to the Linky instance.
 * 
 * This timer is configured to run once with a 0.1 sec delay.
 * It is started when the LED is lighted on.
 * The LED must be lighted off at timer expiration.
 *
 * Static private.
 */
void Linky::TrameLedOnCb( void *data )
{
    Linky *instance = ( Linky *) data;
    instance->ledOff( instance->ledPin );
}

/**
 * Linky::TrameLedStatusCb:
 * @data: a pointer to the Linky instance.
 * 
 * This timer is configured to run continuously with a period depending of the LED status.
 * It is (re)started each time the status is changed.
 *
 * Static private.
 */
void Linky::TrameLedStatusCb( void *data )
{
    Linky *instance = ( Linky *) data;
    instance->ledOn( instance->ledPin );
    instance->led_on_timer.start();
}

