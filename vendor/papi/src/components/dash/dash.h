#ifndef DASH_H
#define DASH_H

#define DASH_GLOB_REF_LVALUE_ACCESS             0
#define DASH_GLOB_REF_RVALUE_ACCESS             1
#define DASH_GLOB_PTR_ACCESS                    2
#define DASH_ONESIDED_PUT                       3
#define DASH_ONESIDED_GET                       4
#define DASH_LOCAL_PTR_ACCESS                   5

#define DASH_TOTAL_EVENTS        				6
#define DASH_MAX_SIMULTANEOUS_COUNTERS 			6
#define DASH_MAX_MULTIPLEX_COUNTERS 			6

extern long long _papi_component_dash_values[DASH_TOTAL_EVENTS];

long long* _papi_dash_values();
#endif
