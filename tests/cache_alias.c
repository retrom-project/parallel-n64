/* Production SW/invalidation/jump dispatch; memory and decoder are test fixtures. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct precomp_instr { void (*ops)(void); uint32_t addr; };
struct precomp_block {
   uint32_t start, end;
   void *code, *jumps_table, *riprel_table;
   struct precomp_instr *block;
};
static char invalid_code[0x100000];
static struct precomp_block *blocks[0x100000], *actual;
static struct precomp_instr *PC;
static uint32_t jump_to_address, address, cpu_word, ram[1024];
static int skip_jump, r4300emu, decoded, translations;
static struct {int r4300;} g_dev;
static int32_t base_register, immediate;
static int64_t value_register;
enum { CORE_PURE_INTERPRETER = 0, CORE_INTERPRETER = 1, CORE_DYNAREC = 2 };
static void uncompiled(void) {}
static void old_instruction(void) {}
static struct {void (*NOTCOMPILED)(void);} current_instruction_table = {uncompiled};
static uint32_t virtual_to_physical_address(void *cpu, uint32_t addr, int access)
{(void)cpu; (void)access; translations++; return addr;}
static void dyna_jump(void) {abort();}
static void write_word_in_memory(void) {ram[(address & 0xfff) / 4] = cpu_word;}
static void init_block(struct precomp_block *block)
{
   decoded++;
   for (unsigned i = 0; i < 1024; i++) block->block[i].ops = uncompiled;
   invalid_code[block->start >> 12] = 0;
}
#define irs32 base_register
#define iimmediate immediate
#define irt value_register
#define ADD_TO_PC(n) (PC += (n))
#define DECLARE_INSTRUCTION(name) static void name(void)
#define jump_to(a) do {jump_to_address = (a); jump_to_func();} while (0)

/* PRODUCTION_FUNCTIONS */

static void prepare(uint32_t page)
{
   blocks[page] = calloc(1, sizeof(*blocks[page]));
   assert(blocks[page]);
   blocks[page]->start = page << 12;
   blocks[page]->end = (page + 1) << 12;
   blocks[page]->block = calloc(1024, sizeof(*blocks[page]->block));
   assert(blocks[page]->block);
   for (unsigned i = 0; i < 1024; i++) blocks[page]->block[i].ops = uncompiled;
   invalid_code[page] = 0;
}

int main(int argc, char **argv)
{
   uint32_t write_addr = 0xa0000180, exec_addr = 0x80000180;
   struct precomp_instr store[2] = {{0}, {0}};
   int data_page = 0, tlb = 0;
   assert(argc == 2);
   r4300emu = CORE_INTERPRETER;
   if (!strcmp(argv[1], "cached-to-uncached")) {
      write_addr = 0x80000180; exec_addr = 0xa0000180;
   }
   if (!strcmp(argv[1], "same-alias")) write_addr = exec_addr;
   if (!strcmp(argv[1], "last-word")) {write_addr = 0xa0000ffc; exec_addr = 0x80000ffc;}
   if (!strcmp(argv[1], "data-page")) data_page = 1;
   if (!strcmp(argv[1], "tlb-address")) {write_addr = 0x00400180; exec_addr = 0x20400180; tlb = 1;}
   memset(invalid_code, 1, sizeof(invalid_code));
   prepare(write_addr >> 12);
   if ((write_addr >> 12) != (exec_addr >> 12)) prepare(exec_addr >> 12);
   if (!data_page) blocks[exec_addr >> 12]->block[(exec_addr & 0xfff) / 4].ops = old_instruction;
   if (!strncmp(argv[1], "restore-", 8)) {
      ram[(exec_addr & 0xfff) / 4] = 0x3c1b8001; /* RAM was replaced by state loading. */
      PC = store;
      if (!strcmp(argv[1], "restore-pure")) r4300emu = CORE_PURE_INTERPRETER;
      savestates_load_set_pc(exec_addr);
      if (r4300emu == CORE_PURE_INTERPRETER) {
         assert(decoded == 0);
         assert(PC->addr == exec_addr);
         assert(invalid_code[exec_addr >> 12] == 0);
      } else {
         assert(decoded == 1);
         assert(PC->ops == current_instruction_table.NOTCOMPILED);
      }
      return 0;
   }
   ram[(write_addr & 0xfff) / 4] = 0x24020001;
   base_register = (int32_t)write_addr;
   value_register = 0x3c1b8001; /* first instruction of a newly installed exception trampoline */
   PC = store;
   SW();
   assert(PC == store + 1);
   assert(ram[(write_addr & 0xfff) / 4] == 0x3c1b8001);
   assert(translations == 0);
   if (data_page || tlb) {
      assert(invalid_code[write_addr >> 12] == 0);
      assert(invalid_code[exec_addr >> 12] == 0);
      return 0;
   }
   jump_to_address = exec_addr;
   jump_to_func();
   /* Dispatch must request decoding of the updated RAM, never retain the old opcode. */
   assert(decoded == 1);
   assert(PC == blocks[exec_addr >> 12]->block + (exec_addr & 0xfff) / 4);
   assert(PC->ops == current_instruction_table.NOTCOMPILED);
   assert(ram[(exec_addr & 0xfff) / 4] == 0x3c1b8001);
   return 0;
}
