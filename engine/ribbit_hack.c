#include "sdk.h"
#include "engine.h"

#define HACK_STDOUT (NULL)

void* hack_malloc(size_t size)
{
	void* result = Z_Malloc(size, PU_STATIC, NULL);

	if (result == NULL)
	{
		engine_error("RIBBIT",
		             "Ribbit VM wanted to malloc(), but it failed");
	}

	return result;
}

void* hack_free(void* ptr)
{
	if (ptr == NULL)
	{
		engine_error("RIBBIT",
		             "Ribbit VM tried to free() a null pointer");
	}

	Z_Free(ptr);
}

void hack_exit(int code)
{
	engine_error(
	    "RIBBIT",
	    "Ribbit VM requested exit; this absolutely should not happen!");
}

#define PUTCHAR_BUF_LEN 512

static uint8_t putchar_buf[PUTCHAR_BUF_LEN] = {0};

int hack_putchar(int c)
{
	size_t idx = 0;

	for (uint8_t* current = putchar_buf; idx < PUTCHAR_BUF_LEN - 1;
	     current++, idx++)
	{
		if (*current == 0)
		{
			current[0] = c;
			current[1] = 0;
			break;
		}
	}

	return 0;
}

int hack_fflush(FILE* file)
{
	if (file != HACK_STDOUT)
	{
		engine_error("RIBBIT", "`fflush` called outside of the ribbit "
		                       "module; this should never happen!");
	}

	player_t* pl = players + consoleplayer;

	pl->text.text = putchar_buf;
	pl->text.tic = leveltime + 5 * 35;
	pl->text.font = 0;
	pl->text.lines = 1;

	size_t idx = 0;

	for (uint8_t* current = putchar_buf; idx < PUTCHAR_BUF_LEN - 1;
	     current++, idx++)
	{
		if (*current == 0)
		{
			current[1] = 0;
			break;
		}

		if (*current == '\n')
		{
			pl->text.lines++;
		}
	}

	return 0;
}

int hack_getchar() { return 0; }

// @@(location pre_decl)@@

// HACK: non-destructively mangle the original VM code to use whatever shit the
// ACE engine provides.

#define NOSTART
#define NO_STD
#define NO_REG

#ifdef stdout
#undef stdout
#endif

// Obliterate these keywords entirely.
#define register      /* register */
#define asm           /* asm */
#define volatile(...) /* volatile */

#define stdout HACK_STDOUT

#define malloc hack_malloc
#define free hack_free
#define exit hack_exit
#define putchar hack_putchar
#define fflush hack_fflush
#define getchar hack_getchar

#include "ribbit_tmp.c"
#define false (!true)

#undef stdout

#undef malloc
#undef free
#undef exit
#undef putchar
#undef fflush
#undef getchar

#undef asm
#undef register
#undef volatile

#undef NOSTART
#undef NO_STD
#undef NO_REG

// @@(location post_decl)@@

void run_ribbit(bool initial_run);

void init_ribbit()
{
	// Mostly copied from _init.

	// @@(location pre_init)@@

	heap_start = Z_Malloc(sizeof(obj) * (SPACE_SZ << 1), PU_STATIC, NULL);

	if (heap_start == NULL)
	{
		engine_error("RIBBIT", "Failed to allocate a heap");
	}

	alloc = heap_bot;
	alloc_limit = heap_mid;
	stack = NUM_0;

	FALSE = TAG_RIB(alloc_rib(
	    TAG_RIB(alloc_rib(NUM_0, NUM_0, SINGLETON_TAG)),
	    TAG_RIB(alloc_rib(NUM_0, NUM_0, SINGLETON_TAG)), SINGLETON_TAG));

	build_sym_table();
	decode();

	set_global(TAG_RIB(alloc_rib(NUM_0, symbol_table, CLOSURE_TAG)));
	set_global(FALSE);
	set_global(TRUE);
	set_global(NIL);

	setup_stack();

	// @@(location post_init)@@

	// Initial run. Afterwards, it is run every tick.
	run_ribbit(true);
}

#define ADVANCE_PC                                                             \
	do                                                                     \
	{                                                                      \
		pc = TAG(pc);                                                  \
	} while (0)

obj init_fns = NUM_0;
obj tick_fns = NUM_0;

void run_ribbit(bool initial_run)
{
	obj append_from = initial_run ? init_fns : tick_fns;

	bool halt = false;

	while (!halt)
	{
		num instr = NUM(CAR(pc));

		switch (instr)
		{
		default:
		{ // error
			engine_error("RIBBIT-VM", "Error in Lisp code: %d",
			             instr);
			break;
		}
		case INSTR_HALT:
		{
			halt = true;
			break;
		}
		case INSTR_AP:
		{
			bool jump = TAG(pc) == NUM_0;
			obj proc = get_opnd(CDR(pc));

#define code CAR(proc)
			num nargs = NUM(pop()); // @@(feature arity-check)@@

			if (IS_NUM(code))
			{
				prim(NUM(code));

				if (jump)
				{
					pc = get_cont();
					CDR(stack) = CAR(pc);
				}

				ADVANCE_PC;
			}
			else
			{
				num nparams = NUM(CAR(code)) >> 1;

				obj s2 =
				    TAG_RIB(alloc_rib(NUM_0, proc, PAIR_TAG));
				CAR(pc) = CAR(proc);

				// @@(feature arity-check
				num vari = NUM(CAR(code)) & 1;
				if ((!vari && nparams != nargs) ||
				    (vari && nparams > nargs))
				{
					engine_error("RIBBIT",
					             "Unexpected number of "
					             "arguments nargs: %ld "
					             "nparams: %ld vari: %ld\n",
					             nargs, nparams, vari);
				}
				// )@@

				// @@(feature rest-param (use arity-check)
				nargs -= nparams;
				if (vari)
				{
					obj rest = NIL;
					for (int i = 0; i < nargs; ++i)
					{
						rest = TAG_RIB(alloc_rib(
						    pop(), rest, PAIR_TAG));
					}
					s2 = TAG_RIB(
					    alloc_rib(rest, s2, PAIR_TAG));
				}
				// )@@

				for (int i = 0; i < nparams; ++i)
				{
					s2 = TAG_RIB(
					    alloc_rib(pop(), s2, PAIR_TAG));
				}

				obj c2 = TAG_RIB(list_tail(RIB(s2), nparams));

				if (jump)
				{
					obj k = get_cont();
					CAR(c2) = CAR(k);
					TAG(c2) = TAG(k);
				}
				else
				{
					CAR(c2) = stack;
					TAG(c2) = TAG(pc);
				}

				stack = s2;

				obj new_pc = CAR(pc);
				CAR(pc) = TAG_NUM(instr);
				pc = TAG(new_pc);
			}
			break;
		}
#undef code
		case INSTR_SET:
		{
			obj x = CAR(stack);
			((IS_NUM(CDR(pc))) ? list_tail(RIB(stack), NUM(CDR(pc)))
			                   : RIB(CDR(pc)))
			    ->fields[0] = x;
			stack = CDR(stack);
			ADVANCE_PC;
			break;
		}
		case INSTR_GET:
		{
			push2(get_opnd(CDR(pc)), PAIR_TAG);
			ADVANCE_PC;
			break;
		}
		case INSTR_CONST:
		{
			push2(CDR(pc), PAIR_TAG);
			ADVANCE_PC;
			break;
		}
		case INSTR_IF:
		{
			obj p = pop();

			if (p != FALSE)
			{
				pc = CDR(pc);
			}
			else
			{
				pc = TAG(pc);
			}

			break;
		}
		}
	}
}

#undef ADVANCE_PC
