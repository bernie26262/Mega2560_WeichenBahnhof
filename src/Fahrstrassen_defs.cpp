#include "Fahrstrassen.h"
#include "Fahrstrassen_defs.h"

// ==================================================
// Fahrstraßen – Weichen-Schaltfolgen
// ==================================================

// --------- FS0 ----------
static const WeichenSchaltSchritt FS0_STEPS[] =
{
    { 2, GERADE,   0 },
    { 3, ABBIEGEN, 0 },
    { 8, ABBIEGEN, 1 },
    { 8, GERADE,   2 }
};

// --------- FS1 ----------
static const WeichenSchaltSchritt FS1_STEPS[] =
{
    { 1, ABBIEGEN, 2 },
    { 3, GERADE,   2 }
};

// --------- FS2 ----------
static const WeichenSchaltSchritt FS2_STEPS[] =
{
    { 1, ABBIEGEN, 0 },
    { 7, GERADE,   0 }
};

// --------- FS3 ----------
static const WeichenSchaltSchritt FS3_STEPS[] =
{
    { 2, ABBIEGEN, 0 },
    { 3, GERADE,   0 }
};

// --------- FS4 ----------
static const WeichenSchaltSchritt FS4_STEPS[] =
{
    { 9, GERADE,   1 },
    { 9, ABBIEGEN, 3 }
};

// ==================================================
// ZENTRALE Fahrstraßen-Definition (GENAU EINMAL)
// ==================================================
const SteuerungWeichenDefinition STW_DEFS[NUM_STW_FS] =
{
    { 0, {3,6}, 2, FS0_STEPS, (uint8_t)(sizeof(FS0_STEPS)/sizeof(FS0_STEPS[0])) },
    { 2, { },   0, FS1_STEPS, (uint8_t)(sizeof(FS1_STEPS)/sizeof(FS1_STEPS[0])) },
    { 4, { },   0, FS2_STEPS, (uint8_t)(sizeof(FS2_STEPS)/sizeof(FS2_STEPS[0])) },
    { 7, { },   0, FS3_STEPS, (uint8_t)(sizeof(FS3_STEPS)/sizeof(FS3_STEPS[0])) },
    { 8, {10},  1, FS4_STEPS, (uint8_t)(sizeof(FS4_STEPS)/sizeof(FS4_STEPS[0])) }
};
