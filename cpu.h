#ifndef FILE_CPU_H_SEEN
#define FILE_CPU_H_SEEN

#include <stdbool.h>

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

  bool irqPending;
  bool nmiPending;
};

void executeInstruction(unsigned char instr, struct Computer *state);
void triggerIrqInterrupt(struct Computer *state);
void fireIrqInterrupt(struct Computer *state);
void triggerNmiInterrupt(struct Computer *state);
void fireNmiInterrupt(struct Computer *state);

#endif /* !FILE_CPU_H_SEEN */
