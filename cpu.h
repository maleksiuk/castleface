#ifndef FILE_CPU_H_SEEN
#define FILE_CPU_H_SEEN

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

void executeInstruction(unsigned char instr, struct Computer *state);

#endif /* !FILE_CPU_H_SEEN */
