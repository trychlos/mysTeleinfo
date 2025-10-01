/* **********************************************************************************************************
 * Objet decodeur de teleinformation client (TIC)
 * format Linky "historique" ou anciens compteurs
 * electroniques.
 * V06 : MicroQuettas mars 2018
 * Reference : ERDF-NOI-CPT_54E V1
 *
 * Linky TIC historique
 * ====================
 *
 * Trame historique :
 * - délimiteurs de trame :     <STX> trame <ETX>
 *   <STX> = 0x02
 *   <ETX> = 0x03
 *
 * Groupes dans une trame : <LF>lmnopqrs<SP>123456789012<SP>C<CR>
 *   <LR> = 0x0A
 *   lmnopqrs = label 8 char max
 *   <SP> = 0x20 ou 0x09 (<HT>)
 *   123456789012 = data 12 char max
 *   C = checksum 1 char
 *   <CR> = 0x0d
 *      Longueur max : label + data = 7 + 9 = 16
 *
 * _FR : flag register
 *
 *   |  7  |  6  |   5  |   4  |   3  |  2  |   1  |   0  |
 *   |     |     | _Dec | _GId | _Cks |     | _RxB | _Rec |
 *
 *    _Rec : receiving
 *    _RxB : receive in buffer B, decode in buffer A
 *    _GId : Group identification
 *    _Dec : decode data
 *
 * _DNFR : data available flags
 *
 *   |  7  |  6  |  5  |  4  |  3  |   2   |   1    |   0   |
 *   |     |     |     |     |     | _papp | _iinst | _base |
 *
 * Exemple of group :
 *      <LF>lmnopqr<SP>123456789<SP>C<CR>
 *          0123456  7 890123456  7 8  9
 *    Cks:  xxxxxxx  x xxxxxxxxx    ^
 *                                  |  iCks
 *   Minimum length group  :
 *          <LF>lmno<SP>12<SP>C<CR>
 *              0123  4 56  7 8  9
 *    Cks:      xxxx  x xx    ^
 *                            |  iCks
 * The storing stops at CRC (included), ie a max of 19 chars
 * 
 **********************************************************************************************************
 * pwi 2019- 9-22 adaptation to Enedis-NOI-CPT_54E v3 TIC standard
 * 
 * TIC standard
 * ============
 * Each trame embeds several information groups. We measure one trame per second.
 * An information group:
 * - starts with a 'start_of_information_group' character,
 *   aka LF, LineFeed, 0x0A, \n
 * - ends with a 'end_of_information_group' character,
 *   aka CR, CarriageReturn, 0x0D, \r
 * - fields are separated by TAB
 * 
 * Etiquette  Label                                          Horodatage  Data  Unité  HC/HP Commentaire 3.0-2019
 * ADSC       Adresse secondaire du compteur                 0           12           Oui               Oui
 * VTIC       Version de la TIC                              0           2            Oui
 * DATE       Date et heure courante SAAMMJJHHMISS           13          0            Oui   1 trame/sec Oui
 * NGTF       Nom tarif fournisseur                          0           16           Oui               Oui
 * LTARF      Tarif en cours                                 0           16           Oui               Oui
 * EAST       Energie totale soutirée                        0           9     Wh     Oui
 * EASF01     Energie active soutirée fournisseur index 1    0           9     Wh     Oui               Oui
 * EASF02     Energie active soutirée fournisseur index 2    0           9     Wh     Oui               Oui
 * EASD01     Energie active soutirée distribut. index 1     0           9     Wh     Oui   =EASF01
 * EASF01     Energie active soutirée distribut. index 1     0           9     Wh     Oui   =EASF02
 * IRMS1      Courant efficace, phase 1                      0           3     A      Oui               Oui
 * URMS1      Tension efficace, phase 1                      0           3     V      Oui               Oui
 * PREF       Puissance apparente de référence               0           2     kVA    Oui               Oui
 * PCOUP      Puissance apparente de coupure                 0           2     kVA    Oui
 * SINSTS     Puissance app. instantanée soutirée            0           5     VA     Oui               Oui
 * SMAXSN     Puissance app. max. soutirée n                 13          5     VA     Oui               Oui
 * SMAXSN-1   Puissance app. max. soutirée n-1               13          5     VA     Oui               Oui
 * CCASN      Point n de la courbe de charge active soutirée 13          5     W      Oui               Oui
 * CCASN-1    Point n-1 de la courbe de charge active sout.  13          5     W      Oui               Oui
 * UMOY1      Tension moyenne phase 1                        13          3     V      Oui
 * STGE       Registre de statuts                            0           8            Oui
 * MSG1       Message                                        0           32           Oui
 * MSG2       Message court                                  0           16           Oui
 * PRM        "le" PRM, aka point de livraison (PDL)         0           14           Oui               Oui
 * RELAIS     Relais                                         0           3            Oui
 * NTARF      Numéro de l'index tarifaire en cours           0           2            Oui               Oui
 * NJOURF     Numéro du jour en cours calendrier fournisseur 0           2            Oui
 * NJOURF+1   Numéro de prochain jour calendrier fournisseur 0           2            Oui
 * PJOURF+1   Profil du prochain jour calendrier fournisseur 0           98           Oui
 * 
 * Max considered size = 30 oct.
 *
 **********************************************************************/
#ifndef __LINKY_H__
#define __LINKY_H__

#include <SoftwareSerial.h>
#include <pwiTimer.h>

//#define LINKY_BUFSIZE       32    /* max size of the received, not ignored, information groups */
#define LINKY_BUFSIZE       64    /* max size of the received, not ignored, information groups */
#define LINKY_ADSC_SIZE     12
#define LINKY_VTIC_SIZE      2
#define LINKY_DATE_SIZE     13
#define LINKY_NGTF_SIZE     16
#define LINKY_LTARF_SIZE    16
#define LINKY_PRM_SIZE      14

typedef struct {
    char        adsc[1+LINKY_ADSC_SIZE];
    char        vtic[1+LINKY_VTIC_SIZE];
    char        date[1+LINKY_DATE_SIZE];
    char        ngtf[1+LINKY_NGTF_SIZE];
    char        ltarf[1+LINKY_LTARF_SIZE];
    uint32_t    easf01;
    uint32_t    easf02;
    uint8_t     irms1;
    uint16_t    urms1;
    uint8_t     pref;
    uint16_t    sinsts;
    uint16_t    smaxsn;
    uint16_t    smaxsnm1;
    uint16_t    ccasn;
    uint16_t    ccasnm1;
    char        prm[1+LINKY_PRM_SIZE];
    uint8_t     ntarf;
    bool        hchp;
}
  tic_t;

/* bit position of the corresponding data in the _DNFR data new flag register
 *  each information group has here its own bit position which records the
 *  presence of a new value to be sent to the controller
 */
typedef enum {
    let_adsc = 0,
    let_vtic,
    let_date,
    let_ngtf,
    let_ltarf,
    let_easf01,
    let_easf02,
    let_irms1,
    let_urms1,
    let_pref,
    let_sinsts,
    let_smaxsn,
    let_smaxsnm1,
    let_ccasn,
    let_ccasnm1,
    let_prm,
    let_ntarf,
    let_hchp
}
  linky_etiq_t;

/* bit position of the next step in the flag register
 *  this determines the next step to be done in the receiving loop
 */
typedef enum {
    lst_Rec = 0,  // receive
    lst_RxB,      // receive in buffer B (rather than in buffer A)
    lst_Dec       // decode
}
  linky_step_t;

class Linky
{
    public:
                                  Linky( uint8_t id, uint8_t rxPin, uint8_t ledPin, uint8_t hcPin, uint8_t hpPin );
        virtual void              ledOff( uint8_t pin );
        virtual void              ledOn( uint8_t pin );
        virtual void              loop();
        virtual void              present();
        virtual void              send( bool all=false );
        virtual void              setup( uint32_t min_period_ms, uint32_t max_period_ms );

    private:
        /* construction data
         *  Please note that the SoftwareSerial object requires to be constructed and initialized
         *  at the very same time that this object instance itself (see Linky() constructor).
         */
                SoftwareSerial    linkySerial;
                uint8_t           id;
                uint8_t           rxPin;
                uint8_t           ledPin;
                uint8_t           hcPin;
                uint8_t           hpPin;

        /* runtime data
         */
                char              _BfA[LINKY_BUFSIZE];      /* Buffer A */
                char              _BfB[LINKY_BUFSIZE];      /* Buffer B */
                uint8_t           _FR;                      /* Flag register */
                uint32_t          _DNFR;                    /* Data new flag register */
                char             *_pRec;                    /* Reception pointer in the buffer */
                char             *_pDec;                    /* Decode pointer in the buffer */
                char             *_startLabel;              /* the start of the label */
                uint8_t           _iRec;                    /*  Received char index */
                uint8_t           _iCks;                    /* Index of Cks in the received message */
                uint8_t           _GId;                     /* Group identification */

                tic_t             tic;
                uint32_t          stx_ms;

                pwiTimer          min_period;
                pwiTimer          max_period;
                pwiTimer          timeout_timer;

                // because the LED is visible on the front panel, we choose to manage it with a hardware timer
                pwiTimer          led_status_timer;         /* 3 sec if OK, 1 sec else */
                pwiTimer          led_on_timer;             /* 0.1 sec */

        /* private methods
         */
                void              init();
                void              init_led( uint8_t *dest, uint8_t pin );
                bool              checkHorodate( const char *p );
                bool              decData( char *dest, linky_etiq_t etiq );
                bool              decData( uint8_t *dest, linky_etiq_t etiq );
                bool              decData( uint16_t *dest, linky_etiq_t etiq );
                bool              decData( uint32_t *dest, linky_etiq_t etiq );
                //bool              decData( horodate_t *dest, linky_etiq_t etiq );
                bool              ig_checksum( void );
                void              ig_decode( void );
                void              ig_receive( void );
                void              logIgnored();
                void              sendLog( char *msg );
                void              trameLedSet( uint32_t period_ms );

        /* static methods
         */
        static  void              MaxPeriodCb( void *user_data );
        static  void              MinPeriodCb( void *user_data );
        static  void              TrameLedOnCb( void *data );
        static  void              TrameLedStatusCb( void *data );
        static  void              TrameTimeoutCb( void *data );
};
  
#endif // __LINKY_H__

