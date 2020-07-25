#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "cpu.h"

/*
 * I changed Klaus's 6502_interrupt_test.a65 to have zero_page 0, then assembled it into a bin file with
 * the Kingswood assembler on Windows (available as part of Klaus's test project).
 *
 * When the test stops early, it's helpful to look at the generated 6502_interrupt_test.lst file to
 * figure out what is being tested and why it stopped.
 *
 */

int main(int argc, char **argv) 
{
  printf("hi there\n");

  unsigned char buffer[262160];  // TODO: can't this just be 64k? Where did this number come from?
  FILE *file;

  file = fopen("6502_interrupt_test.bin", "rb");
  fread(buffer, sizeof(buffer), 1, file);
  fclose(file);

  unsigned char *memory = buffer;
  unsigned char instr = 0;

  struct Computer state = { memory, 0, 0, 0, 0, 0, 0, 0, 0 };

  int instructionsExecuted = 0;
  int memoryAddressToStartAt = 0x0400;  // just for the test file

  printf("\n\nbegin execution:\n\n");
  for (state.pc = memoryAddressToStartAt; state.pc < 50000;) {
    int i = state.pc;
    int initialPc = state.pc;

    instr = buffer[i];

    unsigned char oldIrqBit = state.memory[0xbffc] & 0x01;
    unsigned char oldNmiBit = state.memory[0xbffc] & 0x02;

    executeInstruction(instr, &state);

    if (state.pc == 0x06f5) {
      printf("SUCCESS!\n");
      return(0);
    }

    if (initialPc == state.pc) {
      printf("ERROR: did not move to a new instruction\n");
      printf("test number: %02x\n", memory[0x0200]);
      return(0);
    }

    unsigned char newIrqBit = state.memory[0xbffc] & 0x01;
    unsigned char newNmiBit = state.memory[0xbffc] & 0x02;
    if (oldNmiBit == 0 && newNmiBit == 0x02) {
      triggerNmiInterrupt(&state);
    } 
    if (oldIrqBit == 0 && newIrqBit == 1) {
      triggerIrqInterrupt(&state);
    }

    instructionsExecuted++;

    if (instructionsExecuted > 50000000) {
      printf("Stopping due to instruction limit.\n");
      printf("test number: %02x\n", memory[0x0200]);
      return(0);
    }
  }

  return(0);
}



