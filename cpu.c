#include <stdio.h>
#include <stdlib.h>

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

void printState(unsigned char x, unsigned char y, unsigned char a, unsigned char z, unsigned char n, unsigned char c, unsigned char v, unsigned int pc, unsigned char s)
{
  printf("State: A=%02x X=%02x Y=%02x Z=%02x N=%02x C=%02x V=%02x PC=%x S=%02x", a, x, y, z, n, c, v, pc, s);
}

void setNegativeFlag(unsigned char val, unsigned char *negativeFlag)
{
  *negativeFlag = ((val & 0x80) != 0);
}

void setZeroFlag(unsigned char val, unsigned char *zeroFlag)
{
  *zeroFlag = (val == 0);
}

void pushToStack(unsigned char val, unsigned char *memory, unsigned char *stackRegister)
{
  printf("Push %02x to stack at position %02x\n", val, *stackRegister);
  memory[0x0100 + *stackRegister] = val;
  *stackRegister = *stackRegister - 1;
}

unsigned char popFromStack(unsigned char *memory, unsigned char *stackRegister)
{
  *stackRegister = *stackRegister + 1;
  unsigned char val = memory[0x0100 + *stackRegister];
  printf("Pop %02x from stack position %02x\n", val, *stackRegister);
  return val;
}

struct Computer 
{ 
  unsigned char *memory;
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
};

enum AddressingMode { Implicit, Immediate, ZeroPage, ZeroPageX, ZeroPageY, Relative, Absolute, AbsoluteX, AbsoluteY, Indirect, IndexedIndirect, IndirectIndexed };

void brk(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state) 
{
  state->pc++;
  unsigned int pcToPushToStack = state->pc + 1;
  pushToStack(pcToPushToStack >> 8, state->memory, &state->stackRegister);
  pushToStack(pcToPushToStack, state->memory, &state->stackRegister);

  // NV1BDIZC
  // extract into function?
  unsigned char processorStatus = (state->negativeFlag << 7) | (state->overflowFlag << 6) | (1 << 5) | (1 << 4)
    | (state->decimalFlag << 3) | (state->interruptDisable << 2) | (state->zeroFlag << 1) | (state->carryFlag);

  /*processorStatus = processorStatus | 0x10;  // set break command flag*/
  pushToStack(processorStatus, state->memory, &state->stackRegister);

  state->interruptDisable = 1;

  printf("%02x\n", instr);
  printf("-> BRK -- (force interrupt)\n");

  unsigned char lowNibble = (state->memory)[0xFFFE];
  unsigned char highNibble = (state->memory)[0xFFFF];

  state->pc = (highNibble << 8) | lowNibble;
}

// TODO: make getOperandValue use this
int getMemoryAddress(unsigned int *memoryAddress, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;

  if (addressingMode == Absolute)
  {
    length = 2;
    *memoryAddress = (state->memory[state->pc+2] << 8) | state->memory[state->pc+1];
  }
  else if (addressingMode == ZeroPage)
  {
    length = 1;
    *memoryAddress = state->memory[state->pc+1];
  }
  else if (addressingMode == ZeroPageX)
  {
    length = 1;
    unsigned char wrapAroundMemoryAddress = state->memory[state->pc+1] + state->xRegister;
    *memoryAddress = wrapAroundMemoryAddress;
  }
  else if (addressingMode == ZeroPageY)
  {
    length = 1;
    unsigned char wrapAroundMemoryAddress = state->memory[state->pc+1] + state->yRegister;
    *memoryAddress = wrapAroundMemoryAddress;
  }
  else if (addressingMode == AbsoluteX)
  {
    length = 2;
    *memoryAddress = (state->memory[state->pc+2] << 8) | state->memory[state->pc+1];
    *memoryAddress += state->xRegister;
  }
  else if (addressingMode == AbsoluteY)
  {
    length = 2;
    *memoryAddress = (state->memory[state->pc+2] << 8) | state->memory[state->pc+1];
    *memoryAddress += state->yRegister;
  }
  else
  {
    printf("ERROR: Need to implement a new addressing mode.\n");
  }

  return length;
}

int getOperandValue(unsigned char *value, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;

  if (addressingMode == Absolute)
  {
    length = 2;
    unsigned int memoryAddress = (state->memory[state->pc+2] << 8) | state->memory[state->pc+1];
    *value = state->memory[memoryAddress];
  }
  else if (addressingMode == Immediate)
  {
    length = 1;
    *value = state->memory[state->pc+1];
  }
  else if (addressingMode == ZeroPage || addressingMode == ZeroPageX || addressingMode == ZeroPageY || addressingMode == AbsoluteX || addressingMode == AbsoluteY)
  {
    unsigned int memoryAddress = 0;
    length = getMemoryAddress(&memoryAddress, addressingMode, state);
    *value = state->memory[memoryAddress];
  }
  else
  {
    printf("ERROR: Need to implement a new addressing mode.\n");
  }

  return length;
}

void printInstruction(unsigned char instr, int length, struct Computer *state)
{
  printf("%02x", instr);
  for (int i = 1; i <= length; i++)
  {
    printf(" %02x", state->memory[state->pc + i]);
  }
  printf("\n");
}

char *addressingModeString(enum AddressingMode addressingMode)
{
  switch(addressingMode)
  {
    case Implicit:
      return "Implicit";
    case Immediate:
      return "Immediate";
    case ZeroPage:
      return "ZeroPage";
    case ZeroPageX:
      return "ZeroPageX";
    case ZeroPageY:
      return "ZeroPageY";
    case Relative:
      return "Relative";
    case Absolute:
      return "Absolute";
    case AbsoluteX:
      return "AbsoluteX";
    case AbsoluteY:
      return "AbsoluteY";
    case Indirect: 
      return "Indirect";
    case IndexedIndirect: 
      return "IndexedIndirect";
    case IndirectIndexed:
      return "IndirectIndexed";
  }
}

void lda(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  unsigned char value = 0;

  int length = getOperandValue(&value, addressingMode, state);

  printInstruction(instr, length, state);
  printf("-> LDA; %s; Set acc to value %02x\n", addressingModeString(addressingMode), value);

  state->acc = value;
  setZeroFlag(state->acc, &state->zeroFlag);
  setNegativeFlag(state->acc, &state->negativeFlag);
  state->pc += (1 + length);
}

void ldx(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;
  unsigned char value = 0;

  length = getOperandValue(&value, addressingMode, state);

  printInstruction(instr, length, state);
  printf("-> LDX; %s; Set x to value %02x\n", addressingModeString(addressingMode), value);

  state->xRegister = value;
  setZeroFlag(state->xRegister, &state->zeroFlag);
  setNegativeFlag(state->xRegister, &state->negativeFlag);
  state->pc += (1 + length);
}

void ldy(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;
  unsigned char value = 0;

  length = getOperandValue(&value, addressingMode, state);

  printInstruction(instr, length, state);
  printf("-> LDY; %s; Set y to value %02x\n", addressingModeString(addressingMode), value);

  state->yRegister = value;
  setZeroFlag(state->yRegister, &state->zeroFlag);
  setNegativeFlag(state->yRegister, &state->negativeFlag);
  state->pc += (1 + length);
}

void sta(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  unsigned int memoryAddress = 0;
  int length = getMemoryAddress(&memoryAddress, addressingMode, state);

  printInstruction(instr, length, state);
  printf("-> STA; %s; Set memory address %x to acc value %02x\n", addressingModeString(addressingMode), memoryAddress, state->acc);

  state->memory[memoryAddress] = state->acc;
  state->pc += (1 + length);
}

void stx(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  unsigned int memoryAddress = 0;
  int length = getMemoryAddress(&memoryAddress, addressingMode, state);

  printInstruction(instr, length, state);
  printf("-> STX; %s; Set memory address %x to x value %02x\n", addressingModeString(addressingMode), memoryAddress, state->xRegister);

  state->memory[memoryAddress] = state->xRegister;
  state->pc += (1 + length);
}

void sty(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  unsigned int memoryAddress = 0;
  int length = getMemoryAddress(&memoryAddress, addressingMode, state);

  printInstruction(instr, length, state);
  printf("-> STY; %s; Set memory address %x to y value %02x\n", addressingModeString(addressingMode), memoryAddress, state->yRegister);

  state->memory[memoryAddress] = state->yRegister;
  state->pc += (1 + length);
}

void sec(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;
  printInstruction(instr, length, state);
  printf("-> SEC; %s; Set carry flag to 1\n", addressingModeString(addressingMode));

  state->carryFlag = 1;
  state->pc += (1 + length);
}

void cli(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;
  printInstruction(instr, length, state);
  printf("-> CLI; %s; Set interrupt disable flag to 0\n", addressingModeString(addressingMode));

  state->interruptDisable = 0;
  state->pc += (1 + length);
}

void sei(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;
  printInstruction(instr, length, state);
  printf("-> SEI; %s; Set interrupt disable flag to 1\n", addressingModeString(addressingMode));

  state->interruptDisable = 1;
  state->pc += (1 + length);
}

void sed(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;
  printInstruction(instr, length, state);
  printf("-> SED; %s; Set decimal flag to 1\n", addressingModeString(addressingMode));

  state->decimalFlag = 1;
  state->pc += (1 + length);
}

void clv(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;
  printInstruction(instr, length, state);
  printf("-> CLV; %s; Clear overflow flag\n", addressingModeString(addressingMode));

  state->overflowFlag = 0;
  state->pc += (1 + length);
}

void cmp(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;
  unsigned char value = 0;

  length = getOperandValue(&value, addressingMode, state);

  printInstruction(instr, length, state);
  printf("-> CMP; %s; compare value %x to acc %x\n", addressingModeString(addressingMode), value, state->acc);

  unsigned char result = state->acc - value;
  setZeroFlag(result, &state->zeroFlag);
  state->carryFlag = (state->acc >= value);
  setNegativeFlag(result, &state->negativeFlag);

  state->pc += (1 + length);
}

void cpx(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;
  unsigned char value = 0;

  length = getOperandValue(&value, addressingMode, state);

  printInstruction(instr, length, state);
  printf("-> CPX; %s; compare value %x to x value %x\n", addressingModeString(addressingMode), value, state->xRegister);

  unsigned char result = state->xRegister - value;
  setZeroFlag(result, &state->zeroFlag);
  state->carryFlag = (state->xRegister >= value);
  setNegativeFlag(result, &state->negativeFlag);

  state->pc += (1 + length);
}

void cpy(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;
  unsigned char value = 0;

  length = getOperandValue(&value, addressingMode, state);

  printInstruction(instr, length, state);
  printf("-> CPY; %s; compare value %x to y value %x\n", addressingModeString(addressingMode), value, state->yRegister);

  unsigned char result = state->yRegister - value;
  setZeroFlag(result, &state->zeroFlag);
  state->carryFlag = (state->yRegister >= value);
  setNegativeFlag(result, &state->negativeFlag);

  state->pc += (1 + length);
}

int main(int argc, char **argv) 
{
  printf("hi there\n");

  // instruction table
  void (*instructions[256])(unsigned char, enum AddressingMode, struct Computer *) = {
//  0     1     2     3      4     5     6     7     8     9     A     B     C     D     E     F 
    &brk, 0,    0,    0,     0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0, // 0
    0,    0,    0,    0,     0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0, // 1
    0,    0,    0,    0,     0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0, // 2
    0,    0,    0,    0,     0,    0,    0,    0, &sec,    0,    0,    0,    0,    0,    0,    0, // 3
    0,    0,    0,    0,     0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0, // 4
    0,    0,    0,    0,     0,    0,    0,    0, &cli,    0,    0,    0,    0,    0,    0,    0, // 5
    0,    0,    0,    0,     0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0, // 6
    0,    0,    0,    0,     0,    0,    0,    0, &sei,    0,    0,    0,    0,    0,    0,    0, // 7
    0,    0,    0,    0,  &sty, &sta, &stx,    0,    0,    0,    0,    0, &sty, &sta, &stx,    0, // 8
    0,    0,    0,    0,  &sty, &sta, &stx,    0,    0, &sta,    0,    0,    0, &sta,    0,    0, // 9
    &ldy, 0, &ldx,    0,  &ldy, &lda, &ldx,    0,    0, &lda,    0,    0, &ldy, &lda, &ldx,    0, // A
    0,    0,    0,    0,  &ldy, &lda, &ldx,    0, &clv, &lda,    0,    0, &ldy, &lda, &ldx,    0, // B
    &cpy, 0,    0,    0,  &cpy, &cmp,    0,    0,    0, &cmp,    0,    0, &cpy, &cmp,    0,    0, // C
    0,    0,    0,    0,     0, &cmp,    0,    0,    0, &cmp,    0,    0,    0, &cmp,    0,    0, // D
    &cpx, 0,    0,    0,  &cpx,    0,    0,    0,    0,    0,    0,    0, &cpx,    0,    0,    0, // E
    0,    0,    0,    0,     0,    0,    0,    0, &sed,    0,    0,    0,    0,    0,    0,    0 //  F
  };

  enum AddressingMode addressingModes[256] = {
    //  0            1                2                3                4                5                6                7                8                9                A                B                C                D                E                F 
    Implicit,        0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0, // 0
    0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0, // 1
    0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0, // 2
    0,               0,               0,               0,               0,               0,               0,               0,               Implicit,        0,               0,               0,               0,               0,               0,               0, // 3
    0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0, // 4
    0,               0,               0,               0,               0,               0,               0,               0,               Implicit,        0,               0,               0,               0,               0,               0,               0, // 5
    0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0,               0, // 6
    0,               0,               0,               0,               0,               0,               0,               0,               Implicit,        0,               0,               0,               0,               0,               0,               0, // 7
    0,               0,               0,               0,        ZeroPage,               ZeroPage,        ZeroPage,        0,               0,               0,               0,               0,        Absolute,               Absolute,        Absolute,        0, // 8
    0,               0,               0,               0,       ZeroPageX,               ZeroPageX,       ZeroPageY,       0,               0,               AbsoluteY,       0,               0,               0,               AbsoluteX,       0,               0, // 9
    Immediate,       0,       Immediate,               0,        ZeroPage,               ZeroPage,        ZeroPage,        0,               0,               Immediate,       0,               0,        Absolute,               Absolute,        Absolute,        0, // A
    0,               0,               0,               0,       ZeroPageX,               ZeroPageX,       ZeroPageY,       0,               Implicit,        AbsoluteY,       0,               0,       AbsoluteX,               AbsoluteX,       AbsoluteY,       0, // B
    Immediate,       0,               0,               0,        ZeroPage,               ZeroPage,        0,               0,               0,               Immediate,       0,               0,        Absolute,               Absolute,        0,               0, // C
    0,               0,               0,               0,               0,               ZeroPageX,       0,               0,               0,               AbsoluteY,       0,               0,               0,               AbsoluteX,       0,               0, // D
    Immediate,       0,               0,               0,        ZeroPage,               0,               0,               0,               0,               0,               0,               0,        Absolute,               0,               0,               0, // E
    0,               0,               0,               0,               0,               0,               0,               0,               Implicit,        0,               0,               0,               0,               0,               0,               0 //  F
  };

  unsigned char buffer[262160];
  FILE *file;

  file = fopen("6502_functional_test.bin", "rb");

  fread(buffer, sizeof(buffer), 1, file);

  unsigned char *memory = buffer;
  /*unsigned char *memory;*/
  /*memory = (unsigned char *) malloc(0xFFFF);*/

  printf("memory 0200: %02x\n", memory[0x0200]);
  printf("memory 0400: %02x\n", memory[0x0400]);

  unsigned char instr = 0;

  struct Computer state = { memory, 0, 0, 0, 0, 0, 0, 0, 0 };

  int instructionsExecuted = 0;

  int memoryAddressToStartAt = 0x0400;  // just for the test file

  printf("\n\nexecution:\n");
  for (state.pc = memoryAddressToStartAt; state.pc < 50000;)
  {
    int i = state.pc;
    int initialPc = state.pc;

    instr = buffer[i];

    printf("PC: %04x\n", state.pc);

    if (instructions[instr] != 0)
    {
      instructions[instr](instr, addressingModes[instr], &state);
    }
    // BNE; Branch on not equal; Len 2
    else if (instr == 0xD0)
    {
      signed char relativeDisplacement = buffer[i+1];
      printf("%02x %02x\n", instr, buffer[i+1]);
      if (state.zeroFlag == 0) {
        printf("-> BNE %02x -> jump by %d\n", relativeDisplacement, relativeDisplacement);
        state.pc += relativeDisplacement;
      } else {
        printf("-> BNE %02x -> did not jump by %d\n", relativeDisplacement, relativeDisplacement);
      }

      state.pc += 2;
    }
    // BEQ; Branch if equal; Len 2
    else if (instr == 0xF0)
    {
      signed char relativeDisplacement = buffer[i+1];
      printf("%02x %02x\n", instr, buffer[i+1]);
      if (state.zeroFlag == 1) {
        printf("-> BEQ %02x -> jump by %d\n", relativeDisplacement, relativeDisplacement);
        state.pc += relativeDisplacement;
      } else {
        printf("-> BEQ %02x -> did not jump by %d\n", relativeDisplacement, relativeDisplacement);
      }

      state.pc += 2;
    }
    // BCC; Branch if carry clear; Len 2
    else if (instr == 0x90)
    {
      signed char relativeDisplacement = buffer[i+1];
      printf("%02x %02x\n", instr, buffer[i+1]);
      if (state.carryFlag == 0) {
        printf("-> BCC %02x -> carry flag is zero, so jump by %d\n", relativeDisplacement, relativeDisplacement);
        state.pc += relativeDisplacement;
      } else {
        printf("-> BCC %02x -> carry flag is not zero, so did not jump by %d\n", relativeDisplacement, relativeDisplacement);
      }

      state.pc += 2;
    }
    // BCS; Branch if carry set; Len 2
    else if (instr == 0xB0)
    {
      signed char relativeDisplacement = buffer[i+1];
      printf("%02x %02x\n", instr, buffer[i+1]);
      if (state.carryFlag == 1) {
        printf("-> BCS %02x -> carry flag is set, so jump by %d\n", relativeDisplacement, relativeDisplacement);
        state.pc += relativeDisplacement;
      } else {
        printf("-> BCS %02x -> carry flag is not set, so did not jump by %d\n", relativeDisplacement, relativeDisplacement);
      }

      state.pc += 2;
    }
    // JMP; Jump; Absolute; Len 3, Time 3
    else if (instr == 0x4C)
    {
      unsigned int memoryAddress = (buffer[i+2] << 8) | buffer[i+1];
      printf("%02x %02x %02x\n", instr, buffer[i+1], buffer[i+2]);
      printf("-> JMP to memory address: %x\n", memoryAddress);
      state.pc = memoryAddress;
    }
    // JMP; Jump; Indirect; Len 3, Time 5
    else if (instr == 0x6C)
    {
      unsigned int memoryAddress1 = (buffer[i+2] << 8) | buffer[i+1];
      unsigned int memoryAddress2 = ((buffer[i+2] << 8) | buffer[i+1]) + 1;
      unsigned char lowNybble = memory[memoryAddress1];
      unsigned char highNybble = memory[memoryAddress2];
      unsigned int memoryAddress = (highNybble << 8) | lowNybble;
      printf("%02x %02x %02x\n", instr, buffer[i+1], buffer[i+2]);
      printf("-> JMP to indirect memory address: %x\n", memoryAddress);
      state.pc = memoryAddress;
    }
    // JSR; Jump to subroutine; Absolute; Len 3; Time 6
    else if (instr == 0x20)
    {
      unsigned int memoryAddress = (buffer[i+2] << 8) | buffer[i+1];

      unsigned int pcToPutInStack = (state.pc + 3) - 1;
      pushToStack(pcToPutInStack >> 8, memory, &state.stackRegister);
      pushToStack(pcToPutInStack, memory, &state.stackRegister);

      printf("%02x %02x %02x\n", instr, buffer[i+1], buffer[i+2]);
      printf("-> JSR - jump to subroutine: %x\n", memoryAddress);
      state.pc = memoryAddress;
    }
    // RTS; Len 1; Time 6
    else if (instr == 0x60)
    {
      unsigned char lowNibble = popFromStack(memory, &state.stackRegister);
      unsigned char highNibble = popFromStack(memory, &state.stackRegister);

      unsigned int memoryAddress = (highNibble << 8) | lowNibble;

      printf("%02x\n", instr);
      printf("-> RTS - return from subroutine: %x\n", memoryAddress);
      state.pc = memoryAddress;
      state.pc++;
    }
    // PHA; Push acc value to the stack; Len 1; Time 3
    else if (instr == 0x48)
    {
      printf("%02x\n", instr);
      printf("-> PHA (push acc value %02x to stack at position %02x)\n", state.acc, state.stackRegister);

      pushToStack(state.acc, memory, &state.stackRegister);

      state.pc++;
    }
    // PLA; Pull from stack and into acc; Len 1; Time 4
    else if (instr == 0x68)
    {
      state.acc = popFromStack(memory, &state.stackRegister);
      
      printf("%02x\n", instr);
      printf("-> PLA (pull from stack into acc -> %02x)\n", state.acc);

      state.zeroFlag = (state.acc == 0);
      setNegativeFlag(state.acc, &state.negativeFlag);

      state.pc++;
    }
    // PLP; Pull processor status; Len 1; Time 4
    else if (instr == 0x28)
    {
      unsigned char value = popFromStack(memory, &state.stackRegister);

      // NV1BDIZC
      state.carryFlag = (value & 0x01) != 0;
      state.zeroFlag = (value & 0x02) != 0;
      state.interruptDisable = (value & 0x04) != 0;
      state.decimalFlag = (value & 0x08) != 0;
      state.overflowFlag = (value & 0x40) != 0;
      state.negativeFlag = (value & 0x80) != 0;

      printf("%02x\n", instr);
      printf("-> PLP (pull from stack and into status flags)\n");

      state.pc++;
    }
    // PHP; Push processor status; Len 1; Time 3
    else if (instr == 0x08)
    {
      // NV1BDIZC
      unsigned char value = (state.negativeFlag << 7) | (state.overflowFlag << 6) | (1 << 5) | (1 << 4)
        | (state.decimalFlag << 3) | (state.interruptDisable << 2) | (state.zeroFlag << 1) | (state.carryFlag);

      printf("%02x\n", instr);
      printf("-> PHP (push status flags %02x to stack)\n", value);
      pushToStack(value, memory, &state.stackRegister);

      state.pc++;
    }
    // CLD; Clear decimal flag; Len 1; Time 2
    else if (instr == 0xD8)
    {
      printf("%02x\n", instr);
      printf("-> CLD (clear decimal flag)\n");
      state.decimalFlag = 0;
      state.pc++;
    } 
    // TXS; Transfer X to Stack Pointer; Len 1; Time 2
    else if (instr == 0x9A)
    {
      state.stackRegister = state.xRegister;
      printf("%02x\n", instr);
      printf("-> TXS -- (transfer %x to stack reg)\n", state.xRegister);
      state.pc++;
    } 
    // TSX; Transfer Stack Pointer to X; Len 1; Time 2
    else if (instr == 0xBA)
    {
      state.xRegister = state.stackRegister;
      state.zeroFlag = (state.xRegister == 0);
      setNegativeFlag(state.xRegister, &state.negativeFlag);
      printf("%02x\n", instr);
      printf("-> TSX -- (transfer stack register %x to x reg)\n", state.stackRegister);
      state.pc++;
    } 
    // TAY; Transfer Acc to Y; Len 1; Time 2
    else if (instr == 0xA8)
    {
      state.yRegister = state.acc;
      state.zeroFlag = (state.yRegister == 0);
      setNegativeFlag(state.yRegister, &state.negativeFlag);
      printf("%02x\n", instr);
      printf("-> TAY -- (transfer acc %x to y reg)\n", state.acc);
      state.pc++;
    } 
    // CPY; Compare y with another value; immediate; Len 2; Time 2
    else if (instr == 0xC0)
    {
      int operand = buffer[i+1];
      unsigned char result = state.yRegister - operand;
      state.zeroFlag = (result == 0);
      state.carryFlag = (state.yRegister >= operand);
      setNegativeFlag(result, &state.negativeFlag);
      printf("%02x %02x\n", instr, buffer[i+1]);
      printf("-> CPY #%02x (compare value %x to y value %x)\n", operand, operand, state.yRegister);
      state.pc += 2;
    }
    // DEY; Decrement Y register; Len 1; Time 2
    else if (instr == 0x88)
    {
      state.yRegister = state.yRegister - 1;
      state.zeroFlag = (state.yRegister == 0);
      setNegativeFlag(state.yRegister, &state.negativeFlag);
      printf("%02x\n", instr);
      printf("-> DEY -- (decrement Y by 1 to become value %x)\n", state.yRegister);
      state.pc++;
    }
    // TYA; Transfer Y to acc; Len 1; Time 2
    else if (instr == 0x98)
    {
      state.acc = state.yRegister;
      state.zeroFlag = (state.acc == 0);
      setNegativeFlag(state.acc, &state.negativeFlag);
      printf("%02x\n", instr);
      printf("-> TYA -- (transfer Y to acc, so acc becomes %x)\n", state.acc);
      state.pc++;
    }
    // TAX; Transfer acc to X; Len 1; Time 2 
    else if (instr == 0xAA)
    {
      state.xRegister = state.acc;
      state.zeroFlag = (state.xRegister == 0);
      setNegativeFlag(state.xRegister, &state.negativeFlag);
      printf("%02x\n", instr);
      printf("-> TAX -- (transfer acc to X, so X becomes %x)\n", state.xRegister);
      state.pc++;
    }
    // TXA; Transfer X to acc; Len 1; Time 2 
    else if (instr == 0x8A)
    {
      state.acc = state.xRegister;
      state.zeroFlag = (state.acc == 0);
      setNegativeFlag(state.acc, &state.negativeFlag);
      printf("%02x\n", instr);
      printf("-> TXA -- (transfer X to acc, so acc becomes %x)\n", state.acc);
      state.pc++;
    }
    // BPL; Branch on result plus; Len 2
    else if (instr == 0x10)
    {
      signed char relativeDisplacement = buffer[i+1];
      printf("%02x %02x\n", instr, buffer[i+1]);

      if (state.negativeFlag == 0) 
      {
        // branch
        printf("-> BPL %02x -- (negative flag is zero, so jump by %02x)\n", buffer[i+1], relativeDisplacement);
        state.pc += relativeDisplacement;
      } 
      else 
      {
        // no branch
        printf("-> BPL %02x -- (negative flag is not zero, so do not jump)\n", buffer[i+1]);
      }

      state.pc += 2;
    }
    // BMI; Branch if negative flag set; Len 2
    else if (instr == 0x30)
    {
      signed char relativeDisplacement = buffer[i+1];
      printf("%02x %02x\n", instr, buffer[i+1]);

      if (state.negativeFlag == 1) 
      {
        // branch
        printf("-> BMI %02x -- (negative flag is one, so jump by %02x)\n", buffer[i+1], relativeDisplacement);
        state.pc += relativeDisplacement;
      } 
      else 
      {
        // no branch
        printf("-> BMI %02x -- (negative flag is not one, so do not jump)\n", buffer[i+1]);
      }

      state.pc += 2;
    }
    // BVC; Branch if overflow clear; Len 2
    else if (instr == 0x50)
    {
      signed char relativeDisplacement = buffer[i+1];
      printf("%02x %02x\n", instr, buffer[i+1]);

      if (state.overflowFlag == 0) 
      {
        // branch
        printf("-> BVC %02x -- (overflow flag is clear, so jump by %02x)\n", buffer[i+1], relativeDisplacement);
        state.pc += relativeDisplacement;
      } 
      else 
      {
        // no branch
        printf("-> BVC %02x -- (overflow flag is not clear, so do not jump)\n", buffer[i+1]);
      }

      state.pc += 2;
    }
    // BVS; Branch if overflow set; Len 2
    else if (instr == 0x70)
    {
      signed char relativeDisplacement = buffer[i+1];
      printf("%02x %02x\n", instr, buffer[i+1]);

      if (state.overflowFlag == 1) 
      {
        // branch
        printf("-> BVS %02x -- (overflow flag is set, so jump by %02x)\n", buffer[i+1], relativeDisplacement);
        state.pc += relativeDisplacement;
      } 
      else 
      {
        // no branch
        printf("-> BVS %02x -- (overflow flag is not set, so do not jump)\n", buffer[i+1]);
      }

      state.pc += 2;
    }
    // ADC; Add memory to acc with carry; Len 2, Time 2
    // There are some test programs here I should try: http://www.6502.org/tutorials/vflag.html
    else if (instr == 0x69)
    {
      unsigned char operand = buffer[i+1];
      printf("%02x %02x\n", instr, operand);

      if (state.decimalFlag == 1)
      {
        // Apparently the NES doesn't support decimal mode.
      }
      else
      {
        unsigned int result = state.acc + operand + state.carryFlag;
        state.acc = result;
        printf("-> ADC #%02x -- (add %02x and %02x to the acc, resulting in %02x)\n", operand, operand, state.carryFlag, state.acc);
        state.zeroFlag = (state.acc == 0);
        setNegativeFlag(state.acc, &state.negativeFlag);
        state.carryFlag = (result > 255);

        // TODO: Do this a better way. See // http://www.righto.com/2012/12/the-6502-overflow-flag-explained.html
        signed char signedAcc = state.acc;
        signed char signedOperand = operand;
        signed int signedResult = signedAcc + signedOperand;
        state.overflowFlag = (signedResult > 127 || signedResult < -128);
      }

      state.pc += 2;
    }
    // EOR; Len 2; Time 2
    else if (instr == 0x49)
    {
      unsigned char operand = buffer[i+1];
      state.acc = state.acc ^ operand;
      state.zeroFlag = (state.acc == 0);
      setNegativeFlag(state.acc, &state.negativeFlag);
      printf("%02x\n", instr);
      printf("-> EOR #%02x -- (exclusive or between acc and %02x)\n", operand, operand);
      state.pc += 2;
    }
    // ORA; Logical Inclusive OR; immediate; Len 2; Time 2
    else if (instr == 0x09)
    {
      unsigned char operand = buffer[i+1];
      state.acc = state.acc | operand;
      state.zeroFlag = (state.acc == 0);
      setNegativeFlag(state.acc, &state.negativeFlag);
      printf("%02x\n", instr);
      printf("-> ORA #%02x -- (inclusive or between acc and %02x)\n", operand, operand);
      state.pc += 2;
    }
    // DEX; Len 1; Time 2
    else if (instr == 0xCA)
    {
      state.xRegister = state.xRegister - 1;
      state.zeroFlag = (state.xRegister == 0);
      setNegativeFlag(state.xRegister, &state.negativeFlag);
      printf("%02x\n", instr);
      printf("-> DEX -- (decrement X by 1 -> results in %02x)\n", state.xRegister);
      state.pc++;
    }
    // NOP; Len 1; Time 2
    else if (instr == 0xEA)
    {
      printf("%02x\n", instr);
      printf("-> NOP -- (do nothing)\n");
      state.pc++;
    }
    // CLC; Len 1; Time 2
    else if (instr == 0x18)
    {
      printf("%02x\n", instr);
      printf("-> CLC -- (clear carry flag)\n");
      state.carryFlag = 0;
      state.pc++;
    }
    // INX; Increment X; Len 1; Time 2
    else if (instr == 0xE8)
    {
      state.xRegister += 1;
      state.zeroFlag = (state.xRegister == 0);
      setNegativeFlag(state.xRegister, &state.negativeFlag);

      printf("%02x\n", instr);
      printf("-> INX -- (increment x to be %02x)\n", state.xRegister);

      state.pc++;
    }
    // INY; Increment Y; Len 1; Time 2
    else if (instr == 0xC8)
    {
      state.yRegister += 1;
      state.zeroFlag = (state.yRegister == 0);
      setNegativeFlag(state.yRegister, &state.negativeFlag);

      printf("%02x\n", instr);
      printf("-> INY -- (increment y to be %02x)\n", state.yRegister);

      state.pc++;
    }
    // RTI; Len 1; Time 6
    else if (instr == 0x40)
    {
      unsigned char processorFlags = popFromStack(memory, &state.stackRegister);
      unsigned char pcLowNibble = popFromStack(memory, &state.stackRegister);
      unsigned char pcHighNibble = popFromStack(memory, &state.stackRegister);

      state.carryFlag = (processorFlags & 0x01) != 0;
      state.zeroFlag = (processorFlags & 0x02) != 0;
      state.interruptDisable = (processorFlags & 0x04) != 0;
      state.decimalFlag = (processorFlags & 0x08) != 0;
      state.overflowFlag = (processorFlags & 0x40) != 0;
      state.negativeFlag = (processorFlags & 0x80) != 0;

      /*state.interruptDisable = 0;*/

      state.pc = (pcHighNibble << 8) | pcLowNibble;
    }
    else {
      printf("unknown instruction: %x\n", instr);
      printf("test number: %02x\n", memory[0x0200]);
      return(0);
    }

    printState(state.xRegister, state.yRegister, state.acc, state.zeroFlag, state.negativeFlag, state.carryFlag, state.overflowFlag, state.pc, state.stackRegister);
    printf("\nTop stack values: %02x %02x %02x %02x %02x\n", memory[0x01FF], memory[0x01FE], memory[0x01FD], memory[0x01FC], memory[0x01FB]);
    printf("\n\n");

    if (initialPc == state.pc) {
      printf("ERROR: did not move to a new instruction\n");
      printf("memory 0200: %02x\n", memory[0x0200]);
      return(0);
    }

    instructionsExecuted++;

    if (instructionsExecuted > 900000) {
      printf("Stopping due to instruction limit.\n");
      printf("memory 0200: %02x\n", memory[0x0200]);
      return(0);
    }
  }

  // file size: 262160
  // minus 16 byte header: 262144
  //
  // 131072 in PRG ROM
  // 131072 in CHR ROM
  // adds up to 262144!
  // so the file is made up of these two chunks

  // free(memory);

  fclose(file);

  return(0);
}


