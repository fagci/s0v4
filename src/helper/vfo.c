#include "vfo.h"
#include "../driver/uart.h"
#include "../radio.h"
#include "channels.h"

static VfoListItem vfoScanlist[VFO_COUNT_MAX];
static uint8_t vfoScanlistSize = 0;

static uint16_t _scanlistMask = UINT16_MAX;

void VFO_LoadScanlist(uint16_t scanlistMask) {
  Log("Load VFO SL");
  vfoScanlistSize = 0;
  for (uint16_t i = 0; i < CHANNELS_GetCountMax(); ++i) {
    CHMeta meta = CHANNELS_GetMeta(i);

    bool isOurType = (TYPE_FILTER_VFO & (1 << meta.type)) != 0;
    if (!isOurType) {
      continue;
    }

    bool isOurScanlist = (isOurType && scanlistMask == SCANLIST_ALL) ||
                         (CHANNELS_Scanlists(i) & scanlistMask);
    if (isOurScanlist) {
      vfoScanlist[vfoScanlistSize].mr = i;
      CHANNELS_Load(i, &vfoScanlist[vfoScanlistSize].vfo);
      vfoScanlistSize++;
      Log("Load CH %u in VFO SL", i);
    }
  }

  if (gSettings.activeVFO >= vfoScanlistSize) {
    gSettings.activeVFO = vfoScanlistSize;
  }
  Log("SL sz: %u", vfoScanlistSize);
}

VFO VFO_GetNext(bool next) {
  return vfoScanlist[IncDecU(gSettings.activeVFO, 0, vfoScanlistSize, next)]
      .vfo;
}

bool VFO_Next(bool next) {
  gSettings.activeVFO = IncDecU(gSettings.activeVFO, 0, vfoScanlistSize, next);
  radio = vfoScanlist[gSettings.activeVFO].vfo;
  Log("!!! Next VFO CH=%u, f=%u", vfoScanlist[gSettings.activeVFO].mr, radio);
  SETTINGS_Save();
  return true;
}

uint16_t VFO_GetCh(uint8_t n) {
  Log("CURRENT VFO CH IS: %u", vfoScanlist[n].mr);
  return vfoScanlist[n].mr;
}

void VFO_SaveCurrent() {
  if (gSettings.activeVFO >= 0) {
    CHANNELS_Save(vfoScanlist[gSettings.activeVFO].mr, &radio);
  }
}

uint8_t VFO_GetSize() { return vfoScanlistSize; }
