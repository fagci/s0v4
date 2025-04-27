#ifndef SCAN_H
#define SCAN_H

#include "channels.h"
#include "lootlist.h"
#include <stdint.h>

extern uint32_t delay;
extern uint16_t sqLevel;

void SCAN_Init(bool multiband);
void SCAN_setStartF(uint32_t f);
void SCAN_setEndF(uint32_t f);
void SCAN_setBand(Band b);
void SCAN_Check(bool isAnalyserMode);
void SCAN_Next(bool up);
uint32_t SCAN_GetCps();

#endif /* end of include guard: SCAN_H */
