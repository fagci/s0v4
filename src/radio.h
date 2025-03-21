#ifndef RADIO_H
#define RADIO_H

#include "driver/bk4819.h"
#include "helper/lootlist.h"
#include "settings.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  TX_UNKNOWN,
  TX_ON,
  TX_VOL_HIGH,
  TX_BAT_LOW,
  TX_DISABLED,
  TX_DISABLED_UPCONVERTER,
  TX_POW_OVERDRIVE,
} TXState;

extern CH *radio;
extern CH gVFO[2];

extern Measurement gLoot[2];

extern bool gIsListening;
extern bool gMonitorMode;
extern TXState gTxState;
extern bool gShowAllRSSI;
extern uint8_t gCurrentTxPower;

extern const uint16_t StepFrequencyTable[15];
extern const char *modulationTypeOptions[8];
extern const SquelchType sqTypeValues[4];

extern const char *upConverterFreqNames[3];
extern const char *vfoStateNames[];
extern const char *powerNames[];
extern const char *radioNames[4];
extern const char *shortRadioNames[3];
extern const char *deviationNames[];
extern const char *sqTypeNames[4];
extern const char *TX_STATE_NAMES[7];
extern const char *bwNames[10];

const char *RADIO_GetBWName(const VFO *vfo);
Radio RADIO_GetRadio();
ModulationType RADIO_GetModulation();
void RADIO_Init();

void RADIO_SaveCurrentVFO();
void RADIO_SaveCurrentVFODelayed(void);
void RADIO_LoadCurrentVFO();

TXState RADIO_GetTXState(uint32_t txF);
void RADIO_ToggleRX(bool on);
void RADIO_ToggleTX(bool on);
void RADIO_ToggleTXEX(bool on, uint32_t txF, uint8_t power, bool paEnabled);

void RADIO_TuneToPure(uint32_t f, bool precise);
void RADIO_TuneTo(uint32_t f);
void RADIO_TuneToSave(uint32_t f);

bool RADIO_TuneToMR(int16_t num);
void RADIO_TuneToCH(int16_t num);
void RADIO_TuneToBand(int16_t num);

void RADIO_Setup();
void RADIO_SwitchRadioPure();

void RADIO_VfoLoadCH(uint8_t i);
void RADIO_SetupByCurrentVFO();
void RADIO_NextVFO(void);
void RADIO_NextF(bool inc);
void RADIO_ToggleVfoMR();

void RADIO_SetSquelch(uint8_t sq);
void RADIO_SetGain(uint8_t gainIndex);
void RADIO_ToggleModulationEx(bool next);
void RADIO_ToggleModulation();
void RADIO_ToggleListeningBW();
void RADIO_SetFilterBandwidth(BK4819_FilterBandwidth_t bw);
void RADIO_ToggleTxPower(void);
void RADIO_UpdateStep(bool inc);
void RADIO_UpdateSquelchLevel(bool next);
bool RADIO_IsSquelchOpen();

bool RADIO_IsSSB();
uint32_t GetScreenF(uint32_t f);
uint32_t GetTuneF(uint32_t f);
uint16_t RADIO_GetRSSI(void);
uint8_t RADIO_GetSNR(void);
uint16_t RADIO_GetS();
uint32_t RADIO_GetTXF(void);
uint32_t RADIO_GetTXFEx(const VFO *vfo);
uint32_t RADIO_GetTxPower(uint32_t txF);
void RADIO_ToggleBK1080(bool on);

bool RADIO_HasSi();
void RADIO_SendDTMF(const char *pattern, ...);
bool RADIO_IsChMode();

void RADIO_GetGainString(char *String, uint8_t i);

void RADIO_CheckAndListen();

#endif /* end of include guard: RADIO_H */
