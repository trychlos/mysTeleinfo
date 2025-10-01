#ifndef __CHILDIDS_H__
#define __CHILDIDS_H__

/* **********************************************************************************************************
 * pwi 2025-10- 1 v1 creation
 */

enum {
    CHILD_MAIN                  = 1,
    CHILD_MAIN_LOG              = CHILD_MAIN+0,
    CHILD_MAIN_ACTION_RESET     = CHILD_MAIN+1,
    CHILD_MAIN_ACTION_DUMP      = CHILD_MAIN+2,
    CHILD_MAIN_PARM_DUMP_PERIOD = CHILD_MAIN+6,
    CHILD_MAIN_PARM_MIN_PERIOD  = CHILD_MAIN+7,
    CHILD_MAIN_PARM_MAX_PERIOD  = CHILD_MAIN+8,
    CHILD_TI                    = 100,
    CHILD_ID_ADSC               = CHILD_TI+0,
    CHILD_ID_VTIC               = CHILD_TI+1,
    CHILD_ID_DATE               = CHILD_TI+2,
    CHILD_ID_NGTF               = CHILD_TI+3,
    CHILD_ID_LTARF              = CHILD_TI+4,
    CHILD_ID_EASF01             = CHILD_TI+6,
    CHILD_ID_EASF02             = CHILD_TI+7,
    CHILD_ID_IRMS1              = CHILD_TI+25,
    CHILD_ID_URMS1              = CHILD_TI+28,
    CHILD_ID_PREF               = CHILD_TI+31,
    CHILD_ID_SINSTS             = CHILD_TI+33,
    CHILD_ID_SMAXSN             = CHILD_TI+37,
    CHILD_ID_SMAXSN_1           = CHILD_TI+41,
    CHILD_ID_CCASN              = CHILD_TI+48,
    CHILD_ID_CCASN_1            = CHILD_TI+49,
    CHILD_ID_PRM                = CHILD_TI+64,
    CHILD_ID_NTARF              = CHILD_TI+66,
    CHILD_ID_HCHP               = CHILD_TI-1,
};

#endif // __CHILDIDS_H__

