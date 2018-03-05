// Copyright (c) 2017-2018, Intel Corporation.

// C includes
#include <string.h>

// Zephyr includes
#include <zephyr.h>

// ZJS includes
#include "zjs_buffer.h"
#include "zjs_ipm.h"
#include "zjs_util.h"

#define ZJS_PME_TIMEOUT_TICKS 5000

#define MAX_NEURONS     128
#define MAX_VECTOR_SIZE 128

#define RBF_MODE      0
#define KNN_MODE      1
#define L1_DISTANCE   0
#define LSUP_DISTANCE 1

#define NO_MATCH      0x7fff

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

#define CALL_REMOTE_FUNCTION_NO_REPLY(send) \
    ({                                      \
        zjs_ipm_message_t reply;            \
        CALL_REMOTE_FUNCTION(send, reply);  \
    })

static bool zjs_pme_ipm_send_sync(zjs_ipm_message_t *send,
                                  zjs_ipm_message_t *result)
{
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

static void ipm_msg_receive_callback(void *context, u32_t id,
                                     volatile void *data)
{
    if (id != MSG_ID_PME)
        return;

    zjs_ipm_message_t *msg = (zjs_ipm_message_t *)(*(uintptr_t *)data);

    if ((msg->flags & MSG_SYNC_FLAG) == MSG_SYNC_FLAG) {
        zjs_ipm_message_t *result = (zjs_ipm_message_t *)msg->user_data;

        // synchronous ipm, copy the results
        if (result) {
            *result = *msg;
        }

        // un-block sync api
        k_sem_give(&pme_sem);
    } else {
        // asynchronous ipm, should not get here
        ZJS_ASSERT(false, "async message received");
    }
}

static ZJS_DECL_FUNC(zjs_pme_begin)
{
    zjs_ipm_message_t send;
    send.type = TYPE_PME_BEGIN;

    CALL_REMOTE_FUNCTION_NO_REPLY(send);
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_pme_forget)
{
    zjs_ipm_message_t send;
    send.type = TYPE_PME_FORGET;

    CALL_REMOTE_FUNCTION_NO_REPLY(send);
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_pme_configure)
{
    // args: context, classification mode, distance mode, minIF, maxIF
    ZJS_VALIDATE_ARGS(Z_NUMBER, Z_NUMBER, Z_NUMBER, Z_NUMBER, Z_NUMBER);
    zjs_ipm_message_t send;
    send.type = TYPE_PME_CONFIGURE;

    send.data.pme.g_context = jerry_get_number_value(argv[0]);
    send.data.pme.c_mode = jerry_get_number_value(argv[1]);
    send.data.pme.d_mode = jerry_get_number_value(argv[2]);
    send.data.pme.min_if = jerry_get_number_value(argv[3]);
    send.data.pme.max_if = jerry_get_number_value(argv[4]);
    CALL_REMOTE_FUNCTION_NO_REPLY(send);
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_pme_learn)
{
    // args: pattern array, category
    ZJS_VALIDATE_ARGS(Z_ARRAY, Z_NUMBER);
    jerry_value_t vector = argv[0];
    u32_t vector_len = jerry_get_array_length(vector);

    if (vector_len > MAX_VECTOR_SIZE) {
        return zjs_error("exceeded max vector size");
    }

    zjs_ipm_message_t send;
    send.type = TYPE_PME_LEARN;

    for (int i = 0; i < vector_len; i++) {
        ZVAL num = jerry_get_property_by_index(vector, i);
        if (!jerry_value_is_number(num)) {
            return zjs_error("invalid vector type");
        }

        u8_t byte = jerry_get_number_value(num);
        send.data.pme.vector[i] = byte;
    }

    send.data.pme.vector_size = vector_len;
    send.data.pme.category = jerry_get_number_value(argv[1]);

    CALL_REMOTE_FUNCTION_NO_REPLY(send);
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_pme_classify)
{
    // args: pattern array
    ZJS_VALIDATE_ARGS(Z_ARRAY);
    jerry_value_t vector = argv[0];
    u32_t vector_len = jerry_get_array_length(vector);

    if (vector_len > MAX_VECTOR_SIZE) {
        return zjs_error("exceeded max vector size");
    }

    zjs_ipm_message_t send, reply;
    send.type = TYPE_PME_CLASSIFY;

    for (int i = 0; i < vector_len; i++) {
        ZVAL num = jerry_get_property_by_index(vector, i);
        if (!jerry_value_is_number(num)) {
            return zjs_error("invalid vector type");
        }

        u8_t byte = jerry_get_number_value(num);
        send.data.pme.vector[i] = byte;
    }

    send.data.pme.vector_size = vector_len;
    send.data.pme.category = jerry_get_number_value(argv[1]);

    CALL_REMOTE_FUNCTION(send, reply);
    return jerry_create_number(reply.data.pme.category);
}

static ZJS_DECL_FUNC(zjs_pme_read_neuron)
{
    // args: neuron id
    ZJS_VALIDATE_ARGS(Z_NUMBER);
    zjs_ipm_message_t send, reply;
    send.type = TYPE_PME_READ_NEURON;
    send.data.pme.neuron_id = jerry_get_number_value(argv[0]);

    CALL_REMOTE_FUNCTION(send, reply);
    jerry_value_t obj = zjs_create_object();
    zjs_obj_add_number(obj, "category", reply.data.pme.category);
    zjs_obj_add_number(obj, "context", reply.data.pme.n_context);
    zjs_obj_add_number(obj, "AIF", reply.data.pme.aif);
    zjs_obj_add_number(obj, "minIF", reply.data.pme.min_if);

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
    // args: pattern array
    ZJS_VALIDATE_ARGS(Z_ARRAY);
    jerry_value_t vector = argv[0];
    u32_t vector_len = jerry_get_array_length(vector);

    if (vector_len > MAX_VECTOR_SIZE) {
        return zjs_error("exceeded max vector size");
    }

    zjs_ipm_message_t send;
    send.type = TYPE_PME_WRITE_VECTOR;

    memset(send.data.pme.vector, 0, sizeof(u8_t) * MAX_VECTOR_SIZE);
    for (int i = 0; i < vector_len; i++) {
        ZVAL num = jerry_get_property_by_index(vector, i);
        if (!jerry_value_is_number(num)) {
            return zjs_error("invalid vector type");
        }

        u8_t byte = jerry_get_number_value(num);
        send.data.pme.vector[i] = byte;
    }

    send.data.pme.vector_size = vector_len;
    send.data.pme.category = jerry_get_number_value(argv[1]);

    CALL_REMOTE_FUNCTION_NO_REPLY(send);
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
    return jerry_create_number(reply.data.pme.g_context);
}

static ZJS_DECL_FUNC(zjs_pme_get_classifier_mode)
{
    zjs_ipm_message_t send, reply;
    send.type = TYPE_PME_GET_CLASSIFIER_MODE;

    CALL_REMOTE_FUNCTION(send, reply);
    return jerry_create_number(reply.data.pme.c_mode);
}

static ZJS_DECL_FUNC(zjs_pme_set_classifier_mode)
{
    // args: classifier mode
    ZJS_VALIDATE_ARGS(Z_NUMBER);

    u32_t mode = jerry_get_number_value(argv[0]);
    if (mode != RBF_MODE && mode != KNN_MODE) {
        return zjs_error("invalid classifier mode");
    }

    zjs_ipm_message_t send;
    send.type = TYPE_PME_SET_CLASSIFIER_MODE;
    send.data.pme.c_mode = mode;

    CALL_REMOTE_FUNCTION_NO_REPLY(send);
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_pme_get_distance_mode)
{
    zjs_ipm_message_t send, reply;
    send.type = TYPE_PME_GET_DISTANCE_MODE;

    CALL_REMOTE_FUNCTION(send, reply);
    return jerry_create_number(reply.data.pme.d_mode);
}

static ZJS_DECL_FUNC(zjs_pme_set_distance_mode)
{
    // args: distance mode
    ZJS_VALIDATE_ARGS(Z_NUMBER);

    u32_t mode = jerry_get_number_value(argv[0]);
    if (mode != L1_DISTANCE && mode != LSUP_DISTANCE) {
        return zjs_error("invalid distance mode");
    }

    zjs_ipm_message_t send;
    send.type = TYPE_PME_SET_DISTANCE_MODE;
    send.data.pme.d_mode = mode;

    CALL_REMOTE_FUNCTION_NO_REPLY(send);
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_pme_save_neurons)
{
    zjs_ipm_message_t send, reply;
    // to save memory we only export objects for commited neurons
    // so first retrieve the committed count
    send.type = TYPE_PME_GET_COMMITED_COUNT;
    CALL_REMOTE_FUNCTION(send, reply);
    int count = reply.data.pme.committed_count;

    // begin save mode
    send.type = TYPE_PME_BEGIN_SAVE_MODE;
    CALL_REMOTE_FUNCTION_NO_REPLY(send);

    jerry_value_t array_obj = jerry_create_array(count);

    // iterate neurons to save, only for committed neurons
    for (int i = 0; i < count; i++) {
        send.type = TYPE_PME_ITERATE_TO_SAVE;
        CALL_REMOTE_FUNCTION(send, reply);

        // create json object
        ZVAL obj = zjs_create_object();
        zjs_obj_add_number(obj, "category", reply.data.pme.category);
        zjs_obj_add_number(obj, "context", reply.data.pme.n_context);
        zjs_obj_add_number(obj, "AIF", reply.data.pme.aif);
        zjs_obj_add_number(obj, "minIF", reply.data.pme.min_if);

        int v_size = 0;

        // loop once to find out the size of vector
        for (int j = 0; j < MAX_VECTOR_SIZE; j++) {
            // ignore 0s as valid training data?
            if (reply.data.pme.vector[j] == 0) {
                break;
            }
            v_size++;
        }

        ZVAL array = jerry_create_array(v_size);
        for (int j = 0; j < v_size; j++) {
            ZVAL val = jerry_create_number(reply.data.pme.vector[j]);
            jerry_set_property_by_index(array, j, val);
        }
        zjs_set_property(obj, "vector", array);
        jerry_set_property_by_index(array_obj, i, obj);
    }

    // end save mode
    send.type = TYPE_PME_END_SAVE_MODE;
    CALL_REMOTE_FUNCTION_NO_REPLY(send);
    return array_obj;
}

static ZJS_DECL_FUNC(zjs_pme_restore_neurons)
{
    // args: neuron array
    ZJS_VALIDATE_ARGS(Z_ARRAY);
    jerry_value_t array = argv[0];
    u32_t array_len = jerry_get_array_length(array);

    if (array_len > MAX_NEURONS) {
        return zjs_error("exceeded max neuron size");
    }

    zjs_ipm_message_t send;
    // begin restore mode

    send.type = TYPE_PME_BEGIN_RESTORE_MODE;
    CALL_REMOTE_FUNCTION_NO_REPLY(send);

    for (int i = 0; i < array_len; i++) {
        // iterate json object to restore neurons
        u32_t category, context, aif, min_if;
        zjs_ipm_message_t send;

        ZVAL neuron = jerry_get_property_by_index(array, i);
        if (!jerry_value_is_object(neuron)) {
            return zjs_error("invalid neuron object");
        }
        if (!zjs_obj_get_uint32(neuron, "category", &category)) {
            return zjs_error("invalid object, missing category");
        }
        if (!zjs_obj_get_uint32(neuron, "context", &context)) {
            return zjs_error("invalid object, missing context");
        }
        if (!zjs_obj_get_uint32(neuron, "AIF", &aif)) {
            return zjs_error("invalid object, missing AIF");
        }
        if (!zjs_obj_get_uint32(neuron, "minIF", &min_if)) {
            return zjs_error("invalid object, missing minIF");
        }

        jerry_value_t vector = zjs_get_property(neuron, "vector");
        if (!jerry_value_is_array(vector)) {
            return zjs_error("invalid object, missing vector array");
        }

        u32_t vector_len = jerry_get_array_length(vector);
        if (vector_len > MAX_VECTOR_SIZE) {
            return zjs_error("vector size must be <= 128");
        }

        send.type = TYPE_PME_ITERATE_TO_RESTORE;
        send.data.pme.category = category;
        send.data.pme.n_context = context;
        send.data.pme.aif = aif;
        send.data.pme.min_if = min_if;

        memset(send.data.pme.vector, 0, sizeof(u8_t) * MAX_VECTOR_SIZE);
        for (int i = 0; i < vector_len; i++) {
            ZVAL num = jerry_get_property_by_index(vector, i);
            if (!jerry_value_is_number(num)) {
                return zjs_error("invalid vector type");
            }

            u8_t byte = jerry_get_number_value(num);
            send.data.pme.vector[i] = byte;
        }

        send.data.pme.vector_size = vector_len;
        CALL_REMOTE_FUNCTION_NO_REPLY(send);
    }

    // end restore mode
    send.type = TYPE_PME_END_RESTORE_MODE;
    CALL_REMOTE_FUNCTION_NO_REPLY(send);
    return ZJS_UNDEFINED;
}

static void zjs_pme_cleanup(void *native)
{
    jerry_release_value(zjs_pme_prototype);
}

static const jerry_object_native_info_t pme_module_type_info = {
    .free_cb = zjs_pme_cleanup
};

static jerry_value_t zjs_pme_init()
{
    zjs_ipm_init();
    zjs_ipm_register_callback(MSG_ID_PME, ipm_msg_receive_callback);

    k_sem_init(&pme_sem, 0, 1);

    // create PME prototype object
    zjs_native_func_t array[] = {
        { zjs_pme_begin, "begin" },
        { zjs_pme_forget, "forget" },
        { zjs_pme_configure, "configure" },
        { zjs_pme_learn, "learn" },
        { zjs_pme_classify, "classify" },
        { zjs_pme_read_neuron, "readNeuron" },
        { zjs_pme_write_vector, "writeVector" },
        { zjs_pme_get_committed_count, "getCommittedCount" },
        { zjs_pme_get_global_context, "getGlobalContext" },
        { zjs_pme_get_classifier_mode, "getClassifierMode" },
        { zjs_pme_set_classifier_mode, "setClassifierMode" },
        { zjs_pme_get_distance_mode, "getDistanceMode" },
        { zjs_pme_set_distance_mode, "setDistanceMode" },
        { zjs_pme_save_neurons, "saveNeurons" },
        { zjs_pme_restore_neurons, "restoreNeurons" },
        { NULL, NULL }
    };
    zjs_pme_prototype = zjs_create_object();
    zjs_obj_add_functions(zjs_pme_prototype, array);

    // create global PME object
    jerry_value_t pme_obj = zjs_create_object();
    jerry_set_prototype(pme_obj, zjs_pme_prototype);

    // create object properties
    jerry_value_t val;
    val = jerry_create_number(RBF_MODE);
    zjs_set_property(pme_obj, "RBF_MODE", val);
    jerry_release_value(val);

    val = jerry_create_number(KNN_MODE);
    zjs_set_property(pme_obj, "KNN_MODE", val);
    jerry_release_value(val);

    val = jerry_create_number(L1_DISTANCE);
    zjs_set_property(pme_obj, "L1_DISTANCE", val);
    jerry_release_value(val);

    val = jerry_create_number(LSUP_DISTANCE);
    zjs_set_property(pme_obj, "LSUP_DISTANCE", val);
    jerry_release_value(val);

    val = jerry_create_number(NO_MATCH);
    zjs_set_property(pme_obj, "NO_MATCH", val);
    jerry_release_value(val);

    val = jerry_create_number(0);
    zjs_set_property(pme_obj, "MIN_CONTEXT", val);
    jerry_release_value(val);

    val = jerry_create_number(127);
    zjs_set_property(pme_obj, "MAX_CONTEXT", val);
    jerry_release_value(val);

    val = jerry_create_number(MAX_VECTOR_SIZE);
    zjs_set_property(pme_obj, "MAX_VECTOR_SIZE", val);
    jerry_release_value(val);

    val = jerry_create_number(1);
    zjs_set_property(pme_obj, "FIRST_NEURON_ID", val);
    jerry_release_value(val);

    val = jerry_create_number(128);
    zjs_set_property(pme_obj, "LAST_NEURON_ID", val);
    jerry_release_value(val);

    val = jerry_create_number(MAX_NEURONS);
    zjs_set_property(pme_obj, "MAX_NEURONS", val);
    jerry_release_value(val);
    // Set up cleanup function for when the object gets freed
    jerry_set_object_native_pointer(pme_obj, NULL, &pme_module_type_info);
    return pme_obj;
}

JERRYX_NATIVE_MODULE(pme, zjs_pme_init)
