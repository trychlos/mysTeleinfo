#include "teleInfo.h"

// uncomment for debugging TeleInfo class
//#define TI_DEBUG_READ_NEXT
//#define TI_DEBUG_READ_LABEL
//#define TI_DEBUG_GET_LABEL
#define TI_DEBUG

/*
 * At 1200 bps, we may expect 120 characters by second, or one character
 * each 1/120e sec (~10ms). But two groups are elapsed from about (though
 * less than) 400ms. A read timeout of 1000ms before considering that
 * TeleInfo is not available is thus more than enough.
 */
#define TI_READ_TIMEOUT           1000

/*
 * Returns the tarif as a const string
 */
const char *tarifToStr( uint8_t tarif )
{
    switch( tarif ){
        case TI_TARIF_BASE:
            return( TI_BASE );
        case TI_TARIF_TEMPO:
            return( "TEMPO" );
        case TI_TARIF_EJP:
            return( "EJP" );
        case TI_TARIF_HCHP:
            return( TI_HCHP );
        default:
            break;
    }
    return( "UNKNOWN" );
}

/**
 * teleInfo:
 * @rxPin: the pin number from which we get the serial data.
 * @ledPin: [allow-none]: the pin number to which the thread LED is connected;
 *  LED is set ON when ADCO label is found, until MOTDETAT where LED is set OFF.
 * @hcPin: [allow-none]: the pin number to which HC LED is connected.
 *  HC LED is ON during low price hours.
 * @hpPin: [allow-none]: the pin number to which HP LED is connected;
 *  HP LED is ON during full price hours.
 * 
 * Constructor.
 */
teleInfo::teleInfo( uint8_t rxPin, uint8_t ledPin, uint8_t hcPin, uint8_t hpPin )
{
    this->haveTeleinfo = false;
    
  	// pas sur que ce soit nécessaire, mais la doc de SoftwareSerial l'indique
    //pinMode( rxPin, INPUT );
	  tiSerial = new pwiSoftwareSerial( rxPin );
	  tiSerial->begin( 1200 );
    tiSerial->listen();

    this->init_led( &this->ledPin, ledPin );
    this->init_led( &this->hcPin, hcPin );
    this->init_led( &this->hpPin, hpPin );

    this->cb = NULL;
    this->honorThreadCb = false;
    this->rateThreadCb = 10;
    this->countThreadCb = 0;
}

// destructor
teleInfo::~teleInfo()
{
	  if( tiSerial != NULL ){
        delete tiSerial;
    }
    digitalWrite( this->ledPin, LOW );
}

// LED initialization
void teleInfo::init_led( uint8_t *dest, uint8_t pin )
{
    *dest = pin;

    if( pin > 0 ){
        led_off( pin );
        pinMode( pin, OUTPUT );
    }
}

// make a LED ON if pin number is set
void teleInfo::led_on( uint8_t pin )
{
    if( pin > 0 ){
        digitalWrite( pin, HIGH );
    }
}

// make a LED OFF if pin number is set
void teleInfo::led_off( uint8_t pin )
{
    if( pin > 0 ){
        digitalWrite( pin, LOW );
    }
}

// Setters from a string
// The string may have been received as a mySensors payload message
// The string is of the form: <var>=<value>, with:
// - <var>="hTC" for honorThreadCb
// - value="1|0" for true or false
//
void teleInfo::set_from_str( const char *payload )
{
    char *p = strstr( payload, ";" );
    if( p ){
        if( !strncmp( payload, "hTC", p-payload )){
            bool ok = false;
            bool honor;
            if( !strcmp( p+1, "0" )){
                ok = true;
                honor = false;
            } else if( !strcmp( p+1, "1" )){
                ok = true;
                honor = false;
            }
            if( ok ){
                this->set_honor_thread_cb( honor );
            }
        }
    }
}

// Set the HC/HP LEDs depending of the current value
//
void teleInfo::set_hchp_state( const char *value )
{
    if( !strcmp( value, "HC.." )){
        this->led_off( this->hpPin );
        this->led_on( this->hcPin );

    } else if( !strcmp( value, "HP.." )){
        this->led_off( this->hcPin );
        this->led_on( this->hpPin );
    }
}

// get/set the honorThreadCb flag
//
bool teleInfo::get_honor_thread_cb( void )
{
    return( this->honorThreadCb );
}

void teleInfo::set_honor_thread_cb( bool honor )
{
    this->honorThreadCb = honor;
}

// Initialize the thread callback
//
// NOTE:
// Raw transmission rate is about one frames group every 0.55sec
// (135 chars in 11 lines). From mySensors point of view, this
// corresponds to about 20 messages of 19 chars each per second.
//
void teleInfo::set_thread_cb( threadCb cb )
{
    this->cb = cb;
}

// Try to activate the thread callback
//
void teleInfo::try_thread_cb( const char *label, const char *value )
{
    if( this->honorThreadCb && this->cb ){
        this->countThreadCb += 1;
        if( this->countThreadCb == this->rateThreadCb ){
            this->countThreadCb = 0;
            this->cb( label, value );
        }
    }
}

// save TI values in the (good) struct member
bool teleInfo::save( char *label, const char *searchLabel, char *value, uint32_t &dst )
{
	  if( strcmp( label, searchLabel ) != 0 )
		    return false;

  	dst = atol( value );
  	return true;
}

bool teleInfo::save( char *label, const char *searchLabel, char *value, uint8_t &dst ) 
{
  	if( strcmp( label, searchLabel ) != 0 )
	    	return false;

  	dst = atoi( value );
	  return true;
}

bool teleInfo::save( char *label, const char *searchLabel, char &value, char &dst ) 
{
  	if( strcmp( label, searchLabel ) != 0 )
		    return false;

  	dst = value;
  	return true;
}

bool teleInfo::save( char *label, const char *searchLabel, char *value, char *dst ) 
{
  	if( strcmp( label, searchLabel ) != 0 )
    		return false;

  	memset( dst, 0, TI_BUFSIZE );
	  strcpy( dst, value );
  	return true;
}

/*
 * Read the next character available on the input serial line
 * Returns: the read character, or 0 on timeout (no available data)
 */
char teleInfo::read_next( void )
{
    char ch;
    unsigned long timeout_ms = millis() + TI_READ_TIMEOUT;

    while( !tiSerial->available()){
        if( millis() > timeout_ms ){
#ifdef TI_DEBUG
            Serial.println( F( "[teleInfo::read_next] timeout on serial input" ));
#endif
            return( 0 );
        }
        delay( 5 );
    }
    ch = tiSerial->read() & 0x7F;

#ifdef TI_DEBUG_READ_NEXT
    Serial.print( ch );
    //Serial.print( " " ); Serial.println( ch, HEX );
#endif

    return( ch );
}

/*
 * Return true if ok, false if timed out
 */
bool teleInfo::read_until_end_of_line()
{
    char ch;

    while( true ){
        ch = read_next();
        if( ch == 0 ){
            return( false );
        }
        if( ch == '\n' ){
            return( true );
        }
    }
}

/*
 * Read a label up to the trailing space
 * Compute the checksum accordingly
 *  -> the returned buffer does *not* contain the trailing space
 *  -> the returned checksum does take into account this same trailing space
 *  
 * Returns: false on error (no data on serial line, overflow)
 */
bool teleInfo::read_label( char *buffer, uint8_t maxsize, uint8_t *checksum )
{
    char ch;
    uint8_t i;

    ch = 0;
    *checksum = 0;
    memset( buffer, '\0', maxsize );

    for( i=0 ; i<maxsize-1 && ch != ' ' ; ++i ){
        ch = read_next();
        if( ch == 0 ){
            return( false );
        }
        *checksum += ( int ) ch;
        if( ch != ' ' ){
            buffer[i] = ch;
        }
    }

    // if overflow, exit with current result
    if( i == maxsize-1 ){
#ifdef TI_DEBUG
        Serial.print( F( "[teleInfo::read_label] overflow i=" ));
        Serial.print( i );
        Serial.print( F( ", buffer=" ));
        Serial.println( buffer );
#endif
        return( false );
    }

#ifdef TI_DEBUG_READ_LABEL
    Serial.print( F( "[teleInfo::read_label] returns '" )); Serial.print( buffer ); Serial.println( "'" );
#endif

    return( true );
}

/*
 * Read Teleinfo informations from input pin
 * We are reading here all the trames between ADCO (first trame) and MOTDETAT (last trame)
 * and fill the teleInfo_t structure with the gathered datas.
 * 
 * Returns: true if datas have been successfully read, false else
 * (and teleInfo is to be considered as unavailable)
 */
bool teleInfo::get( teleInfo_t *res ) 
{
    char ch, label[TI_BUFSIZE], value[TI_BUFSIZE];
    uint8_t i, checksum;
    unsigned long label_ms;

#ifdef TI_DEBUG
    Serial.println( F( "[teleInfo::get]" ));
#endif

    memset( res, '\0', sizeof( teleInfo_t ));

    /* First go the end of the current groupe of trames (MOTDETAT)
     *  Each trame is 'label+space+value+space+checksum+\n'
     *  Checksum is computed on 'label+space+value'
     */
    while( true ){
        // read label
        if( !read_label( label, TI_BUFSIZE, &checksum )){
            //Serial.println( "read_label returned False" );
            return( false );
        }
        // go until next end of line
        if( !read_until_end_of_line()){
            //Serial.println( "read_until_end_of_line returned False" );
            return( false );
        }
        // exit the loop when end of group is found
        if( !strcmp( label, TI_MOTDETAT )){
            //Serial.println( "found TI_MOTDETAT" );
            break;
        }
    }

    /* Then read the next group of trames
     *  until next 'MOTDETAT'
     */
    while( true ){
        // commencer par detecter le label (search for ' ')
        label_ms = millis();
        if( !read_label( label, TI_BUFSIZE, &checksum )){
            return( false );
        }
#ifdef TI_DEBUG_GET_LABEL
        Serial.print( F( "[teleInfo::get] label='" ));
        Serial.print( label );
        Serial.println( "'" );
#endif
        // light on the LED on start of the group of trames
        if( !strcmp( label, TI_ADCO )){
            this->led_on( this->ledPin );
        }

        // read the value right after the space, until next space
        // nb: the checksum does not considers this last trailing space
        memset( value, '\0', sizeof( value ));
        for( i=0 ; i<TI_BUFSIZE ; ++i ){
            ch = read_next();
            if( ch == 0 ){
                return( false );
            } else if( ch == ' ' ){
                break;
            } else {
                checksum += ( int ) ch;
                value[i] = ch;
            }
        }
#ifdef TI_DEBUG_GET_LABEL
        Serial.print( F( "[teleInfo::get] value='" ));
        Serial.print( value );
        Serial.println( "'" );
#endif
        // if overflow, exit with current result
        if( i == TI_BUFSIZE ){
#ifdef TI_DEBUG
            Serial.print( F( "[teleInfo::get] value overflow i=" ));
            Serial.print( i );
            Serial.print( F( ", value=" ));
            Serial.println( value );
#endif
            break;
        }
        // read and check the checksum
        ch = read_next();
        if( ch == 0 ){
            return( false );
        }
        checksum = ( checksum & 0x3F ) + 0x20;

        // last eat control characters until end-of-line
        if( !read_until_end_of_line()){
            return( false );
        }

        this->haveTeleinfo = true;

#ifdef TI_DEBUG
        Serial.print( label_ms );
        Serial.print( ": " );
        Serial.print( label );
        Serial.print( " " );
        Serial.print( value );
        Serial.print( " " );
        Serial.print( ch );
        Serial.print( " (0x" );
        Serial.print( ch, HEX );
        Serial.print( ")" );
        if( ch != checksum ){
            Serial.print( F( " - Computed checksum was 0x" ));
            Serial.print( checksum, HEX );
        }
        Serial.println( "" );
#endif

        // call the thread callback if it has been set
        try_thread_cb( label, value );
        
        // if label is MOTDETAT then we have read all trames for this iteration
        if( !strcmp( label, TI_MOTDETAT )){
            this->led_off( this->ledPin );
            break;
        }

        // set the HC/HP LEDs depending of the current state
        if( !strcmp( label, TI_PTEC )){
            this->set_hchp_state( value );
        }

        // sauvegarde la value dans le bon element de la structure
        if( !save( label, TI_ADCO, value, res->ADCO ) &&
            !save( label, TI_OPTARIF, value, res->OPTARIF ) &&
            !save( label, TI_ISOUSC, value, res->ISOUSC ) &&
            !save( label, TI_BASE, value, res->BASE ) &&
            !save( label, TI_HCHC, value, res->HC_HC ) &&
            !save( label, TI_HCHP, value, res->HC_HP ) &&
            !save( label, TI_EJPHN, value, res->EJP_HN ) &&
            !save( label, TI_EJPHPM, value, res->EJP_HPM ) &&
            !save( label, TI_BBRHCJB, value, res->BBR_HC_JB ) &&
            !save( label, TI_BBRHPJB, value, res->BBR_HP_JB ) &&
            !save( label, TI_BBRHCJW, value, res->BBR_HC_JW ) &&
            !save( label, TI_BBRHPJW, value, res->BBR_HP_JW ) &&
            !save( label, TI_BBRHCJR, value, res->BBR_HC_JR ) &&
            !save( label, TI_BBRHPJR, value, res->BBR_HP_JR ) &&
            !save( label, TI_PEJP, value, res->PEJP ) &&
            !save( label, TI_PTEC, value, res->PTEC ) &&
            !save( label, TI_DEMAIN, value, res->DEMAIN ) &&
            !save( label, TI_IINST, value, res->IINST ) &&
            !save( label, TI_ADPS, value, res->ADPS ) &&
            !save( label, TI_IMAX, value, res->IMAX ) &&
            !save( label, TI_PAPP, value, res->PAPP ) &&
            !save( label, TI_HHPHC, value[0], res->HHPHC )){
                // what do do here ?
#ifdef TI_DEBUG
                Serial.print( F( "Unknown label=" ));
                Serial.println( label );
#endif
                //break;
        }
    }

		return( this->haveTeleinfo );
}

