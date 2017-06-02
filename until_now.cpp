#include "until_now.h"

#define UNTIL_NOW_MAX_ULONG 4294967295 /* overflow every 49 days */

/**
 * untilNow:
 * @now_ms: the current time (ms).
 * @start_ms: the start of the delay (ms).
 *
 * Compute the delay (ms) from the specified @start_ms and now,
 * taking into account the case where the unsigned long has reached
 * its max value.
 */
unsigned long untilNow( unsigned long now_ms, unsigned long start_ms )
{
    unsigned long delay_ms;
    if( now_ms >= start_ms ){
        delay_ms = now_ms - start_ms;
    } else {
        delay_ms = UNTIL_NOW_MAX_ULONG - start_ms;
        delay_ms += now_ms;
    }
    return( delay_ms );
}

