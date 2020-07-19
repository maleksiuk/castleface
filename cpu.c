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

// TODO: consider storing the status flags in a single byte

/*#define PRINT_INSTRUCTION 1*/
/*#define PRINT_INSTRUCTION_DESCRIPTION 1*/
/*#define PRINT_STATE 1*/
/*#define PRINT_GAP 1*/
/*#define PRINT_PC 1*/

void printState(struct Computer *state)
{
#ifdef PRINT_STATE
  printf("State: A=%02x X=%02x Y=%02x Z=%02x N=%02x C=%02x V=%02x PC=%x S=%02x", state->acc, state->xRegister, state->yRegister, state->zeroFlag, state->negativeFlag, state->carryFlag, state->overflowFlag, state->pc, state->stackRegister);
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

enum AddressingMode { Implicit, Immediate, ZeroPage, ZeroPageX, ZeroPageY, Relative, Absolute, AbsoluteX, AbsoluteY, Indirect, IndexedIndirect, IndirectIndexed, Accumulator };

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

  int length = 0;
  printInstruction(instr, length, state);
  printInstructionDescription("BRK", addressingMode, "force interrupt");

  unsigned char lowNibble = (state->memory)[0xFFFE];
  unsigned char highNibble = (state->memory)[0xFFFF];

  state->pc = (highNibble << 8) | lowNibble;
}

void rti(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state) 
{
  unsigned char processorFlags = popFromStack(state->memory, &state->stackRegister);
  unsigned char pcLowNibble = popFromStack(state->memory, &state->stackRegister);
  unsigned char pcHighNibble = popFromStack(state->memory, &state->stackRegister);

  state->carryFlag = (processorFlags & 0x01) != 0;
  state->zeroFlag = (processorFlags & 0x02) != 0;
  state->interruptDisable = (processorFlags & 0x04) != 0;
  state->decimalFlag = (processorFlags & 0x08) != 0;
  state->overflowFlag = (processorFlags & 0x40) != 0;
  state->negativeFlag = (processorFlags & 0x80) != 0;

  int length = 0;
  printInstruction(instr, length, state);
  printInstructionDescription("RTI", addressingMode, "return from interrupt");

  /*state.interruptDisable = 0;*/

  state->pc = (pcHighNibble << 8) | pcLowNibble;
}

void setAcc(unsigned char value, struct Computer *state)
{
  state->acc = value;
  setZeroFlag(state->acc, &state->zeroFlag);
  setNegativeFlag(state->acc, &state->negativeFlag);
}

void setX(unsigned char value, struct Computer *state)
{
  state->xRegister = value;
  setZeroFlag(state->xRegister, &state->zeroFlag);
  setNegativeFlag(state->xRegister, &state->negativeFlag);
}

void setY(unsigned char value, struct Computer *state)
{
  state->yRegister = value;
  setZeroFlag(state->yRegister, &state->zeroFlag);
  setNegativeFlag(state->yRegister, &state->negativeFlag);
}

void lda(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  unsigned char value = 0;

  int length = getOperandValue(&value, addressingMode, state);

  printInstruction(instr, length, state);
  printInstructionDescription("LDA", addressingMode, "set acc to value %02x", value);

  setAcc(value, state);
  state->pc += (1 + length);
}

void ldx(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;
  unsigned char value = 0;

  length = getOperandValue(&value, addressingMode, state);

  printInstruction(instr, length, state);
  printInstructionDescription("LDX", addressingMode, "set x to value %02x", value);

  setX(value, state);
  state->pc += (1 + length);
}

void ldy(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;
  unsigned char value = 0;

  length = getOperandValue(&value, addressingMode, state);

  printInstruction(instr, length, state);
  printInstructionDescription("LDY", addressingMode, "set y to value %02x", value);

  setY(value, state);
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

void cld(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;
  printInstruction(instr, length, state);
  printInstructionDescription("CLD", addressingMode, "set decimal flag to 0");

  state->decimalFlag = 0;
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

    setAcc(result, state);
    state->carryFlag = (result > 255);
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
    result = result | oldCarryFlag;  // make bit 0 have the value of the old carry flag
    setAcc(result, state);
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

  setAcc(state->acc & value, state);

  state->pc += (1 + length);
}

void eor(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  unsigned char value = 0;
  int length = getOperandValue(&value, addressingMode, state);

  printInstruction(instr, length, state);
  printInstructionDescription("EOR", addressingMode, "exclusive or between acc and %02x", value);

  setAcc(state->acc ^ value, state);

  state->pc += (1 + length);
}

void ora(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  unsigned char value = 0;
  int length = getOperandValue(&value, addressingMode, state);

  printInstruction(instr, length, state);
  printInstructionDescription("ORA", addressingMode, "logical inclusive OR between acc and %02x", value);

  setAcc(state->acc | value, state);

  state->pc += (1 + length);
}

void add(unsigned char value, struct Computer *state)
{
  unsigned char originalCarryFlag = state->carryFlag;
  unsigned int result = state->acc + value + originalCarryFlag;
  unsigned char overflowFlag = ((state->acc^result)&(value^result)&0x80) != 0;
  setAcc(result, state);
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

  setX(state->xRegister + 1, state);

  printInstruction(instr, length, state);
  printInstructionDescription("INX", addressingMode, "increment x register to become %x", state->xRegister);

  state->pc += (1 + length);
}

void iny(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;

  setY(state->yRegister + 1, state);

  printInstruction(instr, length, state);
  printInstructionDescription("INY", addressingMode, "increment y register to become %x", state->yRegister);

  state->pc += (1 + length);
}

void dex(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;

  setX(state->xRegister - 1, state);

  printInstruction(instr, length, state);
  printInstructionDescription("DEX", addressingMode, "decrement x register to become %x", state->xRegister);

  state->pc += (1 + length);
}

void dey(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 0;

  setY(state->yRegister - 1, state);

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
  setAcc(popFromStack(state->memory, &state->stackRegister), state);

  int length = 0;
  printInstruction(instr, length, state);
  printInstructionDescription("PLA", addressingMode, "pull from stack and into acc: %02x", state->acc);

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

signed char branchIfTrue(unsigned char val, unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  int length = 1;
  printInstruction(instr, length, state);
  signed char relativeDisplacement = state->memory[state->pc+1];
  signed char branchedByRelativeDisplacement = 0;

  if (val == 1) 
  {
    state->pc += relativeDisplacement;
    branchedByRelativeDisplacement = relativeDisplacement;
  } 

  state->pc += (1 + length);

  return branchedByRelativeDisplacement;
}

void beq(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{  
  signed char branchedByRelativeDisplacement = branchIfTrue(state->zeroFlag == 1, instr, addressingMode, state);
  printInstructionDescription("BEQ", addressingMode, "branch if the zero flag is one; branched by %d", branchedByRelativeDisplacement);
}

void bcc(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{  
  signed char branchedByRelativeDisplacement = branchIfTrue(state->carryFlag == 0, instr, addressingMode, state);
  printInstructionDescription("BCC", addressingMode, "branch if the carry flag is zero; branched by %d", branchedByRelativeDisplacement);
}

void bcs(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{  
  signed char branchedByRelativeDisplacement = branchIfTrue(state->carryFlag == 1, instr, addressingMode, state);
  printInstructionDescription("BCS", addressingMode, "branch if the carry flag is set; branched by %d", branchedByRelativeDisplacement);
}

void bvc(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{  
  signed char branchedByRelativeDisplacement = branchIfTrue(state->overflowFlag == 0, instr, addressingMode, state);
  printInstructionDescription("BVC", addressingMode, "branch if the overflow flag is zero; branched by %d", branchedByRelativeDisplacement);
}

void bvs(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{  
  signed char branchedByRelativeDisplacement = branchIfTrue(state->overflowFlag == 1, instr, addressingMode, state);
  printInstructionDescription("BVS", addressingMode, "branch if the overflow flag is set; branched by %d", branchedByRelativeDisplacement);
}

void bpl(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{  
  signed char branchedByRelativeDisplacement = branchIfTrue(state->negativeFlag == 0, instr, addressingMode, state);
  printInstructionDescription("BPL", addressingMode, "branch if the negative flag is zero; branched by %d", branchedByRelativeDisplacement);
}

void bmi(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  signed char branchedByRelativeDisplacement = branchIfTrue(state->negativeFlag == 1, instr, addressingMode, state);
  printInstructionDescription("BMI", addressingMode, "branch if the negative flag is one; branched by %d", branchedByRelativeDisplacement);
}

void bne(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  signed char branchedByRelativeDisplacement = branchIfTrue(state->zeroFlag == 0, instr, addressingMode, state);
  printInstructionDescription("BNE", addressingMode, "branch if the zero flag is zero; branched by %d", branchedByRelativeDisplacement);
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

void tay(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  setY(state->acc, state);

  int length = 0;
  printInstruction(instr, length, state);
  printInstructionDescription("TAY", addressingMode, "transfer acc %x to y reg", state->acc);

  state->pc += (1 + length);
}

void tya(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  setAcc(state->yRegister, state);

  int length = 0;
  printInstruction(instr, length, state);
  printInstructionDescription("TYA", addressingMode, "transfer Y %x to acc", state->yRegister);

  state->pc += (1 + length);
}

void tax(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  setX(state->acc, state);

  int length = 0;
  printInstruction(instr, length, state);
  printInstructionDescription("TAX", addressingMode, "transfer acc %x to x reg", state->acc);

  state->pc += (1 + length);
}

void txa(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  setAcc(state->xRegister, state);

  int length = 0;
  printInstruction(instr, length, state);
  printInstructionDescription("TXA", addressingMode, "transfer X %x to acc", state->xRegister);

  state->pc += (1 + length);
}

void txs(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  state->stackRegister = state->xRegister;

  int length = 0;
  printInstruction(instr, length, state);
  printInstructionDescription("TXS", addressingMode, "transfer X %x to stack reg", state->xRegister);

  state->pc += (1 + length);
}

void tsx(unsigned char instr, enum AddressingMode addressingMode, struct Computer *state)
{
  setX(state->stackRegister, state);

  int length = 0;
  printInstruction(instr, length, state);
  printInstructionDescription("TSX", addressingMode, "transfer stack register %x to x reg", state->stackRegister);

  state->pc += (1 + length);
}

  // instruction table
void (*instructions[256])(unsigned char, enum AddressingMode, struct Computer *) = {
  //  0     1     2     3      4     5     6     7     8     9     A     B     C     D     E     F 
  &brk, &ora, 0,    0,     0, &ora,    &asl, 0, &php, &ora, &asl,    0,    0, &ora, &asl,    0, // 0
  &bpl, &ora, 0,    0,     0, &ora,    &asl, 0, &clc, &ora,    0,    0,    0, &ora, &asl,    0, // 1
  &jsr, &and, 0,    0,  &bit, &and,    &rol, 0, &plp, &and, &rol,    0, &bit, &and, &rol,    0, // 2
  &bmi, &and, 0,    0,     0, &and,    &rol, 0, &sec, &and,    0,    0,    0, &and, &rol,    0, // 3
  &rti, &eor, 0,    0,     0, &eor,    &lsr, 0, &pha, &eor, &lsr,    0, &jmp, &eor, &lsr,    0, // 4
  &bvc, &eor, 0,    0,     0, &eor,    &lsr, 0, &cli, &eor,    0,    0,    0, &eor, &lsr,    0, // 5
  &rts, &adc, 0,    0,     0, &adc,    &ror, 0, &pla, &adc, &ror,    0, &jmp, &adc, &ror,    0, // 6
  &bvs, &adc, 0,    0,     0, &adc,    &ror, 0, &sei, &adc,    0,    0,    0, &adc, &ror,    0, // 7
  0,    &sta, 0,    0,  &sty, &sta,    &stx, 0, &dey,    0, &txa,    0, &sty, &sta, &stx,    0, // 8
  &bcc, &sta, 0,    0,  &sty, &sta,    &stx, 0, &tya, &sta, &txs,    0,    0, &sta,    0,    0, // 9
  &ldy, &lda, &ldx, 0,  &ldy, &lda,    &ldx, 0, &tay, &lda, &tax,    0, &ldy, &lda, &ldx,    0, // A
  &bcs, &lda, 0,    0,  &ldy, &lda,    &ldx, 0, &clv, &lda, &tsx,    0, &ldy, &lda, &ldx,    0, // B
  &cpy, &cmp, 0,    0,  &cpy, &cmp,    &dec, 0, &iny, &cmp, &dex,    0, &cpy, &cmp, &dec,    0, // C
  &bne, &cmp, 0,    0,     0, &cmp,    &dec, 0, &cld, &cmp,    0,    0,    0, &cmp, &dec,    0, // D
  &cpx, &sbc, 0,    0,  &cpx, &sbc,    &inc, 0, &inx, &sbc, &nop,    0, &cpx, &sbc, &inc,    0, // E
  &beq, &sbc, 0,    0,     0, &sbc,    &inc, 0, &sed, &sbc,    0,    0,    0, &sbc, &inc,    0  // F
};

enum AddressingMode addressingModes[256] = {
  //  0            1                2                3                4                5                6                7                8                9                A                B                C                D                E                F 
  Implicit,        IndexedIndirect, 0,               0,               0,               ZeroPage,        ZeroPage,        0,               Implicit,        Immediate,       Accumulator,     0,               0,               Absolute,        Absolute,        0, // 0
  Relative,        IndirectIndexed, 0,               0,               0,               ZeroPageX,       ZeroPageX,       0,               Implicit,        AbsoluteY,       0,               0,               0,               AbsoluteX,       AbsoluteX,       0, // 1
  Absolute,        IndexedIndirect, 0,               0,               ZeroPage,        ZeroPage,        ZeroPage,        0,               Implicit,        Immediate,       Accumulator,     0,        Absolute,               Absolute,        Absolute,        0, // 2
  Relative,        IndirectIndexed, 0,               0,               0,               ZeroPageX,       ZeroPageX,       0,               Implicit,        AbsoluteY,       0,               0,               0,               AbsoluteX,       AbsoluteX,       0, // 3
  Implicit,        IndexedIndirect, 0,               0,               0,               ZeroPage,        ZeroPage,        0,               Implicit,        Immediate,       Accumulator,     0,        Absolute,               Absolute,        Absolute,        0, // 4
  Relative,        IndirectIndexed, 0,               0,               0,               ZeroPageX,       ZeroPageX,       0,               Implicit,        AbsoluteY,       0,               0,               0,               AbsoluteX,       AbsoluteX,       0, // 5
  Implicit,        IndexedIndirect, 0,               0,               0,               ZeroPage,        ZeroPage,        0,               Implicit,        Immediate,       Accumulator,     0,        Indirect,               Absolute,        Absolute,        0, // 6
  Relative,        IndirectIndexed, 0,               0,               0,               ZeroPageX,       ZeroPageX,       0,               Implicit,        AbsoluteY,       0,               0,               0,               AbsoluteX,       AbsoluteX,       0, // 7
  0,               IndexedIndirect, 0,               0,               ZeroPage,        ZeroPage,        ZeroPage,        0,               Implicit,        0,               Implicit,        0,        Absolute,               Absolute,        Absolute,        0, // 8
  Relative,        IndirectIndexed, 0,               0,               ZeroPageX,       ZeroPageX,       ZeroPageY,       0,               Implicit,        AbsoluteY,       Implicit,        0,               0,               AbsoluteX,       0,               0, // 9
  Immediate,       IndexedIndirect, Immediate,       0,               ZeroPage,        ZeroPage,        ZeroPage,        0,               Implicit,        Immediate,       Implicit,        0,        Absolute,               Absolute,        Absolute,        0, // A
  Relative,        IndirectIndexed, 0,               0,               ZeroPageX,       ZeroPageX,       ZeroPageY,       0,               Implicit,        AbsoluteY,       Implicit,        0,       AbsoluteX,               AbsoluteX,       AbsoluteY,       0, // B
  Immediate,       IndexedIndirect, 0,               0,               ZeroPage,        ZeroPage,        ZeroPage,        0,               Implicit,        Immediate,       Implicit,        0,        Absolute,               Absolute,        Absolute,        0, // C
  Relative,        IndirectIndexed, 0,               0,               0,               ZeroPageX,       ZeroPageX,       0,               Implicit,        AbsoluteY,       0,               0,               0,               AbsoluteX,       AbsoluteX,       0, // D
  Immediate,       IndexedIndirect, 0,               0,               ZeroPage,        ZeroPage,        ZeroPage,        0,               Implicit,        Immediate,       Implicit,        0,        Absolute,               Absolute,        Absolute,        0, // E
  Relative,        IndirectIndexed, 0,               0,               0,               ZeroPageX,       ZeroPageX,       0,               Implicit,        AbsoluteY,       0,               0,               0,               AbsoluteX,       AbsoluteX,       0  // F
};

void executeInstruction(unsigned char instr, struct Computer *state)
{
#ifdef PRINT_PC
  printf("PC: %04x\n", state->pc);
#endif

  instructions[instr](instr, addressingModes[instr], state);

  printState(state);
#ifdef PRINT_STACK_VALUES
  printf("\nTop stack values: %02x %02x %02x %02x %02x\n", state->memory[0x01FF], state->memory[0x01FE], state->memory[0x01FD], state->memory[0x01FC], state->memory[0x01FB]);
#endif
#ifdef PRINT_GAP
  printf("\n\n");
#endif

}


