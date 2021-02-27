#ifndef FILE_EMU_H_SEEN
#define FILE_EMU_H_SEEN

#include <stdbool.h>
#include "cpu.h"
#include "ppu.h"

bool executeEmulatorCycle(struct Computer *state, struct PPU *ppu, void *videoBuffer, struct Color *palette);
void buildPPUClosure(struct PPUClosure *ppuClosure, struct PPU *ppu);

#endif /* !FILE_EMU_H_SEEN */
