// Copyright (c) 2017-2018, Intel Corporation.

// C includes
#include <string.h>

// Zephyr includes
#include <zephyr.h>

// ZJS includes
#include "arc_common.h"
#include "arc_curie_pme.h"
#include "arc_pme.h"

#ifdef BUILD_MODULE_PME
static bool save_mode = false;
static bool restore_mode = false;
#endif

void arc_handle_pme(struct zjs_ipm_message *msg)
{
    u32_t error_code = ERROR_IPM_NONE;

    switch (msg->type) {
    case TYPE_PME_BEGIN:
        curie_pme_begin();
        break;
    case TYPE_PME_FORGET:
        curie_pme_forget();
        break;
    case TYPE_PME_CONFIGURE:
        // valid range is 1-127
        if (msg->data.pme.g_context < 1 || msg->data.pme.g_context > 127) {
            ERR_PRINT("context range has to be 1-127\n");
            ipm_send_error(msg, ERROR_IPM_INVALID_PARAMETER);
            return;
        }

        if (msg->data.pme.c_mode != RBF_MODE &&
            msg->data.pme.c_mode != KNN_MODE) {
            ERR_PRINT("invalid classifier mode\n");
            ipm_send_error(msg, ERROR_IPM_INVALID_PARAMETER);
            return;
        }

        if (msg->data.pme.d_mode != L1_DISTANCE &&
            msg->data.pme.d_mode != LSUP_DISTANCE) {
            ERR_PRINT("invalid distance mode\n");
            ipm_send_error(msg, ERROR_IPM_INVALID_PARAMETER);
            return;
        }

        curie_pme_configure(msg->data.pme.g_context, msg->data.pme.d_mode,
                            msg->data.pme.c_mode, msg->data.pme.min_if,
                            msg->data.pme.max_if);
        break;
    case TYPE_PME_LEARN:
        DBG_PRINT("learning: category=%d is %lu byte vector\n",
                  msg->data.pme.category, msg->data.pme.vector_size);
        if (msg->data.pme.vector_size > MAX_VECTOR_SIZE) {
            ERR_PRINT("vector cannot be greater than %d\n", MAX_VECTOR_SIZE);
            ipm_send_error(msg, ERROR_IPM_INVALID_PARAMETER);
            return;
        }

        for (int i = 0; i < msg->data.pme.vector_size; i++) {
            DBG_PRINT("%d ", msg->data.pme.vector[i]);
        }

        curie_pme_learn(msg->data.pme.vector, msg->data.pme.vector_size,
                        msg->data.pme.category);
        DBG_PRINT("\nlearned with %d neruons\n",
                  curie_pme_get_committed_count());
        break;
    case TYPE_PME_CLASSIFY:
        DBG_PRINT("classify: %lu byte vector\n", msg->data.pme.category,
                  msg->data.pme.vector_size);
        if (msg->data.pme.vector_size > MAX_VECTOR_SIZE) {
            ERR_PRINT("vector cannot be greater than %d\n", MAX_VECTOR_SIZE);
            ipm_send_error(msg, ERROR_IPM_INVALID_PARAMETER);
            return;
        }

        for (int i = 0; i < msg->data.pme.vector_size; i++) {
            DBG_PRINT("%d ", msg->data.pme.vector[i]);
        }

        msg->data.pme.category = curie_pme_classify(msg->data.pme.vector,
                                                    msg->data.pme.vector_size);
        DBG_PRINT("\ncategory: %d\n", msg->data.pme.category);
        break;
    case TYPE_PME_READ_NEURON:
        DBG_PRINT("read: neuron id=%d\n", msg->data.pme.neuron_id);
        if (msg->data.pme.neuron_id < 1 || msg->data.pme.neuron_id > 128) {
            ERR_PRINT("invalid neuron id, must be between 1 and 128\n");
            ipm_send_error(msg, ERROR_IPM_INVALID_PARAMETER);
            return;
        }

        neuron_data_t data;
        curie_pme_read_neuron(msg->data.pme.neuron_id, &data);
        msg->data.pme.category = data.category;
        msg->data.pme.n_context = data.context;
        msg->data.pme.aif = data.aif;
        msg->data.pme.min_if = data.min_if;
        memcpy(msg->data.pme.vector, data.vector, sizeof(data.vector));
        DBG_PRINT("neuron: id=%d, CTX=%d, AIF=%d MIF=%d, cat=%d\n",
                  data.context & NCR_ID, data.context & NCR_CONTEXT, data.aif,
                  data.min_if, data.category);
        break;
    case TYPE_PME_WRITE_VECTOR:
        DBG_PRINT("write vector: %lu byte vector\n", msg->data.pme.category,
                  msg->data.pme.vector_size);
        if (msg->data.pme.vector_size > MAX_VECTOR_SIZE) {
            ERR_PRINT("vector cannot be greater than %d\n", MAX_VECTOR_SIZE);
            ipm_send_error(msg, ERROR_IPM_INVALID_PARAMETER);
            return;
        }

        if (curie_pme_get_classifier_mode() != KNN_MODE) {
            ERR_PRINT("write vector only supports KNN_MODE\n");
            ipm_send_error(msg, ERROR_IPM_INVALID_PARAMETER);
            return;
        }

        for (int i = 0; i < msg->data.pme.vector_size; i++) {
            DBG_PRINT("%d ", msg->data.pme.vector[i]);
        }

        curie_pme_write_vector(msg->data.pme.vector, msg->data.pme.vector_size);
        DBG_PRINT("\nwrote with %d neruons\n", curie_pme_get_committed_count());
        break;
    case TYPE_PME_GET_COMMITED_COUNT:
        msg->data.pme.committed_count = curie_pme_get_committed_count();
        DBG_PRINT("committed count: %d\n", msg->data.pme.committed_count);
        break;
    case TYPE_PME_GET_GLOBAL_CONTEXT:
        msg->data.pme.g_context = curie_pme_get_global_context();
        DBG_PRINT("global context: %d\n", msg->data.pme.g_context);
        break;
    case TYPE_PME_GET_CLASSIFIER_MODE:
        msg->data.pme.c_mode = curie_pme_get_classifier_mode();
        DBG_PRINT("classifier mode: %d\n", msg->data.pme.c_mode);
        break;
    case TYPE_PME_SET_CLASSIFIER_MODE:
        if (msg->data.pme.c_mode != RBF_MODE &&
            msg->data.pme.c_mode != KNN_MODE) {
            ERR_PRINT("invalid classifier mode\n");
            ipm_send_error(msg, ERROR_IPM_INVALID_PARAMETER);
            return;
        }

        curie_pme_set_classifier_mode(msg->data.pme.c_mode);
        DBG_PRINT("classifier mode: %d\n", msg->data.pme.c_mode);
        break;
    case TYPE_PME_GET_DISTANCE_MODE:
        msg->data.pme.d_mode = curie_pme_get_distance_mode();
        DBG_PRINT("distance mode: %d\n", msg->data.pme.d_mode);
        break;
    case TYPE_PME_SET_DISTANCE_MODE:
        if (msg->data.pme.d_mode != L1_DISTANCE &&
            msg->data.pme.d_mode != LSUP_DISTANCE) {
            ERR_PRINT("invalid distance mode\n");
            ipm_send_error(msg, ERROR_IPM_INVALID_PARAMETER);
            return;
        }

        curie_pme_set_distance_mode(msg->data.pme.d_mode);
        DBG_PRINT("distance mode: %d\n", msg->data.pme.d_mode);
        break;
    case TYPE_PME_BEGIN_SAVE_MODE:
        curie_pme_begin_save_mode();
        save_mode = true;
        DBG_PRINT("begin save mode\n");
        break;
    case TYPE_PME_ITERATE_TO_SAVE:
        if (!save_mode) {
            ERR_PRINT("not in save mode\n");
            ipm_send_error(msg, ERROR_IPM_OPERATION_FAILED);
        }

        curie_pme_iterate_neurons_to_save(&data);
        msg->data.pme.category = data.category;
        msg->data.pme.n_context = data.context;
        msg->data.pme.aif = data.aif;
        msg->data.pme.min_if = data.min_if;
        memcpy(msg->data.pme.vector, data.vector, sizeof(data.vector));
        DBG_PRINT("save neuron: id=%d, CTX=%d, AIF=%d MIF=%d, cat=%d\n",
                  data.context & NCR_ID, data.context & NCR_CONTEXT, data.aif,
                  data.min_if, data.category);
        break;
    case TYPE_PME_END_SAVE_MODE:
        curie_pme_end_save_mode();
        save_mode = false;
        DBG_PRINT("end save mode\n");
        break;
    case TYPE_PME_BEGIN_RESTORE_MODE:
        curie_pme_begin_restore_mode();
        restore_mode = true;
        DBG_PRINT("begin restore mode\n");
        break;
    case TYPE_PME_ITERATE_TO_RESTORE:
        if (!restore_mode) {
            ERR_PRINT("not in restore mode\n");
            ipm_send_error(msg, ERROR_IPM_OPERATION_FAILED);
        }

        data.category = msg->data.pme.category;
        data.context = msg->data.pme.n_context;
        data.aif = msg->data.pme.aif;
        data.min_if = msg->data.pme.min_if;
        memcpy(data.vector, msg->data.pme.vector, sizeof(msg->data.pme.vector));
        curie_pme_iterate_neurons_to_restore(&data);
        DBG_PRINT("restore neuron: id=%d, CTX=%d, AIF=%d MIF=%d, cat=%d\n",
                  data.context & NCR_ID, data.context & NCR_CONTEXT, data.aif,
                  data.min_if, data.category);

        break;
    case TYPE_PME_END_RESTORE_MODE:
        curie_pme_end_restore_mode();
        restore_mode = false;
        DBG_PRINT("end retore mode\n");
        break;

    default:
        ERR_PRINT("unsupported pme message type %u\n", msg->type);
        error_code = ERROR_IPM_NOT_SUPPORTED;
    }

    if (error_code != ERROR_IPM_NONE) {
        ipm_send_error(msg, error_code);
        return;
    }

    ipm_send_msg(msg);
}
