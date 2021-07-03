#ifndef FILE_CPU_H_SEEN
#define FILE_CPU_H_SEEN

#include <stdbool.h>
#include <stdint.h>

struct PPUClosure;
struct KeyboardInput;

struct Computer 
{ 
  unsigned char *memory;

  // TODO: I don't like having this in the CPU
  // 8 kB each. Covering 0x8000 to 0xFFFF
  uint8_t *prgRomBlock1; 
  uint8_t *prgRomBlock2;
  uint8_t *prgRomBlock3;
  uint8_t *prgRomBlock4;

  unsigned int pc;

  unsigned char acc;
  unsigned char xRegister;
  unsigned char yRegister;
  unsigned char stackRegister;

  unsigned char negativeFlag;
  unsigned char overflowFlag;
  unsigned char decimalFlag;
  unsigned char interruptDisable;
  unsigned char zeroFlag;
  unsigned char carryFlag;

  bool irqPending;
  bool nmiPending;

  // TODO: this isn't just PPU stuff anymore. Mappers need to intercept memory writes, for example. Rename, maybe to
  // something about event handling. Or have a separate one for mapper/cartridge.
  struct PPUClosure *ppuClosure;
  unsigned int totalCyclesCompleted;

  // TODO: this is temporary
  uint8_t mmc1ShiftRegister;
  uint8_t mmc1PrgRomBank;
  uint8_t mmc1ShiftCounter;

  struct KeyboardInput *keyboardInput;

  // TODO: do we actually need pollController? Can we move these elsewhere?
  bool pollController;
  uint8_t currentButtonBit;
  uint8_t buttons;

  bool debuggingOn;
};

int executeInstruction(unsigned char instr, struct Computer *state);
void triggerIrqInterrupt(struct Computer *state);
void fireIrqInterrupt(struct Computer *state);
void triggerNmiInterrupt(struct Computer *state);
void fireNmiInterrupt(struct Computer *state);
unsigned char readMemory(unsigned int memoryAddress, struct Computer *state);
void justForTesting(void *videoBuffer);

#endif /* !FILE_CPU_H_SEEN */
