#ifndef __TELEINFO_H__
#define __TELEINFO_H__

#include <Arduino.h>
#include "pwiSoftwareSerial.h"

#define TI_BUFSIZE 13 // 12 + null-terminator

/*
 * Compteur « Bleu » électronique monophasé multitarif (CBEMM - évolution ICC).
 * 
 * Le tableau ci-dessous fournit, pour chaque groupe d'information possible, sa désignation, son étiquette
 * d'identification, ainsi que le nombre de caractères nécessaires pour coder la donnée correspondante et
 * l’unité utilisée pour la donnée.
 * 
 * Source: doc ERDF ERDF-NOI-CPT_02E.pdf - Sept. 2014
 * 
 * Etiquette  Contenu                                 Taille datas  Unité
 * ---------  --------------------------------------  ------------  -----
 * ADCO       Adresse du concentrateur de téléreport      X(12)
 *            (identifiant du compteur)
 * OPTARIF    Option tarifaire choisie                     X(4)
 *            (type d’abonnement)
 *            Valeurs possibles:
 *              BASE: option Base
 *              HC..: option Heure Creuse
 *              EJP.: option EJP
 *              BBRx: option Tempo
 * ISOUSC     Intensité souscrite                          9(2)       A
 * BASE       Index option Base                            9(9)       Wh
 *            Index option Heures Creuses
 * HCHC         Heures Creuses                             9(9)       Wh
 * HCHP         Heures Pleines                             9(9)       Wh
 *            Index option EJP
 * EJPHN        Heures Normales                            9(9)       Wh
 * EJPHPM       Heures de Pointe Mobile                    9(9)       Wh
 *            Index option Tempo
 * BBRHCJB      Heures Creuses Jours Bleus                 9(9)       Wh
 * BBRHPJB      Heures Pleines Jours Bleus                 9(9)       Wh
 * BBRHCJW      Heures Creuses Jours Blancs                9(9)       Wh
 * BBRHPJW      Heures Pleines Jours Blancs                9(9)       Wh
 * BBRHCJR      Heures Creuses Jours Rouges                9(9)       Wh
 * BBRHPJR      Heures Pleines Jours Rouges                9(9)       Wh
 * PEJP       Préavis Début EJP (30 min)                   9(2)       min
 *            N'est émis que par un compteur programmé
 *            en option EJP. Ce groupe apparaît pendant
 *            toute la période de préavis et pendant la
 *            période de pointe mobile.
 *            Valeur fixe = 30.
 * PTEC       Période Tarifaire en cours                   X(4)
 *            Valeurs possibles:
 *              TH..: Toutes les Heures.
 *              HC..: Heures Creuses.
 *              HP..: Heures Pleines.
 *              HN..: Heures Normales.
 *              PM..: Heures de Pointe Mobile.
 *              HCJB: Heures Creuses Jours Bleus.
 *              HCJW: Heures Creuses Jours Blancs (White).
 *              HCJR: Heures Creuses Jours Rouges.
 *              HPJB: Heures Pleines Jours Bleus.
 *              HPJW: Heures Pleines Jours Blancs (White).
 *              HPJR: Heures Pleines Jours Rouges
 * DEMAIN     Couleur du lendemain                         X(4)
 *            N'est émise que par un compteur programmé
 *            en option Tempo.
 *            Valeurs possibles:
 *              ----: couleur du lendemain non connue
 *              BLEU: le lendemain est un jour bleu 
 *              BLAN: le lendemain est un jour blanc
 *              ROUG: le lendemain est un jour rouge
 * IINST      Intensité Instantanée                        9(3)       A
 * ADPS       Avertissement de Dépassement De Puissance    9(3)       A
 *            Souscrite.
 *            N'est émis que lorsque la puissance
 *            consommée dépasse la puissance souscrite.
 * IMAX       Intensité maximale appelée                   9(3)       A
 * PAPP       Puissance apparente                          9(5)       VA
 *            Arrondie à la dizaine de VA la plus proche.
 * HHPHC      Horaire Heures Pleines Heures Creuses        X(1)
 *            L'horaire heures pleines/heures creuses
 *            est codé par le caractère alphanumérique
 *            A, C, D, E ou Y correspondant à la
 *            programmation du compteur.
 * MOTDETAT   Mot d'état du compteur                       X(6)
 *            Usage réservé au distributeur.
 *            
 * Les trames sont émises les unes après les autres de façon continue,
 * en respectant l'ordre de ce tableau.
 * Chaque trame (courte ou longue) est constituée de l'ensemble des
 * groupes d'information, définis dans le tableau précédent, et utiles
 * ou significatifs au moment de son émission.
 * Parmi l'ensemble des groupes d'information relatifs aux index de
 * consommation, seuls ceux qui correspondent à l'option tarifaire
 * choisie sont émis.
 * Le groupe d'information de préavis EJP (PEJP) est émis uniquement
 * pendant les périodes de préavis et pointe mobile, à condition que
 * l'option tarifaire EJP soit effectivement programmée sur le compteur.
 * Le groupe d'information de couleur du lendemain (DEMAIN) est émis
 * uniquement par un compteur programmé en option TEMPO.
 * 
 * Shamelessly copied from https://github.com/jaysee/teleInfo
 */

typedef struct {
  	char     ADCO[TI_BUFSIZE];      // Adresse du concentrateur de téléreport (identifiant du compteur)
    uint8_t  last_adco;             //    last day number the information has been sent
	  char     OPTARIF[TI_BUFSIZE];   // Option tarifaire choisie (type d’abonnement)
    uint8_t  last_optarif;          //    last day number the information has been sent
  	uint8_t  ISOUSC;                // Intensité souscrite 2chars (A)
    uint8_t  last_isousc;           //    last day number the information has been sent
	  char     PTEC[TI_BUFSIZE];      // Période tarifaire en cours
  	uint8_t  IINST;                 // Intensité instantanée (A)
	  uint8_t  ADPS;                  // Avertissement de dépassement de puissance souscrite (A)
  	uint8_t  IMAX;                  // Intensité maximale (A)
    uint8_t  last_imax;             //    last day number the information has been sent
	  uint32_t PAPP;                  // Puissance apparente (VA)
  	uint32_t BASE;                  // Index si option = base (Wh)
	  uint32_t HC_HC;                 // Index heures creuses si option = heures creuses (Wh)
  	uint32_t HC_HP;                 // Index heures pleines si option = heures creuses (Wh)
	  uint32_t EJP_HN;                // Index heures normales si option = EJP (Wh)
  	uint32_t EJP_HPM;               // Index heures de pointe mobile si option = EJP (Wh)
	  uint8_t  PEJP;                  // Préavis EJP si option = EJP 30mn avant période EJP (mn)
    uint32_t BBR_HC_JB;             // Index heures creuses jours bleus si option = tempo (Wh)
   	uint32_t BBR_HP_JB;             // Index heures pleines jours bleus si option = tempo (Wh)
	  uint32_t BBR_HC_JW;             // Index heures creuses jours blancs si option = tempo (Wh)
  	uint32_t BBR_HP_JW;             // Index heures pleines jours blancs si option = tempo (Wh)
	  uint32_t BBR_HC_JR;             // Index heures creuses jours rouges si option = tempo (Wh)
  	uint32_t BBR_HP_JR;             // Index heures pleines jours rouges si option = tempo (Wh)
    char     DEMAIN[TI_BUFSIZE];    // Couleur du lendemain si option = tempo
	  char     HHPHC;                 // Groupe horaire si option = heures creuses ou tempo
}
    teleInfo_t;

/*
 * Associe une valeur numérique aux options tarifaires
 */
#define TI_TARIF_BASE          1
#define TI_TARIF_HCHP          2
#define TI_TARIF_EJP           4
#define TI_TARIF_TEMPO         8

/*
 * Define static strings
 */
static const char * const TI_ADCO        = "ADCO";
static const char * const TI_OPTARIF     = "OPTARIF";
static const char * const TI_ISOUSC      = "ISOUSC";
static const char * const TI_BASE        = "BASE";
static const char * const TI_HCHC        = "HCHC";
static const char * const TI_HCHP        = "HCHP";
static const char * const TI_EJPHN       = "EJPHN";
static const char * const TI_EJPHPM      = "EJPHPM";
static const char * const TI_BBRHCJB     = "BBRHCJB";
static const char * const TI_BBRHPJB     = "BBRHPJB";
static const char * const TI_BBRHCJW     = "BBRHCJW";
static const char * const TI_BBRHPJW     = "BBRHPJW";
static const char * const TI_BBRHCJR     = "BBRHCJR";
static const char * const TI_BBRHPJR     = "BBRHPJR";
static const char * const TI_PEJP        = "PEJP";
static const char * const TI_PTEC        = "PTEC";
static const char * const TI_DEMAIN      = "DEMAIN";
static const char * const TI_IINST       = "IINST";
static const char * const TI_ADPS        = "ADPS";
static const char * const TI_IMAX        = "IMAX";
static const char * const TI_PAPP        = "PAPP";
static const char * const TI_HHPHC       = "HHPHC";
static const char * const TI_MOTDETAT    = "MOTDETAT";
static const char * const TI_OPTARIF_HC  = "HC..";
static const char * const TI_OPTARIF_EJP = "EJP.";
static const char * const TI_OPTARIF_BBR = "BBR";

const char *tarifToStr( uint8_t tarif );

/*
 * Let the caller receive a copy of the thread
 * If this callback is set, then it will be called on each received line
 * with: cb( label, value )
 */
typedef void ( *threadCb )( const char *, const char * );

class teleInfo {
  public:
		teleInfo( uint8_t rxPin, uint8_t ledPin, uint8_t hcPin, uint8_t hpPin );
		~teleInfo();
    bool get( teleInfo_t *data );
		void set_thread_cb( threadCb cb );

	private:
		pwiSoftwareSerial* tiSerial;
    uint8_t            ledPin;
    uint8_t            hcPin;
    uint8_t            hpPin;
    bool               haveTeleinfo;
    bool               honorThreadCb;
    threadCb           cb;

    void init_led( uint8_t *dest, uint8_t pin );
    void led_on( uint8_t pin );
    void led_off( uint8_t pin );
    void set_hchp_state( const char *value );
    char read_next( void );
    bool read_until_end_of_line( void );
    bool read_label( char *buffer, uint8_t maxsize, uint8_t *chksum );

		bool save( char *label, const char *searchLabel, char *value, uint32_t &dst );
		bool save( char *label, const char *searchLabel, char *value, uint8_t  &dst );
		bool save( char *label, const char *searchLabel, char &value, char     &dst );
		bool save( char *label, const char *searchLabel, char *value, char     *dst );
};

#endif // __TELEINFO_H__

