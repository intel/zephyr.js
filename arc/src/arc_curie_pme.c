// Copyright (c) 2017, Intel Corporation.

#include "arc_curie_pme.h"

static u16_t nsr_save = 0;

// Default initializer
void curie_pme_begin(void)
{
    u16_t saved_nsr = reg_read16(NSR);
    curie_pme_forget();

    reg_write16(NSR, (u16_t)NSR_NET_MODE);

    for (int i = 0; i < MAX_NEURONS; i++) {
        reg_write16(TESTCOMP, 0);
    }

    reg_write16(TESTCAT, 0);
    reg_write16(NSR, saved_nsr);
}

void curie_pme_configure(u16_t global_context,
                         PATTERN_MATCHING_DISTANCE_MODE distance_mode,
                         PATTERN_MATCHING_CLASSIFICATION_MODE classification_mode,
                         u16_t min_if,
                         u16_t max_if)
{
    reg_write16(GCR, (global_context | (distance_mode << 7)));
    reg_write16(NSR, reg_read16(NSR) | (classification_mode << 5));
    reg_write16(MINIF, min_if);
    reg_write16(MAXIF, max_if);
}

// clear all commits in the network and make it ready to learn
void curie_pme_forget(void) { reg_write16(FORGET_NCOUNT, 0); }

// mark --learn and classify--
u16_t curie_pme_learn(u8_t *pattern_vector,
                      s32_t vector_length,
                      u16_t category)
{
    if (vector_length > MAX_VECTOR_SIZE)
        vector_length = MAX_VECTOR_SIZE;

    for (int i = 0; i < vector_length - 1; i++) {
        reg_write16(COMP, pattern_vector[i]);
    }

    reg_write16(LCOMP, pattern_vector[vector_length - 1]);

    // Mask off the 15th bit-- valid categories range from 1-32766,
    // and bit 15 is used to indicate if a firing neuron has degenerated
    reg_write16(CAT, (reg_read16(CAT) & ~CAT_CATEGORY) | (category & CAT_CATEGORY));

    return reg_read16(FORGET_NCOUNT);
}

u16_t curie_pme_classify(u8_t *pattern_vector, s32_t vector_length)
{
    u8_t *current_vector = pattern_vector;

    if (vector_length > MAX_VECTOR_SIZE)
        return -1;

    for (u8_t index = 0; index < (vector_length - 1); index++) {
        reg_write16(COMP, current_vector[index]);
    }

    reg_write16(LCOMP, current_vector[vector_length - 1]);
    reg_read16(IDX_DIST); // Sort by distance by David Florey

    return (reg_read16(CAT) & CAT_CATEGORY);
}

// write vector is used for kNN recognition and does not alter
// the CAT register, which moves the chain along.
u16_t curie_pme_write_vector(u8_t *pattern_vector, s32_t vector_length)
{
    u8_t *current_vector = pattern_vector;
    u8_t index = 0;

    if (vector_length > MAX_VECTOR_SIZE)
        return -1;

    for (index = 0; index < (vector_length - 1); index++) {
        reg_write16(COMP, current_vector[index]);
    }

    reg_write16(LCOMP, current_vector[vector_length - 1]);

    return 0;
}

// retrieve the data of a specific neuron element by ID, between 1 and 128.
u16_t curie_pme_read_neuron(s32_t neuron_id, neuron_data_t *data_array)
{
    u16_t dummy = 0;

    // range check the id - technically, this should be an error.

    if (neuron_id < FIRST_NEURON_ID)
        neuron_id = FIRST_NEURON_ID;
    if (neuron_id > LAST_NEURON_ID)
        neuron_id = LAST_NEURON_ID;

    // use the begin_save_mode method
    curie_pme_begin_save_mode();

    // iterate over n elements in order to reach the one we want.
    for (int i = 0; i < (neuron_id - 1); i++) {
        dummy = reg_read16(CAT);
    }

    // retrieve the data using the iterateToSave method
    curie_pme_iterate_neurons_to_save(data_array);

    // restore the network to how we found it.
    curie_pme_end_save_mode();

    return 0;
}

// mark --save and restore network--

// save and restore knowledge
void curie_pme_begin_save_mode(void)
{
    nsr_save = reg_read16(NSR);

    // set save/restore mode in the NSR
    reg_write16(NSR, reg_read16(NSR) | NSR_NET_MODE);
    // reset the chain to 0th neuron
    reg_write16(RSTCHAIN, 0);
}

// pass the function a structure to save data into
u16_t curie_pme_iterate_neurons_to_save(neuron_data_t *array)
{
    array->context = reg_read16(NCR);
    for (int i = 0; i < SAVE_RESTORE_SIZE; i++) {
        array->vector[i] = reg_read16(COMP);
    }

    array->aif = reg_read16(AIF);
    array->min_if = reg_read16(MINIF);
    array->category = reg_read16(CAT);

    return array->category;
}

void curie_pme_end_save_mode(void)
{
    // restore the network to how we found it.
    reg_write16(NSR, (nsr_save & ~NSR_NET_MODE));
}

void curie_pme_begin_restore_mode(void)
{
    nsr_save = reg_read16(NSR);

    curie_pme_forget();
    // set save/restore mode in the NSR
    reg_write16(NSR, reg_read16(NSR) | NSR_NET_MODE);
    // reset the chain to 0th neuron
    reg_write16(RSTCHAIN, 0);
}

u16_t curie_pme_iterate_neurons_to_restore(neuron_data_t *array)
{
    reg_write16(NCR, array->context);
    for (int i = 0; i < SAVE_RESTORE_SIZE; i++) {
        reg_write16(COMP, array->vector[i]);
    }

    reg_write16(AIF, array->aif);
    reg_write16(MINIF, array->min_if);
    reg_write16(CAT, array->category);

    return 0;
}

void curie_pme_end_restore_mode(void)
{
    // restore the network to how we found it.
    reg_write16(NSR, (nsr_save & ~NSR_NET_MODE));
}

// mark -- getter and setters--
PATTERN_MATCHING_DISTANCE_MODE curie_pme_get_distance_mode(void) // L1 or LSup
{
    return (GCR_DIST & reg_read16(GCR)) ? LSUP_DISTANCE : L1_DISTANCE;
}

void curie_pme_set_distance_mode(PATTERN_MATCHING_DISTANCE_MODE mode) // L1 or LSup
{
    u16_t rd = reg_read16(GCR);

    // do a read modify write on the GCR register
    reg_write16(GCR, (mode == LSUP_DISTANCE) ? rd | GCR_DIST : rd & ~GCR_DIST);
}

u16_t curie_pme_get_global_context(void)
{
    return (GCR_GLOBAL & reg_read16(GCR));
}

// A valid context value is in the range of 1-127. A context
// value of 0 enables all neurons, without regard to their context
void curie_pme_set_global_context(u16_t context)
{
    u16_t gcr_mask = ~GCR_GLOBAL & reg_read16(GCR);
    gcr_mask |= (context & GCR_GLOBAL);
    reg_write16(GCR, gcr_mask);
}

u16_t curie_pme_get_neuron_context(void)
{
    return (NCR_CONTEXT & reg_read16(NCR));
}

// valid range is 1-127
void curie_pme_set_neuron_context(u16_t context)
{
    u16_t ncr_mask = ~NCR_CONTEXT & reg_read16(NCR);
    ncr_mask |= (context & NCR_CONTEXT);
    reg_write16(NCR, ncr_mask);
}

u16_t curie_pme_get_committed_count(void)
{
    return (getFORGET_NCOUNT() & 0xff);
}

PATTERN_MATCHING_CLASSIFICATION_MODE curie_pme_get_classifier_mode(void) // RBF or KNN
{
    if (reg_read16(NSR) & NSR_CLASS_MODE)
        return KNN_MODE;

    return RBF_MODE;
}

void curie_pme_set_classifier_mode(PATTERN_MATCHING_CLASSIFICATION_MODE mode)
{
    u16_t mask = reg_read16(NSR);
    mask &= ~NSR_CLASS_MODE;

    if (mode == KNN_MODE)
        mask |= NSR_CLASS_MODE;

    reg_write16(NSR, mask);
}
