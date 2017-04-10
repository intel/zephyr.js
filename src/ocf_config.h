// Copyright (c) 2017, Intel Corporation.

#ifndef __ocf_config_h__
#define __ocf_config_h__

#include <stdint.h>

/* Time resolution */
typedef uint64_t oc_clock_time_t;

/* Sets one clock tick to 1 ms */
#define OC_CLOCK_CONF_TICKS_PER_SECOND (1000)

/* Memory pool sizes */
#define OC_BYTES_POOL_SIZE (1500)
#define OC_INTS_POOL_SIZE (100)
#define OC_DOUBLES_POOL_SIZE (4)

/* Server-side parameters */
/* Maximum number of server resources */
#define OC_MAX_APP_RESOURCES (2)

#define OC_MAX_NUM_COLLECTIONS (1)

/* Common paramters */
/* Limit MTU size to lower layers, enable block-wise transfers */
//#define OC_BLOCK_WISE_SET_MTU (400)

/* Maximum size of request/response PDUs */
#define OC_MAX_PDU_BUFFER_SIZE (1024)

/* Maximum number of concurrent requests */
#define OC_MAX_NUM_CONCURRENT_REQUESTS (2)

/* Maximum number of nodes in a payload tree structure */
#define OC_MAX_NUM_REP_OBJECTS (100)

/* Number of devices on the OCF platform */
#define OC_MAX_NUM_DEVICES (1)

/* Platform payload size */
#define OC_MAX_PLATFORM_PAYLOAD_SIZE (256)

/* Device payload size */
#define OC_MAX_DEVICE_PAYLOAD_SIZE (256)

/* Security layer */
/* Maximum number of authorized clients */
#define OC_MAX_NUM_SUBJECTS (2)

/* Maximum number of concurrent DTLS sessions */
#define OC_MAX_DTLS_PEERS (1)

/* Max inactivity timeout before tearing down DTLS connection */
#define OC_DTLS_INACTIVITY_TIMEOUT (60)

#endif  // __ocf_config_h__
