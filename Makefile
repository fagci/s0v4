SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin

TARGET = $(BIN_DIR)/firmware

SRC = $(wildcard $(SRC_DIR)/driver/*.c)
SRC += $(wildcard $(SRC_DIR)/helper/*.c)
SRC += $(wildcard $(SRC_DIR)/ui/*.c)
SRC += $(wildcard $(SRC_DIR)/apps/*.c)
SRC += $(wildcard $(SRC_DIR)/*.c)

OBJS = $(OBJ_DIR)/start.o
OBJS += $(OBJ_DIR)/init.o
OBJS += $(OBJ_DIR)/external/printf/printf.o

OBJS += $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

BSP_DEFINITIONS := $(wildcard hardware/*/*.def)
BSP_HEADERS := $(patsubst hardware/%,inc/%,$(BSP_DEFINITIONS))
BSP_HEADERS := $(patsubst %.def,%.h,$(BSP_HEADERS))

AS = arm-none-eabi-gcc
CC = arm-none-eabi-gcc
LD = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

GIT_HASH := $(shell git rev-parse --short HEAD)
TS := $(shell date -u +'"%Y-%m-%d %H:%M UTC"')
TS_FILE := $(shell date -u +'"%Y%m%d_%H%M"')

ASFLAGS = -c -mcpu=cortex-m0
CFLAGS = -Os -Wall -Wno-error -mcpu=cortex-m0 -fno-builtin -fshort-enums -fno-delete-null-pointer-checks -Wno-error=incompatible-pointer-types -std=c2x -MMD -flto=auto -Wextra
CFLAGS += -DPRINTF_INCLUDE_CONFIG_H
CFLAGS += -DGIT_HASH=\"$(GIT_HASH)\"
CFLAGS += -DTIME_STAMP=\"$(TS)\"
CFLAGS += -DCMSIS_device_header=\"ARMCM0.h\"


CCFLAGS += -Wall -Werror -mcpu=cortex-m0 -fno-builtin -fshort-enums -fno-delete-null-pointer-checks -MMD -g
CCFLAGS += -ftree-vectorize -funroll-loops
CCFLAGS += -Wextra -Wno-unused-function -Wno-unused-variable -Wno-unknown-pragmas 
#-Wunused-parameter -Wconversion
CCFLAGS += -fno-math-errno -pipe -ffunction-sections -fdata-sections -ffast-math
CCFLAGS += -fsingle-precision-constant -finline-functions-called-once
CCFLAGS += -Os -g3 -fno-exceptions -fno-non-call-exceptions -fno-delete-null-pointer-checks
CCFLAGS += -DARMCM0


LDFLAGS = -mcpu=cortex-m0 -nostartfiles -Wl,-T,firmware.ld
# Use newlib-nano instead of newlib
LDFLAGS += --specs=nano.specs -lc -lnosys -mthumb -mabi=aapcs -lm -fno-rtti -fno-exceptions
LDFLAGS += -Wl,--build-id=none
LDFLAGS += -z noseparate-code -z noexecstack -mcpu=cortex-m0 -nostartfiles -Wl,-L,linker -Wl,--gc-sections
LDFLAGS += -Wl,--print-memory-usage
LDFLAGS += -Wl,-Map=./obj/output.map

INC =
INC += -I ./src/config
INC += -I ./src/external/CMSIS_5/CMSIS/Core/Include/
INC += -I ./src/external/CMSIS_5/Device/ARM/ARMCM0/Include
INC += -I ./src/external/mcufont/decoder/
INC += -I ./src/external/mcufont/fonts/

DEPS = $(OBJS:.o=.d)

.PHONY: all clean

all: $(TARGET)
	$(OBJCOPY) -O binary $< $<.bin
	-python3 fw-pack.py $<.bin $(GIT_HASH) $<.packed.bin

debug: CCFLAGS += -DDEBUG
debug: clean all

release: clean all
	cp $(BIN_DIR)/firmware.packed.bin $(BIN_DIR)/s0va-by-fagci-$(TS_FILE).bin

version.o: .FORCE

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(LD) $(LDFLAGS) $^ -o $@

inc/dp32g030/%.h: hardware/dp32g030/%.def

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(BSP_HEADERS) $(OBJ_DIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.S | $(OBJ_DIR)
	$(AS) $(ASFLAGS) $< -o $@

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

.FORCE:

-include $(DEPS)

clean:
	rm -f $(TARGET).bin $(TARGET).packed.bin $(TARGET) $(OBJ_DIR)/*.o $(OBJ_DIR)/*.d $(OBJ_DIR)/**/*.o $(OBJ_DIR)/**/**/*.o $(OBJ_DIR)/**/*.d
