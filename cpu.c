#include <stdio.h>
#include <stdlib.h>

// the 6502 has 256 byte pages

/*
 * Starting out by seeing if I can get the functional test to pass from https://github.com/koute/pinky
 * (file name: 6502_functional_test.bin)
 * actually this one seems to work better: https://github.com/Klaus2m5/6502_65C02_functional_tests
 *
 * Based on recommendation from https://www.reddit.com/r/EmuDev/comments/9s755i/is_there_a_comprehensive_nes_emulation_guide/e8qi80f/
 *
 * Addressing: http://obelisk.me.uk/6502/addressing.html#ZPG
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

int main(int argc, char **argv) 
{
  printf("hi there\n");

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

  unsigned char acc = 0;
  unsigned char zeroFlag = 0;
  unsigned char negativeFlag = 0;
  unsigned char carryFlag = 0;
  unsigned char overflowFlag = 0;
  unsigned char decimalFlag = 0; 
  unsigned char interruptDisable = 0;
  unsigned char xRegister = 0;   // 8-bit index register
  unsigned char yRegister = 0;   // 8-bit index register
  unsigned char stackRegister = 0;  // stack register

  int instructionsExecuted = 0;

  int memoryAddressToStartAt = 0x0400;  // just for the test file

  printf("\n\nexecution:\n");
  for (int pc = memoryAddressToStartAt; pc < 50000;)
  {
    int i = pc;
    int initialPc = pc;

    instr = buffer[i];

    printf("PC: %04x\n", pc);

    // LDA; Absolute; Len 3; Time 4
    if (instr == 0xAD)
    {
      unsigned int memoryAddress = (buffer[i+2] << 8) | buffer[i+1];
      acc = memory[memoryAddress];
      zeroFlag = (acc == 0);
      negativeFlag = ((acc & 0xFF) == 0x80);
      printf("%02x %02x %02x\n", instr, buffer[i+1], buffer[i+2]);
      printf("-> LDA %02x%02x -> acc = %d\n", buffer[i+2], buffer[i+1], acc);
      pc += 3;
    } 
    // LDA; Immediate; Len 2; Time 2
    else if (instr == 0xA9)
    {
      acc = buffer[i+1];
      zeroFlag = (acc == 0);
      negativeFlag = ((acc & 0xFF) == 0x80);
      printf("%02x %02x\n", instr, buffer[i+1]);
      printf("-> LDA #%02x -> acc = %d\n", acc, acc);
      pc += 2;
    }
    // BNE; Branch on not equal; Len 2
    else if (instr == 0xD0)
    {
      signed char relativeDisplacement = buffer[i+1];
      printf("%02x %02x\n", instr, buffer[i+1]);
      if (zeroFlag == 0) {
        printf("-> BNE %02x -> jump by %d\n", relativeDisplacement, relativeDisplacement);
        pc += relativeDisplacement;
      } else {
        printf("-> BNE %02x -> did not jump by %d\n", relativeDisplacement, relativeDisplacement);
      }

      pc += 2;
    }
    // BEQ; Branch if equal; Len 2
    else if (instr == 0xF0)
    {
      signed char relativeDisplacement = buffer[i+1];
      printf("%02x %02x\n", instr, buffer[i+1]);
      if (zeroFlag == 1) {
        printf("-> BEQ %02x -> jump by %d\n", relativeDisplacement, relativeDisplacement);
        pc += relativeDisplacement;
      } else {
        printf("-> BEQ %02x -> did not jump by %d\n", relativeDisplacement, relativeDisplacement);
      }

      pc += 2;
    }
    // BCC; Branch if carry clear; Len 2
    else if (instr == 0x90)
    {
      signed char relativeDisplacement = buffer[i+1];
      printf("%02x %02x\n", instr, buffer[i+1]);
      if (carryFlag == 0) {
        printf("-> BCC %02x -> carry flag is zero, so jump by %d\n", relativeDisplacement, relativeDisplacement);
        pc += relativeDisplacement;
      } else {
        printf("-> BCC %02x -> carry flag is not zero, so did not jump by %d\n", relativeDisplacement, relativeDisplacement);
      }

      pc += 2;
    }
    // BCS; Branch if carry set; Len 2
    else if (instr == 0xB0)
    {
      signed char relativeDisplacement = buffer[i+1];
      printf("%02x %02x\n", instr, buffer[i+1]);
      if (carryFlag == 1) {
        printf("-> BCS %02x -> carry flag is set, so jump by %d\n", relativeDisplacement, relativeDisplacement);
        pc += relativeDisplacement;
      } else {
        printf("-> BCS %02x -> carry flag is not set, so did not jump by %d\n", relativeDisplacement, relativeDisplacement);
      }

      pc += 2;
    }
    // JMP; Jump; Absolute; Len 3, Time 3
    else if (instr == 0x4C)
    {
      unsigned int memoryAddress = (buffer[i+2] << 8) | buffer[i+1];
      printf("%02x %02x %02x\n", instr, buffer[i+1], buffer[i+2]);
      printf("-> JMP to memory address: %x\n", memoryAddress);
      pc = memoryAddress;
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
      pc = memoryAddress;
    }
    // JSR; Jump to subroutine; Absolute; Len 3; Time 6
    else if (instr == 0x20)
    {
      unsigned int memoryAddress = (buffer[i+2] << 8) | buffer[i+1];

      unsigned int pcToPutInStack = (pc + 3) - 1;

      unsigned int loc1 = 0x0100 + stackRegister;
      memory[loc1] = pcToPutInStack >> 8;
      stackRegister--;

      unsigned int loc2 = 0x0100 + stackRegister;
      memory[loc2] = pcToPutInStack;  // TODO: do I need to cast to unsigned char?
      stackRegister--;

      printf("%02x %02x %02x\n", instr, buffer[i+1], buffer[i+2]);
      printf("-> JSR - jump to subroutine: %x\n", memoryAddress);
      pc = memoryAddress;
    }
    // RTS; Len 1; Time 6
    else if (instr == 0x60)
    {
      stackRegister++;
      unsigned int loc1 = 0x0100 + stackRegister;
      unsigned char lowNibble = memory[loc1];

      stackRegister++;
      unsigned int loc2 = 0x0100 + stackRegister;
      unsigned char highNibble = memory[loc2];

      unsigned int memoryAddress = (highNibble << 8) | lowNibble;

      printf("%02x\n", instr);
      printf("-> RTS - return from subroutine: %x\n", memoryAddress);
      pc = memoryAddress;
      pc++;
    }
    // PHA; Push acc value to the stack; Len 1; Time 3
    else if (instr == 0x48)
    {
      printf("%02x\n", instr);
      printf("-> PHA (push acc value %02x to stack at position %02x)\n", acc, stackRegister);

      unsigned int loc = 0x0100 + stackRegister;
      memory[loc] = acc;
      stackRegister--;

      pc++;
    }
    // PLA; Pull from stack and into acc; Len 1; Time 4
    else if (instr == 0x68)
    {
      stackRegister++;
      unsigned int loc = 0x0100 + stackRegister;
      acc = memory[loc];
      
      printf("%02x\n", instr);
      printf("-> PLA (pull from stack position %02x and into acc -> %02x)\n", stackRegister, acc);

      zeroFlag = (acc == 0);
      setNegativeFlag(acc, &negativeFlag);

      pc++;
    }
    // PLP; Pull processor status; Len 1; Time 4
    else if (instr == 0x28)
    {
      stackRegister++;
      unsigned int loc = 0x0100 + stackRegister;
      unsigned char value = memory[loc];

      // NV1BDIZC
      carryFlag = (value & 0x01) != 0;
      zeroFlag = (value & 0x02) != 0;
      interruptDisable = (value & 0x04) != 0;
      decimalFlag = (value & 0x08) != 0;
      overflowFlag = (value & 0x40) != 0;
      negativeFlag = (value & 0x80) != 0;

      printf("%02x\n", instr);
      printf("-> PLP (pull from stack position %02x and into status flags)\n", stackRegister);

      pc++;
    }
    // PHP; Push processor status; Len 1; Time 3
    else if (instr == 0x08)
    {
      // NV1BDIZC
      unsigned char value = (negativeFlag << 7) | (overflowFlag << 6) | (1 << 5) | (1 << 4)
        | (decimalFlag << 3) | (interruptDisable << 2) | (zeroFlag << 1) | (carryFlag);

      unsigned int loc = 0x0100 + stackRegister;
      memory[loc] = value;

      printf("%02x\n", instr);
      printf("-> PHP (push status flags %02x to stack position %02x)\n", value, stackRegister);

      stackRegister--;
      pc++;
    }
    // CLD; Clear decimal flag; Len 1; Time 2
    else if (instr == 0xD8)
    {
      printf("%02x\n", instr);
      printf("-> CLD (clear decimal flag)\n");
      decimalFlag = 0;
      pc++;
    } 
    // LDX; Load register X; immediate; Len 2; Time 2
    else if (instr == 0xA2) 
    {
      xRegister = buffer[i+1];
      zeroFlag = (xRegister == 0);
      setNegativeFlag(xRegister, &negativeFlag);
      printf("%02x %02x\n", instr, buffer[i+1]);
      printf("-> LDX #%x -- (load X with value %x)\n", xRegister, xRegister);
      pc += 2;
    }
    // TXS; Transfer X to Stack Pointer; Len 1; Time 2
    else if (instr == 0x9A)
    {
      stackRegister = xRegister;
      printf("%02x\n", instr);
      printf("-> TXS -- (transfer %x to stack reg)\n", xRegister);
      pc++;
    } 
    // TSX; Transfer Stack Pointer to X; Len 1; Time 2
    else if (instr == 0xBA)
    {
      xRegister = stackRegister;
      zeroFlag = (xRegister == 0);
      setNegativeFlag(xRegister, &negativeFlag);
      printf("%02x\n", instr);
      printf("-> TSX -- (transfer %x to x reg)\n", stackRegister);
      pc++;
    } 
    // TAY; Transfer Acc to Y; Len 1; Time 2
    else if (instr == 0xA8)
    {
      yRegister = acc;
      zeroFlag = (yRegister == 0);
      setNegativeFlag(yRegister, &negativeFlag);
      printf("%02x\n", instr);
      printf("-> TAY -- (transfer acc %x to y reg)\n", acc);
      pc++;
    } 
    // STA; Store accumulator in memory; absolute; Len 3; Time 4
    else if (instr == 0x8D)
    {
      int operand = buffer[i+2] << 8 | buffer[i+1];
      memory[operand] = acc;
      printf("%02x %02x %02x\n", instr, buffer[i+1], buffer[i+2]);
      printf("-> STA (set memory address %x to acc value %x)\n", operand, acc);
      pc += 3;
    }
    // CMP; Compare acc with another value; immediate; Len 2; Time 2
    else if (instr == 0xC9)
    {
      int operand = buffer[i+1];
      unsigned char result = acc - operand;
      zeroFlag = (result == 0);
      carryFlag = (acc >= operand);
      setNegativeFlag(result, &negativeFlag);
      printf("%02x %02x\n", instr, buffer[i+1]);
      printf("-> CMP #%02x (compare value %x to acc value %x)\n", operand, operand, acc);
      pc += 2;
    }
    // CMP; Compare acc with another value; absolute; Len 3; Time 4
    else if (instr == 0xCD)
    {
      int operand = buffer[i+2] << 8 | buffer[i+1];
      unsigned char value = memory[operand];
      unsigned char result = acc - value;
      zeroFlag = (result == 0);
      carryFlag = (acc >= value);
      setNegativeFlag(result, &negativeFlag);
      printf("%02x %02x %02x\n", instr, buffer[i+1], buffer[i+2]);
      printf("-> CMP %x (compare value at %x, %02x, to acc value %x)\n", operand, operand, value, acc);
      pc += 3;
    }
    // CPY; Compare y with another value; immediate; Len 2; Time 2
    else if (instr == 0xC0)
    {
      int operand = buffer[i+1];
      unsigned char result = yRegister - operand;
      zeroFlag = (result == 0);
      carryFlag = (yRegister >= operand);
      setNegativeFlag(result, &negativeFlag);
      printf("%02x %02x\n", instr, buffer[i+1]);
      printf("-> CPY #%02x (compare value %x to y value %x)\n", operand, operand, yRegister);
      pc += 2;
    }
    // CPX; Compare x with another value; immediate; Len 2; Time 2
    else if (instr == 0xE0)
    {
      int operand = buffer[i+1];
      unsigned char result = xRegister - operand;
      zeroFlag = (result == 0);
      carryFlag = (xRegister >= operand);
      setNegativeFlag(result, &negativeFlag);
      printf("%02x %02x\n", instr, buffer[i+1]);
      printf("-> CPX #%02x (compare value %x to x value %x)\n", operand, operand, xRegister);
      pc += 2;
    }
    // LDY; Load Y register with value; immediate; Len 2; Time 2
    else if (instr == 0xA0)
    {
      yRegister = buffer[i+1];
      zeroFlag = (yRegister == 0);
      setNegativeFlag(yRegister, &negativeFlag);
      printf("%02x %02x\n", instr, buffer[i+1]);
      printf("-> LDY #%x -- (load Y with value %x)\n", yRegister, yRegister);
      pc += 2;
    }
    // DEY; Decrement Y register; Len 1; Time 2
    else if (instr == 0x88)
    {
      yRegister = yRegister - 1;
      zeroFlag = (yRegister == 0);
      setNegativeFlag(yRegister, &negativeFlag);
      printf("%02x\n", instr);
      printf("-> DEY -- (decrement Y by 1 to become value %x)\n", yRegister);
      pc++;
    }
    // TYA; Transfer Y to acc; Len 1; Time 2
    else if (instr == 0x98)
    {
      acc = yRegister;
      zeroFlag = (acc == 0);
      setNegativeFlag(acc, &negativeFlag);
      printf("%02x\n", instr);
      printf("-> TYA -- (transfer Y to acc, so acc becomes %x)\n", acc);
      pc++;
    }
    // TAX; Transfer acc to X; Len 1; Time 2 
    else if (instr == 0xAA)
    {
      xRegister = acc;
      zeroFlag = (xRegister == 0);
      setNegativeFlag(xRegister, &negativeFlag);
      printf("%02x\n", instr);
      printf("-> TAX -- (transfer acc to X, so X becomes %x)\n", xRegister);
      pc++;
    }
    // TXA; Transfer X to acc; Len 1; Time 2 
    else if (instr == 0x8A)
    {
      acc = xRegister;
      zeroFlag = (acc == 0);
      setNegativeFlag(acc, &negativeFlag);
      printf("%02x\n", instr);
      printf("-> TXA -- (transfer X to acc, so acc becomes %x)\n", acc);
      pc++;
    }
    // BPL; Branch on result plus; Len 2
    else if (instr == 0x10)
    {
      signed char relativeDisplacement = buffer[i+1];
      printf("%02x %02x\n", instr, buffer[i+1]);

      if (negativeFlag == 0) 
      {
        // branch
        printf("-> BPL %02x -- (negative flag is zero, so jump by %02x)\n", buffer[i+1], relativeDisplacement);
        pc += relativeDisplacement;
      } 
      else 
      {
        // no branch
        printf("-> BPL %02x -- (negative flag is not zero, so do not jump)\n", buffer[i+1]);
      }

      pc += 2;
    }
    // BMI; Branch if negative flag set; Len 2
    else if (instr == 0x30)
    {
      signed char relativeDisplacement = buffer[i+1];
      printf("%02x %02x\n", instr, buffer[i+1]);

      if (negativeFlag == 1) 
      {
        // branch
        printf("-> BMI %02x -- (negative flag is one, so jump by %02x)\n", buffer[i+1], relativeDisplacement);
        pc += relativeDisplacement;
      } 
      else 
      {
        // no branch
        printf("-> BMI %02x -- (negative flag is not one, so do not jump)\n", buffer[i+1]);
      }

      pc += 2;
    }
    // BVC; Branch if overflow clear; Len 2
    else if (instr == 0x50)
    {
      signed char relativeDisplacement = buffer[i+1];
      printf("%02x %02x\n", instr, buffer[i+1]);

      if (overflowFlag == 0) 
      {
        // branch
        printf("-> BVC %02x -- (overflow flag is clear, so jump by %02x)\n", buffer[i+1], relativeDisplacement);
        pc += relativeDisplacement;
      } 
      else 
      {
        // no branch
        printf("-> BVC %02x -- (overflow flag is not clear, so do not jump)\n", buffer[i+1]);
      }

      pc += 2;
    }
    // BVS; Branch if overflow set; Len 2
    else if (instr == 0x70)
    {
      signed char relativeDisplacement = buffer[i+1];
      printf("%02x %02x\n", instr, buffer[i+1]);

      if (overflowFlag == 1) 
      {
        // branch
        printf("-> BVS %02x -- (overflow flag is set, so jump by %02x)\n", buffer[i+1], relativeDisplacement);
        pc += relativeDisplacement;
      } 
      else 
      {
        // no branch
        printf("-> BVS %02x -- (overflow flag is not set, so do not jump)\n", buffer[i+1]);
      }

      pc += 2;
    }
    // ADC; Add memory to acc with carry; Len 2, Time 2
    // There are some test programs here I should try: http://www.6502.org/tutorials/vflag.html
    else if (instr == 0x69)
    {
      unsigned char operand = buffer[i+1];
      printf("%02x %02x\n", instr, operand);

      if (decimalFlag == 1)
      {
        // Apparently the NES doesn't support decimal mode.
      }
      else
      {
        unsigned int result = acc + operand + carryFlag;
        acc = result;
        printf("-> ADC #%02x -- (add %02x and %02x to the acc, resulting in %02x)\n", operand, operand, carryFlag, acc);
        zeroFlag = (acc == 0);
        setNegativeFlag(acc, &negativeFlag);
        carryFlag = (result > 255);

        // TODO: Do this a better way. See // http://www.righto.com/2012/12/the-6502-overflow-flag-explained.html
        signed char signedAcc = acc;
        signed char signedOperand = operand;
        signed int signedResult = signedAcc + signedOperand;
        overflowFlag = (signedResult > 127 || signedResult < -128);
      }

      pc += 2;
    }
    // EOR; Len 2; Time 2
    else if (instr == 0x49)
    {
      unsigned char operand = buffer[i+1];
      acc = acc ^ operand;
      zeroFlag = (acc == 0);
      setNegativeFlag(acc, &negativeFlag);
      printf("%02x\n", instr);
      printf("-> EOR #%02x -- (exclusive or between acc and %02x)\n", operand, operand);
      pc += 2;
    }
    // DEX; Len 1; Time 2
    else if (instr == 0xCA)
    {
      xRegister = xRegister - 1;
      zeroFlag = (xRegister == 0);
      setNegativeFlag(xRegister, &negativeFlag);
      printf("%02x\n", instr);
      printf("-> DEX -- (decrement X by 1 -> results in %02x)\n", xRegister);
      pc++;
    }
    // NOP; Len 1; Time 2
    else if (instr == 0xEA)
    {
      printf("%02x\n", instr);
      printf("-> NOP -- (do nothing)\n");
      pc++;
    }
    // CLC; Len 1; Time 2
    else if (instr == 0x18)
    {
      printf("%02x\n", instr);
      printf("-> CLC -- (clear carry flag)\n");
      carryFlag = 0;
      pc++;
    }
    // INX; Increment X; Len 1; Time 2
    else if (instr == 0xE8)
    {
      xRegister += 1;
      zeroFlag = (xRegister == 0);
      setNegativeFlag(xRegister, &negativeFlag);

      printf("%02x\n", instr);
      printf("-> INX -- (increment x to be %02x)\n", xRegister);

      pc++;
    }
    // INY; Increment Y; Len 1; Time 2
    else if (instr == 0xC8)
    {
      yRegister += 1;
      zeroFlag = (yRegister == 0);
      setNegativeFlag(yRegister, &negativeFlag);

      printf("%02x\n", instr);
      printf("-> INY -- (increment y to be %02x)\n", yRegister);

      pc++;
    }
    else {
      printf("unknown instruction: %x\n", instr);
      printf("test number: %02x\n", memory[0x0200]);
      return(0);
    }

    printState(xRegister, yRegister, acc, zeroFlag, negativeFlag, carryFlag, overflowFlag, pc, stackRegister);
    printf("\nTop stack values: %02x %02x %02x %02x %02x\n", memory[0x01FF], memory[0x01FE], memory[0x01FD], memory[0x01FC], memory[0x01FB]);
    printf("\n\n");

    if (initialPc == pc) {
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



