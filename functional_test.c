#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "cpu.h"

// the 6502 has 256 byte pages

/*
 * Starting out by seeing if I can get the functional test to pass from https://github.com/Klaus2m5/6502_65C02_functional_tests
 * (file name: 6502_functional_test.bin)
 *
 * This lst is an important file to compare my output to:
 * https://github.com/Klaus2m5/6502_65C02_functional_tests/blob/master/bin_files/6502_functional_test.lst
 *
 * Based on recommendation from https://www.reddit.com/r/EmuDev/comments/9s755i/is_there_a_comprehensive_nes_emulation_guide/e8qi80f/
 *
 * Addressing: http://obelisk.me.uk/6502/addressing.html#ZPG
 *
 * Status flags: NV1BDIZC
 *
 */

int main(int argc, char **argv) 
{
  printf("hi there\n");

  unsigned char buffer[262160];
  FILE *file;

  file = fopen("6502_functional_test.bin", "rb");
  fread(buffer, sizeof(buffer), 1, file);
  fclose(file);

  unsigned char *memory = buffer;
  unsigned char instr = 0;
  struct Computer state = { memory, 0, 0, 0, 0, 0, 0, 0, 0 };
  int instructionsExecuted = 0;
  int memoryAddressToStartAt = 0x0400;  // just for the test file

  printf("\n\nbegin execution:\n\n");
  for (state.pc = memoryAddressToStartAt; state.pc < 50000;)
  {
    int i = state.pc;
    int initialPc = state.pc;

    instr = buffer[i];
    executeInstruction(instr, &state);

    if (initialPc == state.pc) {
      printf("ERROR: did not move to a new instruction\n");
      printf("test number: %02x\n", memory[0x0200]);
      return(0);
    }

    if (state.pc == 0x336d)
    {
      printf("got to the beginning of the decimal mode tests, so I am calling this a success\n");
      printf("instruction count: %d\n", instructionsExecuted);
      return(0);
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



