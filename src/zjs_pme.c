// Copyright (c) 2017, Intel Corporation.

// Zephyr includes
#include <zephyr.h>
#include <string.h>

// ZJS includes
#include "zjs_ipm.h"
#include "zjs_util.h"
#include "zjs_pme.h"

#define ZJS_PME_TIMEOUT_TICKS 5000
#define MAX_VECTOR_SIZE 128

static struct k_sem pme_sem;

static jerry_value_t zjs_pme_prototype;

#define CALL_REMOTE_FUNCTION(send, reply)                                  \
    ({                                                                     \
        if (!zjs_pme_ipm_send_sync(&send, &reply)) {                       \
            return zjs_error("ipm message failed or timed out!");          \
        }                                                                  \
        if (reply.error_code != ERROR_IPM_NONE) {                          \
            ERR_PRINT("error code: %u\n", (unsigned int)reply.error_code); \
            return zjs_error("error received");                            \
        }                                                                  \
    })

static bool zjs_pme_ipm_send_sync(zjs_ipm_message_t *send,
                                  zjs_ipm_message_t *result) {
    send->id = MSG_ID_PME;
    send->flags = 0 | MSG_SYNC_FLAG;
    send->user_data = (void *)result;
    send->error_code = ERROR_IPM_NONE;

    if (zjs_ipm_send(MSG_ID_PME, send) != 0) {
        ERR_PRINT("failed to send message\n");
        return false;
    }

    // block until reply or timeout, we shouldn't see the ARC
    // time out, if the ARC response comes back after it
    // times out, it could pollute the result on the stack
    if (k_sem_take(&pme_sem, ZJS_PME_TIMEOUT_TICKS)) {
        ERR_PRINT("FATAL ERROR, ipm timed out\n");
        return false;
    }

    return true;
}

static void ipm_msg_receive_callback(void *context, uint32_t id, volatile void *data)
{
    if (id != MSG_ID_PME)
        return;

    zjs_ipm_message_t *msg = (zjs_ipm_message_t *)(*(uintptr_t *)data);

    if ((msg->flags & MSG_SYNC_FLAG) == MSG_SYNC_FLAG) {
         zjs_ipm_message_t *result = (zjs_ipm_message_t *)msg->user_data;
        // synchrounus ipm, copy the results
        if (result)
            memcpy(result, msg, sizeof(zjs_ipm_message_t));

        // un-block sync api
        k_sem_give(&pme_sem);
    } else {
        // asynchronous ipm, should not get here
        ERR_PRINT("async message received\n");
    }
}

static ZJS_DECL_FUNC(zjs_pme_begin)
{
    zjs_ipm_message_t send, reply;
    send.type = TYPE_PME_BEGIN;

    CALL_REMOTE_FUNCTION(send, reply);
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_pme_forget)
{
    zjs_ipm_message_t send, reply;
    send.type = TYPE_PME_FORGET;

    CALL_REMOTE_FUNCTION(send, reply);
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_pme_learn)
{
    ZJS_VALIDATE_ARGS(Z_ARRAY, Z_NUMBER);
    jerry_value_t array = argv[0];
    uint32_t array_len = jerry_get_array_length(array);

    if (array_len > MAX_VECTOR_SIZE) {
        return zjs_error("exceeded max vector size");
    }

    zjs_ipm_message_t send, reply;
    send.type = TYPE_PME_LEARN;
    for (int i = 0; i < array_len; i++) {
        ZVAL vector = jerry_get_property_by_index(array, i);
        if (!jerry_value_is_number(vector)) {
            return zjs_error("invalid vector type");
        }
        uint8_t byte = jerry_get_number_value(vector);
        memcpy(&send.data.pme.vector[i], &byte, sizeof(uint8_t));
    }

    send.data.pme.vector_size = array_len;
    send.data.pme.category = jerry_get_number_value(argv[1]);

    CALL_REMOTE_FUNCTION(send, reply);
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_pme_classify)
{
    ZJS_VALIDATE_ARGS(Z_ARRAY);
    jerry_value_t array = argv[0];
    uint32_t array_len = jerry_get_array_length(array);

    if (array_len > MAX_VECTOR_SIZE) {
        return zjs_error("exceeded max vector size");
    }

    zjs_ipm_message_t send, reply;
    send.type = TYPE_PME_CLASSIFY;
    for (int i = 0; i < array_len; i++) {
        ZVAL vector = jerry_get_property_by_index(array, i);
        if (!jerry_value_is_number(vector)) {
            return zjs_error("invalid vector type");
        }
        uint8_t byte = jerry_get_number_value(vector);
        memcpy(&send.data.pme.vector[i], &byte, sizeof(uint8_t));
    }

    send.data.pme.vector_size = array_len;
    send.data.pme.category = jerry_get_number_value(argv[1]);

    CALL_REMOTE_FUNCTION(send, reply);
    return jerry_create_number(reply.data.pme.category);
}

static ZJS_DECL_FUNC(zjs_pme_read_neuron)
{
    ZJS_VALIDATE_ARGS(Z_NUMBER);
    zjs_ipm_message_t send, reply;
    send.type = TYPE_PME_READ_NEURON;
    send.data.pme.neuron_id = jerry_get_number_value(argv[0]);

    CALL_REMOTE_FUNCTION(send, reply);
    jerry_value_t obj = jerry_create_object();
    zjs_obj_add_number(obj, reply.data.pme.category, "category");
    zjs_obj_add_number(obj, reply.data.pme.context, "context");
    zjs_obj_add_number(obj, reply.data.pme.influence, "influence");
    zjs_obj_add_number(obj, reply.data.pme.influence, "minInfluence");

    ZVAL array = jerry_create_array(MAX_VECTOR_SIZE);
    for (int i = 0; i < MAX_VECTOR_SIZE; i++) {
        ZVAL val = jerry_create_number(reply.data.pme.vector[i]);
        jerry_set_property_by_index(array, i, val);
    }
    zjs_set_property(obj, "vector", array);
    return obj;
}

static ZJS_DECL_FUNC(zjs_pme_write_vector)
{
    ZJS_VALIDATE_ARGS(Z_ARRAY);
    jerry_value_t array = argv[0];
    uint32_t array_len = jerry_get_array_length(array);

    if (array_len > MAX_VECTOR_SIZE) {
        return zjs_error("exceeded max vector size");
    }

    zjs_ipm_message_t send, reply;
    send.type = TYPE_PME_WRITE_VECTOR;
    for (int i = 0; i < array_len; i++) {
        ZVAL vector = jerry_get_property_by_index(array, i);
        if (!jerry_value_is_number(vector)) {
            return zjs_error("invalid vector type");
        }
        uint8_t byte = jerry_get_number_value(vector);
        memcpy(&send.data.pme.vector[i], &byte, sizeof(uint8_t));
    }

    send.data.pme.vector_size = array_len;
    send.data.pme.category = jerry_get_number_value(argv[1]);

    CALL_REMOTE_FUNCTION(send, reply);
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_pme_get_committed_count)
{
    zjs_ipm_message_t send, reply;
    send.type = TYPE_PME_GET_COMMITED_COUNT;

    CALL_REMOTE_FUNCTION(send, reply);
    return jerry_create_number(reply.data.pme.committed_count);
}

static ZJS_DECL_FUNC(zjs_pme_get_global_context)
{
    zjs_ipm_message_t send, reply;
    send.type = TYPE_PME_GET_GLOBAL_CONTEXT;

    CALL_REMOTE_FUNCTION(send, reply);
    return jerry_create_number(reply.data.pme.context);
}

static ZJS_DECL_FUNC(zjs_pme_set_global_context)
{
    ZJS_VALIDATE_ARGS(Z_NUMBER);

    zjs_ipm_message_t send, reply;
    send.type = TYPE_PME_SET_GLOBAL_CONTEXT;
    send.data.pme.context = jerry_get_number_value(argv[0]);

    CALL_REMOTE_FUNCTION(send, reply);
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_pme_get_neuron_context)
{
    zjs_ipm_message_t send, reply;
    send.type = TYPE_PME_GET_NEURON_CONTEXT;

    CALL_REMOTE_FUNCTION(send, reply);
    return jerry_create_number(reply.data.pme.context);
}

static ZJS_DECL_FUNC(zjs_pme_set_neuron_context)
{
    ZJS_VALIDATE_ARGS(Z_NUMBER);

    zjs_ipm_message_t send, reply;
    send.type = TYPE_PME_SET_NEURON_CONTEXT;
    send.data.pme.context = jerry_get_number_value(argv[0]);

    CALL_REMOTE_FUNCTION(send, reply);
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_pme_get_classifier_mode)
{
    zjs_ipm_message_t send, reply;
    send.type = TYPE_PME_GET_CLASSIFIER_MODE;

    CALL_REMOTE_FUNCTION(send, reply);
    return jerry_create_number(reply.data.pme.mode);
}

static ZJS_DECL_FUNC(zjs_pme_set_classifier_mode)
{
    ZJS_VALIDATE_ARGS(Z_NUMBER);

    zjs_ipm_message_t send, reply;
    send.type = TYPE_PME_SET_CLASSIFIER_MODE;
    send.data.pme.mode = jerry_get_number_value(argv[0]);

    CALL_REMOTE_FUNCTION(send, reply);
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_pme_get_distance_mode)
{
    zjs_ipm_message_t send, reply;
    send.type = TYPE_PME_GET_DISTANCE_MODE;

    CALL_REMOTE_FUNCTION(send, reply);
    return jerry_create_number(reply.data.pme.mode);
}

static ZJS_DECL_FUNC(zjs_pme_set_distance_mode)
{
    ZJS_VALIDATE_ARGS(Z_NUMBER);

    zjs_ipm_message_t send, reply;
    send.type = TYPE_PME_SET_DISTANCE_MODE;
    send.data.pme.mode = jerry_get_number_value(argv[0]);

    CALL_REMOTE_FUNCTION(send, reply);
    return ZJS_UNDEFINED;
}

jerry_value_t zjs_pme_init()
{
    zjs_ipm_init();
    zjs_ipm_register_callback(MSG_ID_PME, ipm_msg_receive_callback);

    k_sem_init(&pme_sem, 0, 1);

    // create PME prototype object
    zjs_native_func_t array[] = {
        { zjs_pme_begin, "begin" },
        { zjs_pme_forget, "forget" },
        { zjs_pme_learn, "learn" },
        { zjs_pme_classify, "classify" },
        { zjs_pme_read_neuron, "readNeuron" },
        { zjs_pme_write_vector, "writeVector" },
        { zjs_pme_get_committed_count, "getCommittedCount" },
        { zjs_pme_get_global_context, "getGlobalContext" },
        { zjs_pme_set_global_context, "setGlobalContext" },
        { zjs_pme_get_neuron_context, "getNeuronContext" },
        { zjs_pme_set_neuron_context, "setNeuronContext" },
        { zjs_pme_get_classifier_mode, "getClassifierMode" },
        { zjs_pme_set_classifier_mode, "setClassifierMode" },
        { zjs_pme_get_distance_mode, "getDistanceMode" },
        { zjs_pme_set_distance_mode, "setDistanceMode" },
        { NULL, NULL }
    };
    zjs_pme_prototype = jerry_create_object();
    zjs_obj_add_functions(zjs_pme_prototype, array);

    // create global PME object
    jerry_value_t pme_obj = jerry_create_object();
    jerry_set_prototype(pme_obj, zjs_pme_prototype);

    // create object properties
    jerry_value_t val;
    val = jerry_create_number(0);
    zjs_set_property(pme_obj, "RBF_MODE", val);
    jerry_release_value(val);

    val = jerry_create_number(1);
    zjs_set_property(pme_obj, "KNN_MODE", val);
    jerry_release_value(val);

    val = jerry_create_number(0);
    zjs_set_property(pme_obj, "L1_DISTANCE", val);
    jerry_release_value(val);

    val = jerry_create_number(1);
    zjs_set_property(pme_obj, "LSUP_DISTANCE", val);
    jerry_release_value(val);

    val = jerry_create_number(0x7fff);
    zjs_set_property(pme_obj, "NO_MATCH", val);
    jerry_release_value(val);

    val = jerry_create_number(0);
    zjs_set_property(pme_obj, "MIN_CONTEXT", val);
    jerry_release_value(val);

    val = jerry_create_number(127);
    zjs_set_property(pme_obj, "MAX_CONTEXT", val);
    jerry_release_value(val);

    val = jerry_create_number(128);
    zjs_set_property(pme_obj, "MAX_VECTOR_SIZE", val);
    jerry_release_value(val);

    val = jerry_create_number(1);
    zjs_set_property(pme_obj, "FIRST_NEURON_ID", val);
    jerry_release_value(val);

    val = jerry_create_number(128);
    zjs_set_property(pme_obj, "LAST_NEURON_ID", val);
    jerry_release_value(val);

    val = jerry_create_number(128);
    zjs_set_property(pme_obj, "MAX_NEURONS", val);
    jerry_release_value(val);

    val = jerry_create_number(128);
    zjs_set_property(pme_obj, "SAVE_RESTORE_SIZE", val);
    jerry_release_value(val);

    return pme_obj;
}

void zjs_pme_cleanup()
{
    jerry_release_value(zjs_pme_prototype);
}
