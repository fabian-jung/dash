/**
 * @file    dash.c
 * @author  Joachim Protze
 *          joachim.protze@zih.tu-dresden.de
 * @author  Vince Weaver
 *          vweaver1@eecs.utk.edu
 *
 * @ingroup papi_components
 *
 * @brief
 *	This is an dash component, it demos the component interface
 *  and implements three dash counters.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

/* Headers required by PAPI */
#include "papi.h"
#include "papi_internal.h"
#include "papi_vector.h"
#include "papi_memory.h"    /* defines papi_malloc(), etc. */

/** This driver supports three counters counting at once      */
/*  This is artificially low to allow testing of multiplexing */
#define DASH_MAX_SIMULTANEOUS_COUNTERS 6
#define DASH_MAX_MULTIPLEX_COUNTERS 6

/* Declare our vector in advance */
/* This allows us to modify the component info */
papi_vector_t _dash_vector;

/** Structure that stores private information for each event */
typedef struct dash_register
{
   unsigned int selector;
		           /**< Signifies which counter slot is being used */
			   /**< Indexed from 1 as 0 has a special meaning  */
} dash_register_t;

/** This structure is used to build the table of events  */
/*   The contents of this structure will vary based on   */
/*   your component, however having name and description */
/*   fields are probably useful.                         */
typedef struct dash_native_event_entry
{
	dash_register_t resources;	    /**< Per counter resources       */
	char name[PAPI_MAX_STR_LEN];	    /**< Name of the counter         */
	char description[PAPI_MAX_STR_LEN]; /**< Description of the counter  */
	int writable;			    /**< Whether counter is writable */
	/* any other counter parameters go here */
} dash_native_event_entry_t;

/** This structure is used when doing register allocation
    it possibly is not necessary when there are no
    register constraints */
typedef struct dash_reg_alloc
{
	dash_register_t ra_bits;
} dash_reg_alloc_t;

/** Holds control flags.
 *    There's one of these per event-set.
 *    Use this to hold data specific to the EventSet, either hardware
 *      counter settings or things like counter start values
 */
typedef struct dash_control_state
{
  int num_events;
  int domain;
  int multiplexed;
  int overflow;
  int inherit;
  int which_counter[DASH_MAX_SIMULTANEOUS_COUNTERS];
  long long counter[DASH_MAX_MULTIPLEX_COUNTERS];   /**< Copy of counts, holds results when stopped */
} dash_control_state_t;


/** This table contains the native events */
static dash_native_event_entry_t *dash_native_table;

/** number of events in the table*/
static int num_events = 0;


/*************************************************************************/
/* Below is the actual "hardware implementation" of our dash counters */
/*************************************************************************/

#define DASH_GLOB_REF_LVALUE_ACCESS             0
#define DASH_GLOB_REF_RVALUE_ACCESS             1
#define DASH_GLOB_PTR_ACCESS                    2
#define DASH_ONESIDED_PUT                       3
#define DASH_ONESIDED_GET                       4
#define DASH_LOCAL_PTR_ACCESS                   5

#define DASH_TOTAL_EVENTS         6

/** Holds per-thread information */
typedef struct dash_context
{
     long long values[DASH_TOTAL_EVENTS];
} dash_context_t;

/** Code that resets the hardware.  */
static void
dash_hardware_reset( dash_context_t *ctx )
{
   /* reset per-thread count */
	for(int i = 0; i < num_events; ++i) {
		ctx->values[i] = 0;
	}
}

/** Code that reads event values.                         */
/*   You might replace this with code that accesses       */
/*   hardware or reads values from the operatings system. */
static long long
dash_hardware_read( int which_one, dash_context_t *ctx )
{
	return ctx->values[which_one];
}

/** Code that writes event values.                        */
static int
dash_hardware_write( int which_one,
			dash_context_t *ctx,
			long long value)
{
	if (which_one < 0 || which_one >= num_events) {
		return PAPI_EINVAL;
	}
	ctx->values[which_one] = value;
	return PAPI_OK;
}

static int
detect_dash(void) {

   return PAPI_OK;
}

/********************************************************************/
/* Below are the functions required by the PAPI component interface */
/********************************************************************/


/** Initialize hardware counters, setup the function vector table
 * and get hardware information, this routine is called when the
 * PAPI process is initialized (IE PAPI_library_init)
 */
static int
_dash_init_component( int cidx )
{

	SUBDBG( "_dash_init_component..." );


        /* First, detect that our hardware is available */
        if (detect_dash()!=PAPI_OK) {
	   return PAPI_ECMP;
	}

	/* we know in advance how many events we want                       */
	/* for actual hardware this might have to be determined dynamically */
	num_events = DASH_TOTAL_EVENTS;

	/* Allocate memory for the our native event table */
	dash_native_table =
		( dash_native_event_entry_t * )
		papi_calloc( num_events, sizeof(dash_native_event_entry_t) );
	if ( dash_native_table == NULL ) {
		PAPIERROR( "malloc():Could not get memory for events table" );
		return PAPI_ENOMEM;
	}

	/* fill in the event table parameters */
	/* for complicated components this will be done dynamically */
	/* or by using an external library                          */

	strcpy( dash_native_table[DASH_GLOB_REF_LVALUE_ACCESS].name, "DASH_GLOB_REF_LVALUE_ACCESS" );
	strcpy( dash_native_table[DASH_GLOB_REF_LVALUE_ACCESS].description,
			"This is an dash counter, that counts L-Value accesses to global references" );
	dash_native_table[DASH_GLOB_REF_LVALUE_ACCESS].writable = 1;

	strcpy( dash_native_table[DASH_GLOB_REF_RVALUE_ACCESS].name, "DASH_GLOB_REF_RVALUE_ACCESS" );
	strcpy( dash_native_table[DASH_GLOB_REF_RVALUE_ACCESS].description,
			"This is an dash counter, that counts R-Value accesses to global references" );
	dash_native_table[DASH_GLOB_REF_RVALUE_ACCESS].writable = 1;

	strcpy( dash_native_table[DASH_GLOB_PTR_ACCESS].name, "DASH_GLOB_PTR_ACCESS" );
	strcpy( dash_native_table[DASH_GLOB_PTR_ACCESS].description,
			"This is an dash counter, that counts accesses to global pointers" );
	dash_native_table[DASH_GLOB_PTR_ACCESS].writable = 1;

	strcpy( dash_native_table[DASH_ONESIDED_PUT].name, "DASH_ONESIDED_PUT" );
	strcpy( dash_native_table[DASH_ONESIDED_PUT].description,
			"This is an dash counter, that counts the number of onesided put operations." );
	dash_native_table[DASH_ONESIDED_PUT].writable = 1;

	strcpy( dash_native_table[DASH_ONESIDED_GET].name, "DASH_ONESIDED_GET" );
	strcpy( dash_native_table[DASH_ONESIDED_GET].description,
			"This is an dash counter, that counts the number of onesided get operations." );
	dash_native_table[DASH_ONESIDED_GET].writable = 1;

	strcpy( dash_native_table[DASH_LOCAL_PTR_ACCESS].name, "DASH_LOCAL_PTR_ACCESS" );
	strcpy( dash_native_table[DASH_LOCAL_PTR_ACCESS].description,
			"This is an dash counter, that counts accesses to local pointers" );
	dash_native_table[DASH_LOCAL_PTR_ACCESS].writable = 1;

	/* Export the total number of events available */
	_dash_vector.cmp_info.num_native_events = num_events;

	/* Export the component id */
	_dash_vector.cmp_info.CmpIdx = cidx;



	return PAPI_OK;
}

/** This is called whenever a thread is initialized */
static int
_dash_init_thread( hwd_context_t *ctx )
{

	dash_context_t *dash_context = (dash_context_t *)ctx;

	SUBDBG( "_dash_init_thread %p...", ctx );

	return PAPI_OK;
}



/** Setup a counter control state.
 *   In general a control state holds the hardware info for an
 *   EventSet.
 */

static int
_dash_init_control_state( hwd_control_state_t * ctl )
{
   SUBDBG( "dash_init_control_state... %p\n", ctl );

   dash_control_state_t *dash_ctl = ( dash_control_state_t * ) ctl;
   memset( dash_ctl, 0, sizeof ( dash_control_state_t ) );

   return PAPI_OK;
}


/** Triggered by eventset operations like add or remove */
static int
_dash_update_control_state( hwd_control_state_t *ctl,
				    NativeInfo_t *native,
				    int count,
				    hwd_context_t *ctx )
{

   (void) ctx;
   int i, index;

   dash_control_state_t *dash_ctl = ( dash_control_state_t * ) ctl;

   SUBDBG( "_dash_update_control_state %p %p...", ctl, ctx );

   /* if no events, return */
   if (count==0) return PAPI_OK;

   for( i = 0; i < count; i++ ) {
      index = native[i].ni_event;

      /* Map counter #i to Measure Event "index" */
      dash_ctl->which_counter[i]=index;

      /* We have no constraints on event position, so any event */
      /* can be in any slot.                                    */
      native[i].ni_position = i;
   }

   dash_ctl->num_events=count;

   return PAPI_OK;
}

/** Triggered by PAPI_start() */
static int
_dash_start( hwd_context_t *ctx, hwd_control_state_t *ctl )
{

        (void) ctx;
        (void) ctl;

	SUBDBG( "dash_start %p %p...", ctx, ctl );

	/* anything that would need to be set at counter start time */

	/* reset counters? */
        /* For hardware that cannot reset counters, store initial        */
        /*     counter state to the ctl and subtract it off at read time */

	/* start the counting ?*/

	return PAPI_OK;
}


/** Triggered by PAPI_stop() */
static int
_dash_stop( hwd_context_t *ctx, hwd_control_state_t *ctl )
{

        (void) ctx;
        (void) ctl;

	SUBDBG( "dash_stop %p %p...", ctx, ctl );

	/* anything that would need to be done at counter stop time */



	return PAPI_OK;
}


/** Triggered by PAPI_read()     */
/*     flags field is never set? */
static int
_dash_read( hwd_context_t *ctx, hwd_control_state_t *ctl,
			  long long **events, int flags )
{

   (void) flags;

   dash_context_t *dash_ctx = (dash_context_t *) ctx;
   dash_control_state_t *dash_ctl = ( dash_control_state_t *) ctl;

   SUBDBG( "dash_read... %p %d", ctx, flags );

   int i;

   /* Read counters into expected slot */
   for(i=0;i<dash_ctl->num_events;i++) {
      dash_ctl->counter[i] =
		dash_hardware_read( dash_ctl->which_counter[i],
				       dash_ctx );
   }

   /* return pointer to the values we read */
   *events = dash_ctl->counter;

   return PAPI_OK;
}

/** Triggered by PAPI_write(), but only if the counters are running */
/*    otherwise, the updated state is written to ESI->hw_start      */
static int
_dash_write( hwd_context_t *ctx, hwd_control_state_t *ctl,
			   long long *events )
{

        dash_context_t *dash_ctx = (dash_context_t *) ctx;
        dash_control_state_t *dash_ctl = ( dash_control_state_t *) ctl;

        int i;

	SUBDBG( "dash_write... %p %p", ctx, ctl );

        /* Write counters into expected slot */
        for(i=0;i<dash_ctl->num_events;i++) {
	   dash_hardware_write( dash_ctl->which_counter[i],
				   dash_ctx,
				   events[i] );
	}

	return PAPI_OK;
}


/** Triggered by PAPI_reset() but only if the EventSet is currently running */
/*  If the eventset is not currently running, then the saved value in the   */
/*  EventSet is set to zero without calling this routine.                   */
static int
_dash_reset( hwd_context_t *ctx, hwd_control_state_t *ctl )
{
        dash_context_t *event_ctx = (dash_context_t *)ctx;
	(void) ctl;

	SUBDBG( "dash_reset ctx=%p ctrl=%p...", ctx, ctl );

	/* Reset the hardware */
	dash_hardware_reset( event_ctx );

	return PAPI_OK;
}

/** Triggered by PAPI_shutdown() */
static int
_dash_shutdown_component(void)
{

	SUBDBG( "dash_shutdown_component..." );

        /* Free anything we allocated */

	papi_free(dash_native_table);

	return PAPI_OK;
}

/** Called at thread shutdown */
static int
_dash_shutdown_thread( hwd_context_t *ctx )
{

        (void) ctx;

	SUBDBG( "dash_shutdown_thread... %p", ctx );

	/* Last chance to clean up thread */

	return PAPI_OK;
}



/** This function sets various options in the component
  @param[in] ctx -- hardware context
  @param[in] code valid are PAPI_SET_DEFDOM, PAPI_SET_DOMAIN,
                        PAPI_SETDEFGRN, PAPI_SET_GRANUL and PAPI_SET_INHERIT
  @param[in] option -- options to be set
 */
static int
_dash_ctl( hwd_context_t *ctx, int code, _papi_int_option_t *option )
{

        (void) ctx;
	(void) code;
	(void) option;

	SUBDBG( "dash_ctl..." );

	return PAPI_OK;
}

/** This function has to set the bits needed to count different domains
    In particular: PAPI_DOM_USER, PAPI_DOM_KERNEL PAPI_DOM_OTHER
    By default return PAPI_EINVAL if none of those are specified
    and PAPI_OK with success
    PAPI_DOM_USER is only user context is counted
    PAPI_DOM_KERNEL is only the Kernel/OS context is counted
    PAPI_DOM_OTHER  is Exception/transient mode (like user TLB misses)
    PAPI_DOM_ALL   is all of the domains
 */
static int
_dash_set_domain( hwd_control_state_t * cntrl, int domain )
{
        (void) cntrl;

	int found = 0;
	SUBDBG( "dash_set_domain..." );

	if ( PAPI_DOM_USER & domain ) {
		SUBDBG( " PAPI_DOM_USER " );
		found = 1;
	}
	if ( PAPI_DOM_KERNEL & domain ) {
		SUBDBG( " PAPI_DOM_KERNEL " );
		found = 1;
	}
	if ( PAPI_DOM_OTHER & domain ) {
		SUBDBG( " PAPI_DOM_OTHER " );
		found = 1;
	}
	if ( PAPI_DOM_ALL & domain ) {
		SUBDBG( " PAPI_DOM_ALL " );
		found = 1;
	}
	if ( !found )
		return ( PAPI_EINVAL );

	return PAPI_OK;
}


/**************************************************************/
/* Naming functions, used to translate event numbers to names */
/**************************************************************/


/** Enumerate Native Events
 *   @param EventCode is the event of interest
 *   @param modifier is one of PAPI_ENUM_FIRST, PAPI_ENUM_EVENTS
 *  If your component has attribute masks then these need to
 *   be handled here as well.
 */
static int
_dash_ntv_enum_events( unsigned int *EventCode, int modifier )
{
  int index;


  switch ( modifier ) {

		/* return EventCode of first event */
	case PAPI_ENUM_FIRST:
	   /* return the first event that we support */

	   *EventCode = 0;
	   return PAPI_OK;

		/* return EventCode of next available event */
	case PAPI_ENUM_EVENTS:
	   index = *EventCode;

	   /* Make sure we have at least 1 more event after us */
	   if ( index < num_events - 1 ) {

	      /* This assumes a non-sparse mapping of the events */
	      *EventCode = *EventCode + 1;
	      return PAPI_OK;
	   } else {
	      return PAPI_ENOEVNT;
	   }
	   break;

	default:
	   return PAPI_EINVAL;
  }

  return PAPI_EINVAL;
}

/** Takes a native event code and passes back the name
 * @param EventCode is the native event code
 * @param name is a pointer for the name to be copied to
 * @param len is the size of the name string
 */
static int
_dash_ntv_code_to_name( unsigned int EventCode, char *name, int len )
{
  int index;

  index = EventCode;

  /* Make sure we are in range */
  if (index >= 0 && index < num_events) {
     strncpy( name, dash_native_table[index].name, len );
     return PAPI_OK;
  }

  return PAPI_ENOEVNT;
}

static int
_dash_ntv_name_to_code( char *name, unsigned int *EventCode )
{
  for(int i = 0; i < num_events; ++i) {
	if(strcmp(dash_native_table[i].name, name) == 0) {
	   *EventCode = i;
	   return PAPI_OK;
	}
  }

  return PAPI_ENOEVNT;
}


/** Takes a native event code and passes back the event description
 * @param EventCode is the native event code
 * @param descr is a pointer for the description to be copied to
 * @param len is the size of the descr string
 */
static int
_dash_ntv_code_to_descr( unsigned int EventCode, char *descr, int len )
{
  int index;
  index = EventCode;

  /* make sure event is in range */
  if (index >= 0 && index < num_events) {
     strncpy( descr, dash_native_table[index].description, len );
     return PAPI_OK;
  }

  return PAPI_ENOEVNT;
}

/** Vector that points to entry points for our component */
papi_vector_t _dash_vector = {
	.cmp_info = {
		/* default component information */
		/* (unspecified values are initialized to 0) */
                /* we explicitly set them to zero in this dash */
                /* to show what settings are available            */
		.name = "dash",
		.short_name = "dash",
		.description = "A simple dash component",
		.version = "1.15",
		.support_version = "n/a",
		.kernel_version = "n/a",
		.num_cntrs =               DASH_MAX_SIMULTANEOUS_COUNTERS,
		.num_mpx_cntrs =           DASH_MAX_SIMULTANEOUS_COUNTERS,
		.default_domain =          PAPI_DOM_USER,
		.available_domains =       PAPI_DOM_USER,
		.default_granularity =     PAPI_GRN_THR,
		.available_granularities = PAPI_GRN_THR,
		.hardware_intr_sig =       PAPI_INT_SIGNAL,

		/* component specific cmp_info initializations */
	},

	/* sizes of framework-opaque component-private structures */
	.size = {
	        /* once per thread */
		.context = sizeof ( dash_context_t ),
	        /* once per eventset */
		.control_state = sizeof ( dash_control_state_t ),
	        /* ?? */
		.reg_value = sizeof ( dash_register_t ),
	        /* ?? */
		.reg_alloc = sizeof ( dash_reg_alloc_t ),
	},

	/* function pointers */
        /* by default they are set to NULL */

	/* Used for general PAPI interactions */
	.start =                _dash_start,
	.stop =                 _dash_stop,
	.read =                 _dash_read,
	.reset =                _dash_reset,
	.write =                _dash_write,
	.init_component =       _dash_init_component,
	.init_thread =          _dash_init_thread,
	.init_control_state =   _dash_init_control_state,
	.update_control_state = _dash_update_control_state,
	.ctl =                  _dash_ctl,
	.shutdown_thread =      _dash_shutdown_thread,
	.shutdown_component =   _dash_shutdown_component,
	.set_domain =           _dash_set_domain,
	/* .cleanup_eventset =     NULL, */
	/* called in add_native_events() */
	/* .allocate_registers =   NULL, */

	/* Used for overflow/profiling */
	/* .dispatch_timer =       NULL, */
	/* .get_overflow_address = NULL, */
	/* .stop_profiling =       NULL, */
	/* .set_overflow =         NULL, */
	/* .set_profile =          NULL, */

	/* ??? */
	/* .user =                 NULL, */

	/* Name Mapping Functions */
	.ntv_enum_events =   _dash_ntv_enum_events,
	.ntv_code_to_name =  _dash_ntv_code_to_name,
	.ntv_code_to_descr = _dash_ntv_code_to_descr,
        /* if .ntv_name_to_code not available, PAPI emulates  */
        /* it by enumerating all events and looking manually  */
   	.ntv_name_to_code  = _dash_ntv_name_to_code,


	/* These are only used by _papi_hwi_get_native_event_info() */
	/* Which currently only uses the info for printing native   */
	/* event info, not for any sort of internal use.            */
	/* .ntv_code_to_bits =  NULL, */

};

