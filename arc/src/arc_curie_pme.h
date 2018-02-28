// Copyright (c) 2017-2018, Intel Corporation.

#ifndef _ARC_CURIE_PME_H_
#define _ARC_CURIE_PME_H_

// Zephyr includes
#include <zephyr/types.h>

static const u32_t NO_MATCH = 0x7fff;
static const u16_t MIN_CONTEXT = 0;
static const u16_t MAX_CONTEXT = 127;
static const s32_t MAX_VECTOR_SIZE = 128;
static const s32_t FIRST_NEURON_ID = 1;
static const s32_t LAST_NEURON_ID = 128;
static const s32_t MAX_NEURONS = 128;
static const s32_t SAVE_RESTORE_SIZE = 128;

typedef enum {
    RBF_MODE = 0,
    KNN_MODE = 1
} PME_CLASSIFICATION_MODE;

typedef enum {
    L1_DISTANCE = 0,
    LSUP_DISTANCE = 1
} PME_DISTANCE_MODE;

typedef struct neuron_data {
    u16_t context;
    u16_t aif;
    u16_t min_if;
    u16_t category;
    u8_t vector[128];
} neuron_data_t;

// Default initializer
void curie_pme_begin(void);

void curie_pme_forget(void);

void curie_pme_configure(u16_t global_context,
                         PME_DISTANCE_MODE distance_mode,
                         PME_CLASSIFICATION_MODE classification_mode,
                         u16_t min_if,
                         u16_t max_if);

u16_t curie_pme_learn(u8_t *pattern_vector,
                      s32_t vector_length,
                      u16_t category);
u16_t curie_pme_classify(u8_t *pattern_vector, s32_t vector_length);

u16_t curie_pme_read_neuron(s32_t neuron_id, neuron_data_t *data_array);

// save and restore knowledge
void curie_pme_begin_save_mode(void);
u16_t curie_pme_iterate_neurons_to_save(neuron_data_t *data_array);
void curie_pme_end_save_mode(void);

void curie_pme_begin_restore_mode(void);
u16_t curie_pme_iterate_neurons_to_restore(neuron_data_t *data_array);
void curie_pme_end_restore_mode(void);

// getter and setters
PME_DISTANCE_MODE curie_pme_get_distance_mode(void);
void curie_pme_set_distance_mode(PME_DISTANCE_MODE mode);
u16_t curie_pme_get_global_context(void);
void curie_pme_set_global_context(u16_t context);  // valid range is 1-127
u16_t curie_pme_get_neuron_context(void);
void curie_pme_set_neuron_context(u16_t context);  // valid range is 1-127

// NOTE: get_committed_count() will give inaccurate value if the network is in
// Save/Restore mode.
// It should not be called between the begin_save_mode() and end_save_mode() or
// between
// begin_restore_mode() and end_restore_mode()
u16_t curie_pme_get_committed_count(void);

PME_CLASSIFICATION_MODE curie_pme_get_classifier_mode(void);
void curie_pme_set_classifier_mode(PME_CLASSIFICATION_MODE mode);

// write vector is used for kNN recognition and does not alter
// the CAT register, which moves the chain along.
u16_t curie_pme_write_vector(u8_t *pattern_vector, s32_t vector_length);

// base address of the pattern matching accelerator in Intel(r) Curie(tm) and
// QuarkSE(tm)
static const u32_t baseAddress = 0xB0600000L;

typedef enum {
    NCR = 0x00,           // Neuron Context Register
    COMP = 0x04,          // Component Register
    LCOMP = 0x08,         // Last Component
    IDX_DIST = 0x0C,      // Write Component Index / Read Distance
    CAT = 0x10,           // Category Register
    AIF = 0x14,           // Active Influence Field
    MINIF = 0x18,         // Minimum Influence Field
    MAXIF = 0x1C,         // Maximum Influence Field
    TESTCOMP = 0x20,      // Write Test Component
    TESTCAT = 0x24,       // Write Test Category
    NID = 0x28,           // Network ID
    GCR = 0x2C,           // Global Context Register
    RSTCHAIN = 0x30,      // Reset Chain
    NSR = 0x34,           // Network Status Register
    FORGET_NCOUNT = 0x3C  // Forget Command / Neuron Count
} registers_t;

typedef enum {
    NCR_ID = 0xFF00,              // Upper 8-bit of Neuron ID
    NCR_NORM = 0x0040,            // 1 = LSUP, 0 = L1
    NCR_CONTEXT = 0x007F,         // Neuron Context
    CAT_DEGEN = 0x8000,           // Indicates neuron is degenerate
    CAT_CATEGORY = 0x7FFF,        // the category associated with a neuron
    GCR_DIST = 0x0080,            // distance type, 1 = Lsup, 0 = L1
    GCR_GLOBAL = 0x007F,          // the context of the neuron
    NSR_CLASS_MODE = 0x0020,      // Classifier mode 1 = KNN, 0 = RBF
    NSR_NET_MODE = 0x0010,        // 1 = save/restore 0 = learn/recognize
    NSR_ID_FLAG = 0x0008,         // Indicates positive identification
    NSR_UNCERTAIN_FLAG = 0x0004,  // Indicates uncertain identification
} Masks;

// all pattern matching accelerator registers are 16-bits wide, memory-addressed
// define efficient inline register access
inline volatile u16_t *reg_address(registers_t reg)
{
    return (u16_t *)(0xB0600000L + reg);
}

inline u16_t reg_read16(registers_t reg)
{
    return *reg_address(reg);
}

inline void reg_write16(registers_t reg, u16_t value)
{
    *reg_address(reg) = value;
}

// raw register access - not recommended.
inline u16_t getNCR(void) { return reg_read16(NCR); }
inline u16_t getCOMP(void) { return reg_read16(COMP); }
inline u16_t getLCOMP(void) { return reg_read16(LCOMP); }
inline u16_t getIDX_DIST(void) { return reg_read16(IDX_DIST); }
inline u16_t getCAT(void) { return reg_read16(CAT); }
inline u16_t getAIF(void) { return reg_read16(AIF); }
inline u16_t getMINIF(void) { return reg_read16(MINIF); }
inline u16_t getMAXIF(void) { return reg_read16(MAXIF); }
inline u16_t getNID(void) { return reg_read16(NID); }
inline u16_t getGCR(void) { return reg_read16(GCR); }
inline u16_t getRSTCHAIN(void) { return reg_read16(RSTCHAIN); }
inline u16_t getNSR(void) { return reg_read16(NSR); }
inline u16_t getFORGET_NCOUNT(void) { return reg_read16(FORGET_NCOUNT); }

#endif
