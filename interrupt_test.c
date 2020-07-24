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
  //file = fopen("test.out", "rb");

  fread(buffer, sizeof(buffer), 1, file);

  unsigned char *memory = buffer;
  /*unsigned char *memory;*/
  /*memory = (unsigned char *) malloc(0xFFFF);*/

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

    executeInstruction(instr, &state);

    if (initialPc == state.pc) {
      printf("ERROR: did not move to a new instruction\n");
      printf("test number: %02x\n", memory[0x0200]);
      return(0);
    }

    unsigned char newIrqBit = state.memory[0xbffc] & 0x01;
    if (oldIrqBit == 0 && newIrqBit == 1) {
      fireIrqInterrupt(&state);
    }

    instructionsExecuted++;

    if (instructionsExecuted > 50000000) {
    //if (instructionsExecuted > 8) {
      printf("Stopping due to instruction limit.\n");
      printf("test number: %02x\n", memory[0x0200]);
      return(0);
    }
  }

  fclose(file);

  return(0);
}



