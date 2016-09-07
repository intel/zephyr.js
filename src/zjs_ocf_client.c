#include "zjs_common.h"

jerry_value_t zjs_ocf_client_init()
{
    // create GPIO object
    jerry_value_t ocf = jerry_create_object();
    return ocf;
}
