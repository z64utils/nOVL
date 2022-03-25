#include <stdint.h>
#include "novl.h"

void novl_mips_clearall ( );
void novl_mips_checkmerge ( uint32_t a );
void novl_mips_got_hi16 ( uint32_t a, uint32_t instr, novl_reloc_entry * reloc );
void novl_mips_got_lo16 ( uint32_t a, uint32_t instr, novl_reloc_entry * reloc );
void novl_mips_futurestate ( uint32_t a );
void novl_mips_clearstate ( );
void novl_mips_jr ( );

int novl_mips_is_branch ( uint32_t instr );
int novl_mips_is_branch_likely ( uint32_t instr );
int novl_mips_is_forward_branch ( uint32_t instr );
int novl_mips_is_jump ( uint32_t instr );
int novl_mips_is_jr ( uint32_t instr );
