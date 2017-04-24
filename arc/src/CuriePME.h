// Copyright (c) 2017, Intel Corporation.

#ifndef _CURIE_PME_H_
#define _CURIE_PME_H_

#include <stdint.h>

static const uint32_t noMatch = 0x7fff;
static const uint16_t minContext = 0;
static const uint16_t maxContext = 127;
static const int32_t maxVectorSize = 128;
static const int32_t firstNeuronID = 1;
static const int32_t lastNeuronID = 128;
static const int32_t maxNeurons = 128;
static const int32_t saveRestoreSize = 128;

typedef enum {
    RBF_Mode = 0,
    KNN_Mode = 1
} PATTERN_MATCHING_CLASSIFICATION_MODE;

typedef enum {
    L1_Distance = 0,
    LSUP_Distance = 1
} PATTERN_MATCHING_DISTANCE_MODE;

typedef struct neuronData {
    uint16_t context;
    uint16_t influence;
    uint16_t minInfluence;
    uint16_t category;
    uint8_t vector[128];
} neuronData;

// Default initializer
void CuriePME_begin(void);

void CuriePME_forget(void);

void CuriePME_configure(uint16_t global_context,
                        PATTERN_MATCHING_DISTANCE_MODE distance_mode,
                        PATTERN_MATCHING_CLASSIFICATION_MODE classification_mode,
                        uint16_t minAIF,
                        uint16_t maxAIF);

uint16_t CuriePME_learn(uint8_t* pattern_vector,
                        int32_t vector_length,
                        uint16_t category);
uint16_t CuriePME_classify(uint8_t* pattern_vector, int32_t vector_length);

uint16_t CuriePME_readNeuron(int32_t neuronID, neuronData* data_array);

// save and restore knowledge
void CuriePME_beginSaveMode(void); // saves the contents of the NSR register
uint16_t CuriePME_iterateNeuronsToSave(neuronData* data_array);
void CuriePME_endSaveMode(void); // restores the NSR value saved by beginSaveMode

void CuriePME_beginRestoreMode(void);
uint16_t CuriePME_iterateNeuronsToRestore(neuronData* data_array);
void CuriePME_endRestoreMode(void);

// getter and setters
PATTERN_MATCHING_DISTANCE_MODE CuriePME_getDistanceMode(void);
void CuriePME_setDistanceMode(PATTERN_MATCHING_DISTANCE_MODE mode);
uint16_t CuriePME_getGlobalContext(void);
void CuriePME_setGlobalContext(uint16_t context); // valid range is 1-127
uint16_t CuriePME_getNeuronContext(void);
void CuriePME_setNeuronContext(uint16_t context); // valid range is 1-127

// NOTE: getCommittedCount() will give inaccurate value if the network is in
// Save/Restore mode.
// It should not be called between the beginSaveMode() and endSaveMode() or
// between
// beginRestoreMode() and endRestoreMode()
uint16_t CuriePME_getCommittedCount(void);

PATTERN_MATCHING_CLASSIFICATION_MODE CuriePME_getClassifierMode(void); // RBF or KNN
void CuriePME_setClassifierMode(PATTERN_MATCHING_CLASSIFICATION_MODE mode);

// write vector is used for kNN recognition and does not alter
// the CAT register, which moves the chain along.
uint16_t CuriePME_writeVector(uint8_t* pattern_vector, int32_t vector_length);

// raw register access - not recommended.
uint16_t getNCR(void);
uint16_t getCOMP(void);
uint16_t getLCOMP(void);
uint16_t getIDX_DIST(void);
uint16_t getCAT(void);
uint16_t getAIF(void);
uint16_t getMINIF(void);
uint16_t getMAXIF(void);
uint16_t getNID(void);
uint16_t getGCR(void);
uint16_t getRSTCHAIN(void);
uint16_t getNSR(void);
uint16_t getFORGET_NCOUNT(void);

// base address of the pattern matching accelerator in Intel(r) Curie(tm) and
// QuarkSE(tm)
static const uint32_t baseAddress = 0xB0600000L;

typedef enum {
    NCR = 0x00, // Neuron Context Register
    COMP = 0x04, // Component Register
    LCOMP = 0x08, // Last Component
    IDX_DIST = 0x0C, // Write Component Index / Read Distance
    CAT = 0x10, // Category Register
    AIF = 0x14, // Active Influence Field
    MINIF = 0x18, // Minimum Influence Field
    MAXIF = 0x1C, // Maximum Influence Field
    TESTCOMP = 0x20, // Write Test Component
    TESTCAT = 0x24, // Write Test Category
    NID = 0x28, // Network ID
    GCR = 0x2C, // Global Context Register
    RSTCHAIN = 0x30, // Reset Chain
    NSR = 0x34, // Network Status Register
    FORGET_NCOUNT = 0x3C // Forget Command / Neuron Count
} Registers;

typedef enum {
    NCR_ID = 0xFF00, // Upper 8-bit of Neuron ID
    NCR_NORM = 0x0040, // 1 = LSUP, 0 = L1
    NCR_CONTEXT = 0x007F, // Neuron Context
    CAT_DEGEN = 0x8000, // Indicates neuron is degenerate
    CAT_CATEGORY = 0x7FFF, // the category associated with a neuron
    GCR_DIST = 0x0080, // distance type, 1 = Lsup, 0 = L1
    GCR_GLOBAL = 0x007F, // the context of the neuron, used to segment the network
    NSR_CLASS_MODE = 0x0020, // Classifier mode 1 = KNN, 0 = RBF (KNN not for learning mode)
    NSR_NET_MODE = 0x0010, // 1 = SR (save/restore) 0 = LR (learn/recognize)
    NSR_ID_FLAG = 0x0008, // Indicates positive identification
    NSR_UNCERTAIN_FLAG = 0x0004, // Indicates uncertain identification
} Masks;

// all pattern matching accelerator registers are 16-bits wide, memory-addressed
// define efficient inline register access
inline volatile uint16_t* regAddress(Registers reg)
{
    return (uint16_t*)(0xB0600000L + reg);
}

inline uint16_t regRead16(Registers reg) { return *regAddress(reg); }

inline void regWrite16(Registers reg, uint16_t value)
{
    *regAddress(reg) = value;
}
#if 0
inline void regWrite16 (Registers reg, uint8_t value)
{
	*regAddress(reg) = value;
}

inline void regWrite16 (Registers reg, int value)
{
	*regAddress(reg) = value;
}
#endif
#endif
