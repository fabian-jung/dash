/*****************************************************************************
* This example shows how to use PAPI_add_event, PAPI_start, PAPI_read,       *
*  PAPI_stop and PAPI_remove_event.                                          *
******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "papi.h" /* This needs to be included every time you use PAPI */

#define ERROR_RETURN(retval) { fprintf(stderr, "Error %d %s:line %d: \n", retval,__FILE__,__LINE__);  exit(retval); }

int find_component(const char* name) {
	int numCmps = PAPI_num_components();
	printf("Number of PAPI components:%i\n", numCmps);
	for(int i = 0; i < numCmps; ++i) {
		const PAPI_component_info_t* info = PAPI_get_component_info(i);
		if(strcmp(info->name, name) == 0) {
			printf("Component %s found id = %i\n", info->name, i);
			return i;
		} else {
// 			printf("Component %s\n", info->name);
		}
	}
	printf("Component %s could not be found\n", name);
	return -1;
}

struct counter_t {
	int id;
	char name[PAPI_MAX_STR_LEN];
};

int main()
{
	int retval, number;

	if((retval = PAPI_library_init(PAPI_VER_CURRENT)) != PAPI_VER_CURRENT )
		ERROR_RETURN(retval);
	PAPI_set_debug( PAPI_VERB_ECONT );
	int EventSet = PAPI_NULL;
	/*must be initialized to PAPI_NULL before calling PAPI_create_event*/
	int tmp;

	/*This is where we store the values we read from the eventset */

	/* We use number to keep track of the number of events in the EventSet */

	int dash_cix = find_component("dash");
	const PAPI_component_info_t* dash_info = PAPI_get_component_info(dash_cix);

	struct counter_t counters[dash_info->num_native_events];
	long long values[dash_info->num_native_events];
	printf("Number of counters = %i\n", dash_info->num_native_events);

	int EventCode = PAPI_NATIVE_MASK;
	if((retval = PAPI_enum_cmp_event(&EventCode, PAPI_ENUM_FIRST, dash_cix)) != PAPI_OK)
		ERROR_RETURN(retval);

	printf("First EventCode = %i\n", EventCode);
	counters[0].id = EventCode;
	int i = 0;
	while(PAPI_enum_cmp_event(&EventCode, PAPI_ENUM_EVENTS, dash_cix) == PAPI_OK) {
		printf("Next EventCode = %i\n", EventCode);
		counters[++i].id = EventCode;
	}

	/* Creating the eventset */
	if ( (retval = PAPI_create_eventset(&EventSet)) != PAPI_OK)
		ERROR_RETURN(retval);

	/* Assign dash as component to the eventset */
	if ( (retval = PAPI_assign_eventset_component(EventSet, dash_cix)) != PAPI_OK)
		ERROR_RETURN(retval);

	for(i = 0; i<dash_info->num_native_events; ++i) {
		if((retval = PAPI_event_code_to_name(counters[i].id, counters[i].name)) != PAPI_OK) {
			ERROR_RETURN(retval);
		}
		printf("Add Event %i:%s to EventSet\n",counters[i].id, counters[i].name);
		if ( (retval = PAPI_add_event(EventSet, counters[i].id)) != PAPI_OK)
			ERROR_RETURN(retval);

	}

	/* Start counting */

	if ( (retval = PAPI_start(EventSet)) != PAPI_OK)
		ERROR_RETURN(retval);

	/* you can replace your code here */
	tmp=0;
	for (i = 0; i < 2000000; i++)
	{
		tmp = i + tmp;
	}


	/* read the counter values and store them in the values array */
	if ( (retval=PAPI_read(EventSet, values)) != PAPI_OK)
		ERROR_RETURN(retval);

	printf("First value %lld \n", values[0] );

	/* our slow code again */
	tmp=0;
	for (i = 0; i < 2000000; i++)
	{
		tmp = i + tmp;
	}

	for(int i = 0; i < 6; ++i) {
		values[i] = 42+i;
	}
	if((retval=PAPI_write(EventSet, values)) != PAPI_OK)
		ERROR_RETURN(retval);
	for(int i = 0; i < 6; ++i) {
		values[i] = 0;
	}

// 	if ( (retval=PAPI_read(EventSet, values)) != PAPI_OK)
// 		ERROR_RETURN(retval);

	/* Stop counting and store the values into the array */
	if ( (retval = PAPI_stop(EventSet, values)) != PAPI_OK)
		ERROR_RETURN(retval);

	for(int i = 0; i < 6; ++i) {
		printf("First value[%i] %lld \n", i, values[i] );
	}
	/* free the resources used by PAPI */
	PAPI_shutdown();

	exit(0);
}



