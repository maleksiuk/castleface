#include <stdio.h>
#include <stdarg.h>
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

// TODO: consider storing the status flags in a single byte

//#define PRINT_STATE 1
//#define PRINT_GAP 1
//#define PRINT_PC 1

void printState(unsigned char x, unsigned char y, unsigned char a, unsigned char z, unsigned char n, unsigned char c, unsigned char v, unsigned int pc, unsigned char s)
{
#ifdef PRINT_STATE
  printf("State: A=%02x X=%02x Y=%02x Z=%02x N=%02x C=%02x V=%02x PC=%x S=%02x", a, x, y, z, n, c, v, pc, s);
#endif
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
#ifdef PRINT_PUSH_TO_STACK
  printf("Push %02x to stack at position %02x\n", val, *stackRegister);
#endif
  memory[0x0100 + *stackRegister] = val;
  *stackRegister = *stackRegister - 1;
}

unsigned char popFromStack(unsigned char *memory, unsigned char *stackRegister)
{
  *stackRegister = *stackRegister + 1;
  unsigned char val = memory[0x0100 + *stackRegister];
#ifdef PRINT_POP_FROM_STACK
  printf("Pop %02x from stack position %02x\n", val, *stackRegister);
#endif
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

enum AddressingMode { Implicit, Immediate, ZeroPage, ZeroPageX, ZeroPageY, Relative, Absolute, AbsoluteX, AbsoluteY, Indirect, IndexedIndirect, IndirectIndexed, Accumulator };

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

int getMemoryAddress(unsigned int *memoryAddress, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;

  if (addressingMode == Immediate)
  {
    length = 1;
    *memoryAddress = state->pc+1;
  }
  else if (addressingMode == Absolute)
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
  else if (addressingMode == IndirectIndexed)
  {
    length = 1;
    unsigned char operand = state->memory[state->pc+1]; 
    *memoryAddress = (state->memory[operand+1] << 8) | state->memory[operand];
    *memoryAddress += state->yRegister;
  }
  else if (addressingMode == IndexedIndirect)
  {
    length = 1;
    unsigned char operand = state->memory[state->pc+1]; 
    unsigned char wrapAroundMemoryAddress = operand + state->xRegister;
    *memoryAddress = (state->memory[wrapAroundMemoryAddress+1] << 8) | state->memory[wrapAroundMemoryAddress];
  }
  else if (addressingMode == Indirect)
  {
    length = 2;
    unsigned int memoryAddress1 = (state->memory[state->pc+2] << 8) | state->memory[state->pc+1];
    unsigned int memoryAddress2 = ((state->memory[state->pc+2] << 8) | state->memory[state->pc+1]) + 1;
    unsigned char lowNybble = state->memory[memoryAddress1];
    unsigned char highNybble = state->memory[memoryAddress2];
    *memoryAddress = (highNybble << 8) | lowNybble;
  }
  else
  {
    printf("ERROR: Need to implement a new addressing mode.\n");
  }

#ifdef PRINT_OPERAND_MEMORY_ADDRESS
  printf("Memory address: %x\n", *memoryAddress);
#endif

  return length;
}

int getOperandValue(unsigned char *value, enum AddressingMode addressingMode, struct Computer *state)
{
  unsigned int memoryAddress = 0;
  int length = getMemoryAddress(&memoryAddress, addressingMode, state);
  *value = state->memory[memoryAddress];

  return length;
}

void printInstruction(unsigned char instr, int length, struct Computer *state)
{
#ifdef PRINT_INSTRUCTION
  printf("%02x", instr);
  for (int i = 1; i <= length; i++)
  {
    printf(" %02x", state->memory[state->pc + i]);
  }
  printf("\n");
#endif
}

char *addressingModeString(enum AddressingMode addressingMode)
{
  switch(addressingMode)
  {
    case Accumulator:
      return "Accumulator";
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

  return "Unknown";
}

void printInstructionDescription(char *name, enum AddressingMode addressingMode, char *desc, ...)
{
#ifdef PRINT_INSTRUCTION_DESCRIPTION
  va_list arguments;
  va_start(arguments, desc);

  char str[200];
  vsprintf(str, desc, arguments);
  va_end(arguments);

  printf("-> %s; %s; %s\n", name, addressingModeString(addressingMode), str);
#endif
}

void lda(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  unsigned char value = 0;

  int length = getOperandValue(&value, addressingMode, state);

  printInstruction(instr, length, state);
  printInstructionDescription("LDA", addressingMode, "set acc to value %02x", value);

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
  printInstructionDescription("LDX", addressingMode, "set x to value %02x", value);

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
  printInstructionDescription("LDY", addressingMode, "set y to value %02x", value);

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
  printInstructionDescription("STA", addressingMode, "set memory address %x to acc value %02x", memoryAddress, state->acc);

  state->memory[memoryAddress] = state->acc;
  state->pc += (1 + length);
}

void stx(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  unsigned int memoryAddress = 0;
  int length = getMemoryAddress(&memoryAddress, addressingMode, state);

  printInstruction(instr, length, state);
  printInstructionDescription("STX", addressingMode, "set memory address %x to x value %02x", memoryAddress, state->xRegister);

  state->memory[memoryAddress] = state->xRegister;
  state->pc += (1 + length);
}

void sty(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  unsigned int memoryAddress = 0;
  int length = getMemoryAddress(&memoryAddress, addressingMode, state);

  printInstruction(instr, length, state);
  printInstructionDescription("STY", addressingMode, "set memory address %x to y value %02x", memoryAddress, state->yRegister);

  state->memory[memoryAddress] = state->yRegister;
  state->pc += (1 + length);
}

void sec(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;
  printInstruction(instr, length, state);
  printInstructionDescription("SEC", addressingMode, "set carry flag to 1");

  state->carryFlag = 1;
  state->pc += (1 + length);
}

void cli(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;
  printInstruction(instr, length, state);
  printInstructionDescription("CLI", addressingMode, "set interrupt disable flag to 0");

  state->interruptDisable = 0;
  state->pc += (1 + length);
}

void sei(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;
  printInstruction(instr, length, state);
  printInstructionDescription("SEI", addressingMode, "set interrupt disable flag to 1");

  state->interruptDisable = 1;
  state->pc += (1 + length);
}

void sed(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;
  printInstruction(instr, length, state);
  printInstructionDescription("SED", addressingMode, "set decimal flag to 1");

  state->decimalFlag = 1;
  state->pc += (1 + length);
}

void clv(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;
  printInstruction(instr, length, state);
  printInstructionDescription("CLV", addressingMode, "clear overflow flag");

  state->overflowFlag = 0;
  state->pc += (1 + length);
}

void clc(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;
  printInstruction(instr, length, state);
  printInstructionDescription("CLC", addressingMode, "clear carry flag");

  state->carryFlag = 0;
  state->pc += (1 + length);
}

void cmp(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;
  unsigned char value = 0;

  length = getOperandValue(&value, addressingMode, state);

  printInstruction(instr, length, state);
  printInstructionDescription("CMP", addressingMode, "compare value %x to acc %x", value, state->acc);

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
  printInstructionDescription("CPX", addressingMode, "compare value %x to x value %x", value, state->xRegister);

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
  printInstructionDescription("CPY", addressingMode, "compare value %x to y value %x", value, state->yRegister);

  unsigned char result = state->yRegister - value;
  setZeroFlag(result, &state->zeroFlag);
  state->carryFlag = (state->yRegister >= value);
  setNegativeFlag(result, &state->negativeFlag);

  state->pc += (1 + length);
}

void bit(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;
  unsigned char value = 0;

  length = getOperandValue(&value, addressingMode, state);

  printInstruction(instr, length, state);
  printInstructionDescription("BIT", addressingMode, "AND acc %02x and value %02x", state->acc, value);

  unsigned char result = state->acc & value;

  setZeroFlag(result, &state->zeroFlag);
  state->overflowFlag = (value & 0x40) != 0;
  setNegativeFlag(value, &state->negativeFlag);

  state->pc += (1 + length);
}

// TODO: refactor
void asl(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;
  unsigned char value = 0;

  if (addressingMode == Accumulator)
  {
    length = 0;
    printInstruction(instr, length, state);
    printInstructionDescription("ASL", addressingMode, "arithmetic shift left of accumulator");

    unsigned int result = state->acc << 1;

    state->acc = result;
    state->carryFlag = (result > 255);
    setZeroFlag(result, &state->zeroFlag);
    setNegativeFlag(result, &state->negativeFlag);
  }
  else
  {
    unsigned int memoryAddress = 0;
    length = getMemoryAddress(&memoryAddress, addressingMode, state);
    unsigned char value = state->memory[memoryAddress];

    printInstruction(instr, length, state);
    printInstructionDescription("ASL", addressingMode, "arithmetic shift left of value", value);

    unsigned int bigResult = value << 1;
    unsigned char smallResult = bigResult;

    state->memory[memoryAddress] = smallResult;
    state->carryFlag = (bigResult > 255);
    setZeroFlag(smallResult, &state->zeroFlag);
    setNegativeFlag(smallResult, &state->negativeFlag);
  }

  state->pc += (1 + length);
}

// TODO: refactor
void rol(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;
  unsigned char value = 0;

  if (addressingMode == Accumulator)
  {
    length = 0;
    printInstruction(instr, length, state);
    printInstructionDescription("ROL", addressingMode, "rorate left of accumulator", value);

    unsigned char result = state->acc << 1;

    unsigned char oldCarryFlag = state->carryFlag;
    state->carryFlag = (state->acc & 0x80) != 0;  // save bit 7 into the carry flag
    state->acc = result;
    state->acc = state->acc | oldCarryFlag;  // make bit 0 have the value of the old carry flag
    setZeroFlag(state->acc, &state->zeroFlag);
    setNegativeFlag(state->acc, &state->negativeFlag);
  }
  else
  {
    unsigned int memoryAddress = 0;
    length = getMemoryAddress(&memoryAddress, addressingMode, state);
    unsigned char value = state->memory[memoryAddress];

    printInstruction(instr, length, state);
    printInstructionDescription("ROL", addressingMode, "rorate left of value %02x", value);

    unsigned char oldCarryFlag = state->carryFlag;
    unsigned char result = value << 1;
    result = result | oldCarryFlag;  // make bit 0 have the value of the old carry flag

    state->carryFlag = (value & 0x80) != 0;  // save bit 7 into the carry flag
    state->memory[memoryAddress] = result;
    setZeroFlag(result, &state->zeroFlag);
    setNegativeFlag(result, &state->negativeFlag);
  }

  state->pc += (1 + length);
}

void ror(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;

  unsigned char *value;

  if (addressingMode == Accumulator)
  {
    length = 0;
    value = &state->acc;
  }
  else
  {
    unsigned int memoryAddress = 0;
    length = getMemoryAddress(&memoryAddress, addressingMode, state);
    value = &state->memory[memoryAddress];
  }

  printInstruction(instr, length, state);
  printInstructionDescription("ROR", addressingMode, "rorate right of value %02x", *value);

  unsigned char oldBitZero = (*value & 0x01) != 0; 
  unsigned char result = *value >> 1;
  // make bit 7 have the value of the current carry flag
  if (state->carryFlag == 1)
  {
    result = result | 0x80;
  }
  else
  {
    result = result & 0x7F;
  }

  state->carryFlag = oldBitZero;
  *value = result;
  setZeroFlag(result, &state->zeroFlag);
  setNegativeFlag(result, &state->negativeFlag);

  state->pc += (1 + length);
}

void lsr(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;
  unsigned char *value;

  if (addressingMode == Accumulator)
  {
    length = 0;
    value = &state->acc;
  }
  else
  {
    unsigned int memoryAddress = 0;
    length = getMemoryAddress(&memoryAddress, addressingMode, state);
    value = &state->memory[memoryAddress];
  }

  printInstruction(instr, length, state);
  printInstructionDescription("LSR", addressingMode, "logical shift right of value %02x", *value);

  unsigned char oldBitZero = (*value & 0x01) != 0; 
  unsigned char result = *value >> 1;

  result = result & 0x7F;  // clear bit 7
  *value = result;
  state->carryFlag = oldBitZero;
  setZeroFlag(result, &state->zeroFlag);
  setNegativeFlag(result, &state->negativeFlag);

  state->pc += (1 + length);
}

void inc(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  unsigned int memoryAddress = 0;
  int length = getMemoryAddress(&memoryAddress, addressingMode, state);
  unsigned char *value = &state->memory[memoryAddress];

  *value = *value + 1;

  printInstruction(instr, length, state);
  printInstructionDescription("INC", addressingMode, "increment value at memory address %x to be %02x", memoryAddress, *value);

  setZeroFlag(*value, &state->zeroFlag);
  setNegativeFlag(*value, &state->negativeFlag);

  state->pc += (1 + length);
}

void dec(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  unsigned int memoryAddress = 0;
  int length = getMemoryAddress(&memoryAddress, addressingMode, state);
  unsigned char *value = &state->memory[memoryAddress];

  *value = *value - 1;

  printInstruction(instr, length, state);
  printInstructionDescription("DEC", addressingMode, "decrement value at memory address %x to be %02x", memoryAddress, *value);

  setZeroFlag(*value, &state->zeroFlag);
  setNegativeFlag(*value, &state->negativeFlag);

  state->pc += (1 + length);
}

void and(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  unsigned char value = 0;
  int length = getOperandValue(&value, addressingMode, state);

  printInstruction(instr, length, state);
  printInstructionDescription("AND", addressingMode, "AND acc value with %02x", value);

  state->acc = state->acc & value;

  setZeroFlag(state->acc, &state->zeroFlag);
  setNegativeFlag(state->acc, &state->negativeFlag);

  state->pc += (1 + length);
}

void eor(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  unsigned char value = 0;
  int length = getOperandValue(&value, addressingMode, state);

  printInstruction(instr, length, state);
  printInstructionDescription("EOR", addressingMode, "exclusive or between acc and %02x", value);

  state->acc = state->acc ^ value;

  setZeroFlag(state->acc, &state->zeroFlag);
  setNegativeFlag(state->acc, &state->negativeFlag);

  state->pc += (1 + length);
}

void ora(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  unsigned char value = 0;
  int length = getOperandValue(&value, addressingMode, state);

  printInstruction(instr, length, state);
  printInstructionDescription("ORA", addressingMode, "logical inclusive OR between acc and %02x", value);

  state->acc = state->acc | value;

  setZeroFlag(state->acc, &state->zeroFlag);
  setNegativeFlag(state->acc, &state->negativeFlag);

  state->pc += (1 + length);
}

void add(unsigned char value, struct Computer *state)
{
  unsigned char originalCarryFlag = state->carryFlag;
  unsigned int result = state->acc + value + originalCarryFlag;
  unsigned char overflowFlag = ((state->acc^result)&(value^result)&0x80) != 0;
  state->acc = result;
  setZeroFlag(state->acc, &state->zeroFlag);
  setNegativeFlag(state->acc, &state->negativeFlag);
  state->carryFlag = (result > 255);

  // See http://www.righto.com/2012/12/the-6502-overflow-flag-explained.html
  state->overflowFlag = overflowFlag;
}

void adc(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  unsigned char value = 0;
  int length = getOperandValue(&value, addressingMode, state);

  printInstruction(instr, length, state);
  printInstructionDescription("ADC", addressingMode, "add with carry: value %02x", value);

  if (state->decimalFlag == 1)
  {
    printf("Error: adc decimal mode not supported.\n");
  }
  else
  {
    add(value, state);
  }

  state->pc += (1 + length);
}

void sbc(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  unsigned char value = 0;
  int length = getOperandValue(&value, addressingMode, state);

  printInstruction(instr, length, state);
  printInstructionDescription("SBC", addressingMode, "subtract with carry: value %x", value);

  if (state->decimalFlag == 1)
  {
    printf("Error: sbc decimal mode not supported.\n");
  }
  else
  {
    add(value ^ 0xFF, state);
  }

  state->pc += (1 + length);
}

void jmp(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  unsigned int memoryAddress = 0;
  int length = getMemoryAddress(&memoryAddress, addressingMode, state);

  printInstruction(instr, length, state);
  printInstructionDescription("JMP", addressingMode, "jump to memory address %x", memoryAddress);

  state->pc = memoryAddress;
}

void inx(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;

  state->xRegister = state->xRegister + 1;
  setZeroFlag(state->xRegister, &state->zeroFlag);
  setNegativeFlag(state->xRegister, &state->negativeFlag);

  printInstruction(instr, length, state);
  printInstructionDescription("INX", addressingMode, "increment x register to become %x", state->xRegister);

  state->pc += (1 + length);
}

void iny(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;

  state->yRegister = state->yRegister + 1;
  setZeroFlag(state->yRegister, &state->zeroFlag);
  setNegativeFlag(state->yRegister, &state->negativeFlag);

  printInstruction(instr, length, state);
  printInstructionDescription("INY", addressingMode, "increment y register to become %x", state->yRegister);

  state->pc += (1 + length);
}

void dex(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;

  state->xRegister = state->xRegister - 1;
  setZeroFlag(state->xRegister, &state->zeroFlag);
  setNegativeFlag(state->xRegister, &state->negativeFlag);

  printInstruction(instr, length, state);
  printInstructionDescription("DEX", addressingMode, "decrement x register to become %x", state->xRegister);

  state->pc += (1 + length);
}

void dey(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;

  state->yRegister = state->yRegister - 1;
  setZeroFlag(state->yRegister, &state->zeroFlag);
  setNegativeFlag(state->yRegister, &state->negativeFlag);

  printInstruction(instr, length, state);
  printInstructionDescription("DEY", addressingMode, "decrement y register to become %x", state->yRegister);

  state->pc += (1 + length);
}

void nop(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;
  
  printInstruction(instr, length, state);
  printInstructionDescription("NOP", addressingMode, "do nothing");

  state->pc += (1 + length);
}

void php(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  // NV1BDIZC
  unsigned char value = (state->negativeFlag << 7) | (state->overflowFlag << 6) | (1 << 5) | (1 << 4)
    | (state->decimalFlag << 3) | (state->interruptDisable << 2) | (state->zeroFlag << 1) | (state->carryFlag);

  int length = 0;
  printInstruction(instr, length, state);
  printInstructionDescription("PHP", addressingMode, "push status flags %02x to stack", value);

  pushToStack(value, state->memory, &state->stackRegister);

  state->pc += (1 + length);
}

void plp(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  unsigned char value = popFromStack(state->memory, &state->stackRegister);

  // NV1BDIZC
  state->carryFlag = (value & 0x01) != 0;
  state->zeroFlag = (value & 0x02) != 0;
  state->interruptDisable = (value & 0x04) != 0;
  state->decimalFlag = (value & 0x08) != 0;
  state->overflowFlag = (value & 0x40) != 0;
  state->negativeFlag = (value & 0x80) != 0;

  int length = 0;
  printInstruction(instr, length, state);
  printInstructionDescription("PLP", addressingMode, "pull from stack and into status flags");

  state->pc += (1 + length);
}

void pla(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  state->acc = popFromStack(state->memory, &state->stackRegister);

  int length = 0;
  printInstruction(instr, length, state);
  printInstructionDescription("PLA", addressingMode, "pull from stack and into acc: %02x", state->acc);

  setZeroFlag(state->acc, &state->zeroFlag);
  setNegativeFlag(state->acc, &state->negativeFlag);

  state->pc += (1 + length);
}

void pha(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;
  printInstruction(instr, length, state);
  printInstructionDescription("PHA", addressingMode, "push acc value %02x to stack at position %02x", state->acc, state->stackRegister);

  pushToStack(state->acc, state->memory, &state->stackRegister);

  state->pc += (1 + length);
}

void bpl(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{  
  int length = 1;
  printInstruction(instr, length, state);

  if (state->negativeFlag == 0) 
  {
    // branch
    signed char relativeDisplacement = state->memory[state->pc+1];
    printInstructionDescription("BPL", addressingMode, "branch if the negative flag is zero; did branch by %x", relativeDisplacement);
    state->pc += relativeDisplacement;
  } 
  else 
  {
    // no branch
    printInstructionDescription("BPL", addressingMode, "branch if the negative flag is zero; did not branch");
  }

  state->pc += (1 + length);
}

void bne(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 1;
  printInstruction(instr, length, state);

  if (state->zeroFlag == 0)
  {
    signed char relativeDisplacement = state->memory[state->pc+1];
    printInstructionDescription("BNE", addressingMode, "branch if the zero flag is clear; did branch by %x", relativeDisplacement);
    state->pc += relativeDisplacement;
  }
  else
  {
    printInstructionDescription("BNE", addressingMode, "branch if the zero flag is clear; did not branch");
  }

  state->pc += (1 + length);
}

void jsr(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  unsigned int memoryAddress = 0;
  int length = getMemoryAddress(&memoryAddress, addressingMode, state);

  unsigned int pcToPutInStack = (state->pc + 3) - 1;
  pushToStack(pcToPutInStack >> 8, state->memory, &state->stackRegister);
  pushToStack(pcToPutInStack, state->memory, &state->stackRegister);

  printInstruction(instr, length, state);
  printInstructionDescription("JSR", addressingMode, "jump to subroutine: %x", memoryAddress);

  state->pc = memoryAddress;
}

void rts(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  unsigned char lowNibble = popFromStack(state->memory, &state->stackRegister);
  unsigned char highNibble = popFromStack(state->memory, &state->stackRegister);

  unsigned int memoryAddress = (highNibble << 8) | lowNibble;

  int length = 0;
  printInstruction(instr, length, state);
  printInstructionDescription("RTS", addressingMode, "return from subroutine: %x", memoryAddress);

  state->pc = memoryAddress;
  state->pc += (1 + length);
}

void bmi(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 1;
  signed char relativeDisplacement = state->memory[state->pc+1];
  printInstruction(instr, length, state);

  if (state->negativeFlag == 1) 
  {
    // branch
    printInstructionDescription("BMI", addressingMode, "negative flag is one, so jump by %02x", relativeDisplacement);
    state->pc += relativeDisplacement;
  } 
  else 
  {
    // no branch
    printInstructionDescription("BMI", addressingMode, "negative flag is not one, so do not jump");
  }

  state->pc += (1 + length);
}


int main(int argc, char **argv) 
{
  printf("hi there\n");

  // instruction table
  void (*instructions[256])(unsigned char, enum AddressingMode, struct Computer *) = {
//  0     1     2     3      4     5     6     7     8     9     A     B     C     D     E     F 
    &brk, &ora, 0,    0,     0, &ora,    &asl, 0, &php, &ora, &asl,    0,    0, &ora, &asl,    0, // 0
    &bpl, &ora, 0,    0,     0, &ora,    &asl, 0, &clc, &ora,    0,    0,    0, &ora, &asl,    0, // 1
    &jsr, &and, 0,    0,  &bit, &and,    &rol, 0, &plp, &and, &rol,    0, &bit, &and, &rol,    0, // 2
    &bmi, &and, 0,    0,     0, &and,    &rol, 0, &sec, &and,    0,    0,    0, &and, &rol,    0, // 3
    0,    &eor, 0,    0,     0, &eor,    &lsr, 0, &pha, &eor, &lsr,    0, &jmp, &eor, &lsr,    0, // 4
    0,    &eor, 0,    0,     0, &eor,    &lsr, 0, &cli, &eor,    0,    0,    0, &eor, &lsr,    0, // 5
    &rts, &adc, 0,    0,     0, &adc,    &ror, 0, &pla, &adc, &ror,    0, &jmp, &adc, &ror,    0, // 6
    0,    &adc, 0,    0,     0, &adc,    &ror, 0, &sei, &adc,    0,    0,    0, &adc, &ror,    0, // 7
    0,    &sta, 0,    0,  &sty, &sta,    &stx, 0, &dey,    0,    0,    0, &sty, &sta, &stx,    0, // 8
    0,    &sta, 0,    0,  &sty, &sta,    &stx, 0,    0, &sta,    0,    0,    0, &sta,    0,    0, // 9
    &ldy, &lda, &ldx, 0,  &ldy, &lda,    &ldx, 0,    0, &lda,    0,    0, &ldy, &lda, &ldx,    0, // A
    0,    &lda, 0,    0,  &ldy, &lda,    &ldx, 0, &clv, &lda,    0,    0, &ldy, &lda, &ldx,    0, // B
    &cpy, &cmp, 0,    0,  &cpy, &cmp,    &dec, 0, &iny, &cmp, &dex,    0, &cpy, &cmp, &dec,    0, // C
    &bne, &cmp, 0,    0,     0, &cmp,    &dec, 0,    0, &cmp,    0,    0,    0, &cmp, &dec,    0, // D
    &cpx, &sbc, 0,    0,  &cpx, &sbc,    &inc, 0, &inx, &sbc, &nop,    0, &cpx, &sbc, &inc,    0, // E
    0,    &sbc, 0,    0,     0, &sbc,    &inc, 0, &sed, &sbc,    0,    0,    0, &sbc, &inc,    0  // F
  };

  enum AddressingMode addressingModes[256] = {
    //  0            1                2                3                4                5                6                7                8                9                A                B                C                D                E                F 
    Implicit,        IndexedIndirect, 0,               0,               0,               ZeroPage,        ZeroPage,        0,               Implicit,        Immediate,       Accumulator,     0,               0,               Absolute,        Absolute,        0, // 0
    Relative,        IndirectIndexed, 0,               0,               0,               ZeroPageX,       ZeroPageX,       0,               Implicit,        AbsoluteY,       0,               0,               0,               AbsoluteX,       AbsoluteX,       0, // 1
    Absolute,        IndexedIndirect, 0,               0,               ZeroPage,        ZeroPage,        ZeroPage,        0,               Implicit,        Immediate,       Accumulator,     0,        Absolute,               Absolute,        Absolute,        0, // 2
    Relative,        IndirectIndexed, 0,               0,               0,               ZeroPageX,       ZeroPageX,       0,               Implicit,        AbsoluteY,       0,               0,               0,               AbsoluteX,       AbsoluteX,       0, // 3
    0,               IndexedIndirect, 0,               0,               0,               ZeroPage,        ZeroPage,        0,               Implicit,        Immediate,       Accumulator,     0,        Absolute,               Absolute,        Absolute,        0, // 4
    0,               IndirectIndexed, 0,               0,               0,               ZeroPageX,       ZeroPageX,       0,               Implicit,        AbsoluteY,       0,               0,               0,               AbsoluteX,       AbsoluteX,       0, // 5
    Implicit,        IndexedIndirect, 0,               0,               0,               ZeroPage,        ZeroPage,        0,               Implicit,        Immediate,       Accumulator,     0,        Indirect,               Absolute,        Absolute,        0, // 6
    0,               IndirectIndexed, 0,               0,               0,               ZeroPageX,       ZeroPageX,       0,               Implicit,        AbsoluteY,       0,               0,               0,               AbsoluteX,       AbsoluteX,       0, // 7
    0,               IndexedIndirect, 0,               0,               ZeroPage,        ZeroPage,        ZeroPage,        0,               Implicit,        0,               0,               0,        Absolute,               Absolute,        Absolute,        0, // 8
    0,               IndirectIndexed, 0,               0,               ZeroPageX,       ZeroPageX,       ZeroPageY,       0,               0,               AbsoluteY,       0,               0,               0,               AbsoluteX,       0,               0, // 9
    Immediate,       IndexedIndirect, Immediate,       0,               ZeroPage,        ZeroPage,        ZeroPage,        0,               0,               Immediate,       0,               0,        Absolute,               Absolute,        Absolute,        0, // A
    0,               IndirectIndexed, 0,               0,               ZeroPageX,       ZeroPageX,       ZeroPageY,       0,               Implicit,        AbsoluteY,       0,               0,       AbsoluteX,               AbsoluteX,       AbsoluteY,       0, // B
    Immediate,       IndexedIndirect, 0,               0,               ZeroPage,        ZeroPage,        ZeroPage,        0,               Implicit,        Immediate,       Implicit,        0,        Absolute,               Absolute,        Absolute,        0, // C
    Relative,        IndirectIndexed, 0,               0,               0,               ZeroPageX,       ZeroPageX,       0,               0,               AbsoluteY,       0,               0,               0,               AbsoluteX,       AbsoluteX,       0, // D
    Immediate,       IndexedIndirect, 0,               0,               ZeroPage,        ZeroPage,        ZeroPage,        0,               Implicit,        Immediate,       Implicit,        0,        Absolute,               Absolute,        Absolute,        0, // E
    0,               IndirectIndexed, 0,               0,               0,               ZeroPageX,       ZeroPageX,       0,               Implicit,        AbsoluteY,       0,               0,               0,               AbsoluteX,       AbsoluteX,       0  // F
  };

  unsigned char buffer[262160];
  FILE *file;

  file = fopen("6502_functional_test.bin", "rb");
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
  for (state.pc = memoryAddressToStartAt; state.pc < 50000;)
  {
    int i = state.pc;
    int initialPc = state.pc;

    instr = buffer[i];

#ifdef PRINT_PC
    printf("PC: %04x\n", state.pc);
#endif

    if (instructions[instr] != 0)
    {
      instructions[instr](instr, addressingModes[instr], &state);
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
#ifdef PRINT_STACK_VALUES
    printf("\nTop stack values: %02x %02x %02x %02x %02x\n", memory[0x01FF], memory[0x01FE], memory[0x01FD], memory[0x01FC], memory[0x01FB]);
#endif
#ifdef PRINT_GAP
    printf("\n\n");
#endif

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
    //if (instructionsExecuted > 8) {
      printf("Stopping due to instruction limit.\n");
      printf("test number: %02x\n", memory[0x0200]);
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



