#ifndef VFO_H
#define VFO_H

#include "channels.h"
#include <stdint.h>

#define VFO_COUNT_MAX 16

typedef struct {
  uint16_t mr;
  VFO vfo;
} VfoListItem;

void VFO_LoadScanlist(uint16_t n);
bool VFO_Next(bool next);
void VFO_SaveCurrent();
uint16_t VFO_GetCh(uint8_t n);
VFO VFO_GetNext(bool next);
uint8_t VFO_GetSize();
VFO *VFO_Get(uint8_t n);
void VFO_Select(uint8_t index);

#endif /* end of include guard: VFO_H */
