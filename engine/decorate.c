// kgsws' ACE Engine
////
// Subset of DECORATE. Yeah!
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "dehacked.h"
#include "decorate.h"
#include "action.h"
#include "textpars.h"
#include "sound.h"

#include "decodoom.h"

#define NUM_INFO_HOOKS	3
#define NUM_STATE_HOOKS	7

enum
{
	DT_U16,
	DT_S32,
	DT_FIXED,
	DT_SOUND,
	DT_SKIP1,
	DT_MONSTER,
	DT_PROJECTILE,
	DT_STATES
};

typedef struct
{
	uint8_t *name;
	uint16_t type;
	uint16_t offset;
} dec_attr_t;

typedef struct
{
	uint8_t *name;
	uint32_t bits;
} dec_flag_t;

typedef struct
{
	uint8_t *name;
	uint32_t offset;
} dec_anim_t;

//

uint32_t mobj_netid;

uint32_t num_spr_names = 138;
uint32_t sprite_table[MAX_SPRITE_NAMES];

static uint32_t **spr_names;

uint32_t num_mobj_types = NUMMOBJTYPES;
mobjinfo_t *mobjinfo;

uint32_t num_states = NUMSTATES;
state_t *states;

static hook_t hook_mobjinfo[NUM_INFO_HOOKS];
static hook_t hook_states[NUM_STATE_HOOKS];

uint8_t *parse_actor_name;

static void *parse_mobj_ptr;
static uint32_t parse_first_state;
static uint32_t *parse_next_state;
static uint32_t parse_last_anim;

static uint32_t parse_states();

// default mobj
static const mobjinfo_t default_mobj =
{
	.doomednum = 0xFFFF,
	.spawnhealth = 1000,
	.reactiontime = 8,
	.radius = 20 << FRACBITS,
	.height = 16 << FRACBITS,
	.mass = 100,
};

// mobj animations
static const dec_anim_t mobj_anim[NUM_MOBJ_ANIMS] =
{
	[ANIM_SPAWN] = {"spawn", offsetof(mobjinfo_t, spawnstate)},
	[ANIM_SEE] = {"see", offsetof(mobjinfo_t, seestate)},
	[ANIM_PAIN] = {"pain", offsetof(mobjinfo_t, painstate)},
	[ANIM_MELEE] = {"melee", offsetof(mobjinfo_t, meleestate)},
	[ANIM_MISSILE] = {"missile", offsetof(mobjinfo_t, missilestate)},
	[ANIM_DEATH] = {"death", offsetof(mobjinfo_t, deathstate)},
	[ANIM_XDEATH] = {"xdeath", offsetof(mobjinfo_t, xdeathstate)},
	[ANIM_RAISE] = {"raise", offsetof(mobjinfo_t, raisestate)},
};

// mobj attributes
static const dec_attr_t mobj_attr[] =
{
	{"spawnid", DT_U16, offsetof(mobjinfo_t, spawnid)},
	//
	{"health", DT_S32, offsetof(mobjinfo_t, spawnhealth)},
	{"reactiontime", DT_S32, offsetof(mobjinfo_t, reactiontime)},
	{"painchance", DT_S32, offsetof(mobjinfo_t, painchance)},
	{"damage", DT_S32, offsetof(mobjinfo_t, damage)},
	{"speed", DT_FIXED, offsetof(mobjinfo_t, speed)},
	//
	{"radius", DT_FIXED, offsetof(mobjinfo_t, radius)},
	{"height", DT_FIXED, offsetof(mobjinfo_t, height)},
	{"mass", DT_S32, offsetof(mobjinfo_t, mass)},
	//
	{"activesound", DT_SOUND, offsetof(mobjinfo_t, activesound)},
	{"attacksound", DT_SOUND, offsetof(mobjinfo_t, attacksound)},
	{"deathsound", DT_SOUND, offsetof(mobjinfo_t, deathsound)},
	{"painsound", DT_SOUND, offsetof(mobjinfo_t, painsound)},
	{"seesound", DT_SOUND, offsetof(mobjinfo_t, seesound)},
	//
	{"monster", DT_MONSTER},
	{"projectile", DT_PROJECTILE},
	{"states", DT_STATES},
	//
	{"obituary", DT_SKIP1},
	{"hitobituary", DT_SKIP1},
	// terminator
	{NULL}
};

// mobj flags
static const dec_flag_t mobj_flag[] =
{
	// ignored flags
	{"floorclip", 0},
	{"noskin", 0},
	// flags 0
	{"special", MF_SPECIAL},
	{"solid", MF_SOLID},
	{"shootable", MF_SHOOTABLE},
	{"nosector", MF_NOSECTOR},
	{"noblockmap", MF_NOBLOCKMAP},
	{"ambush", MF_AMBUSH},
	{"justhit", MF_JUSTHIT},
	{"justattacked", MF_JUSTATTACKED},
	{"spawnceiling", MF_SPAWNCEILING},
	{"nogravity", MF_NOGRAVITY},
	{"dropoff", MF_DROPOFF},
	{"pickup", MF_PICKUP},
	{"noclip", MF_NOCLIP},
	{"slidesonwalls", MF_SLIDE},
	{"float", MF_FLOAT},
	{"teleport", MF_TELEPORT},
	{"missile", MF_MISSILE},
	{"dropped", MF_DROPPED},
	{"shadow", MF_SHADOW},
	{"noblood", MF_NOBLOOD},
	{"corpse", MF_CORPSE},
	{"countkill", MF_COUNTKILL},
	{"countitem", MF_COUNTITEM},
	{"skullfly", MF_SKULLFLY},
	{"notdmatch", MF_NOTDMATCH},
	// flags 1
	{"", 0}, // this is a separator
//	{"noteleport", MF1_NOTELEPORT},
//	{"ismonster", MF1_ISMONSTER},
	// terminator
	{NULL}
};

// destination of different flags in mobjinfo_t
static const uint32_t flag_dest[] =
{
	offsetof(mobjinfo_t, flags0),
	offsetof(mobjinfo_t, flags1),
};

//
// funcs

static uint32_t spr_add_name(uint32_t name)
{
	uint32_t i = 0;

	for(i = 0; i < num_spr_names; i++)
	{
		if(sprite_table[i] == name)
			break;
	}

	if(i == MAX_SPRITE_NAMES)
		I_Error("[DECORATE] Too many sprite names!");

	return i;
}

static uint32_t parse_attr(uint32_t type, void *dest)
{
	num32_t num;
	uint8_t *kw;

	switch(type)
	{
		case DT_U16:
			kw = tp_get_keyword();
			if(!kw)
				return 1;
			if(doom_sscanf(kw, "%u", &num.u32) != 1 || num.u32 > 65535)
				return 1;
			*((uint16_t*)dest) = num.u32;
		break;
		case DT_S32:
			kw = tp_get_keyword();
			if(!kw)
				return 1;
			if(doom_sscanf(kw, "%d", num.s32) != 1)
				return 1;
			*((int32_t*)dest) = num.s32[0];
		break;
		case DT_FIXED:
			kw = tp_get_keyword();
			if(!kw)
				return 1;
			if(tp_parse_fixed(kw, num.s32))
				return 1;
			*((fixed_t*)dest) = num.s32[0];
		break;
		case DT_SOUND:
			kw = tp_get_keyword();
			if(!kw)
				return 1;
			*((uint16_t*)dest) = sfx_by_name(kw);
		break;
		case DT_SKIP1:
			kw = tp_get_keyword();
			if(!kw)
				return 1;
		break;
		case DT_MONSTER:
			((mobjinfo_t*)dest)->flags0 |= MF_SHOOTABLE | MF_COUNTKILL | MF_SOLID;
			// TODO: flags1
		break;
		case DT_PROJECTILE:
			((mobjinfo_t*)dest)->flags0 |= MF_NOBLOCKMAP | MF_NOGRAVITY | MF_DROPOFF | MF_MISSILE;
			// TODO: flags1
		break;
		case DT_STATES:
			num.u32 = parse_states();
			tp_enable_math = 0;
			tp_enable_newline = 0;
			return num.u32;
		break;
	}

	return 0;
}

static int32_t check_add_sprite(uint8_t *text)
{
	// make lowercase, get length
	uint32_t len = 0;
	num32_t sprname;

	while(1)
	{
		uint8_t in = *text;

		if(!in)
		{
			if(len != 4)
				return -1;
			break;
		}

		if(in >= 'a' && in <= 'z')
			sprname.u8[len] = in & ~0x20;
		else
			sprname.u8[len] = in;

		text++;
		len++;
	}

	return spr_add_name(sprname.u32);
}

static uint32_t add_states(uint32_t sprite, uint8_t *frames, int32_t tics, uint16_t flags)
{
	uint32_t tmp = num_states;

	if(!parse_first_state)
		parse_first_state = num_states;

	num_states += strlen(frames);
	states = ldr_realloc(states, num_states * sizeof(state_t));

	for(uint32_t i = tmp; i < num_states; i++)
	{
		state_t *state = states + i;
		uint8_t frm;

		if(*frames < 'A' || *frames > ']')
			I_Error("[DECORATE] Invalid frame '%c' in '%s'!", *frames, parse_actor_name);

		if(parse_next_state)
			*parse_next_state = state - states;

		state->sprite = sprite;
		state->frame = (*frames - 'A') | flags;
		state->arg = parse_action_arg;
		state->tics = tics;
		state->action = parse_action_func;
		state->nextstate = parse_first_state;
		state->misc1 = 0;
		state->misc2 = 0;

		parse_next_state = &state->nextstate;

		frames++;
		state++;
	}
}

//
// state parser

static uint32_t parse_states()
{
	uint8_t *kw;
	uint8_t *wk;
	int32_t animation = -1;
	int32_t sprite;
	int32_t tics;
	uint16_t flags;
	uint_fast8_t unfinished;

	kw = tp_get_keyword();
	if(!kw || kw[0] != '{')
		return 1;

	parse_last_anim = 0;
	parse_first_state = 0;
	parse_next_state = NULL;
	unfinished = 0;

	while(1)
	{
		kw = tp_get_keyword_lc();
		if(!kw)
			return 1;

		if(kw[0] == '}')
			break;

		if(!strcmp(kw, "loop"))
		{
			if(!parse_next_state)
				goto error_no_states;
			*parse_next_state = parse_last_anim;
			parse_next_state = NULL;
			continue;
		} else
		if(!strcmp(kw, "stop"))
		{
			if(!parse_next_state)
				goto error_no_states;
			*parse_next_state = 0;
			parse_next_state = NULL;
			continue;
		} else
		if(!strcmp(kw, "wait"))
		{
			if(!parse_next_state)
				goto error_no_states;
			*parse_next_state = num_states - 1;
			parse_next_state = NULL;
			continue;
		}
		if(!strcmp(kw, "fail"))
		{
			if(!parse_next_state)
				goto error_no_states;
			*parse_next_state = 0;
			parse_next_state = NULL;
			continue;
		} else
		if(!strcmp(kw, "goto"))
		{
			uint32_t i;

			if(!parse_next_state)
				goto error_no_states;

			// get state name
			kw = tp_get_keyword_lc();
			if(!kw)
				return 1;

			// find animation
			for(i = 0; i < NUM_MOBJ_ANIMS; i++)
			{
				if(!strcmp(kw, mobj_anim[i].name))
					break;
			}
			if(i >= NUM_MOBJ_ANIMS)
				I_Error("[DECORATE] Unknown animation '%s' in '%s'!", kw, parse_actor_name);

			// TODO: support math
			// TODO: animation 'system'
			*parse_next_state = *((uint32_t*)(parse_mobj_ptr + mobj_anim[i].offset));

			parse_next_state = NULL;
			continue;
		}

		// this is either sprite or state name
		wk = tp_get_keyword_uc();
		if(!wk)
			return 1;

		if(wk[0] == ':')
		{
			// it's a new state
			uint32_t i;

			for(i = 0; i < NUM_MOBJ_ANIMS; i++)
			{
				if(!strcmp(kw, mobj_anim[i].name))
					break;
			}
			if(i >= NUM_MOBJ_ANIMS)
				I_Error("[DECORATE] Unknown animation '%s' in '%s'!", kw, parse_actor_name);

			animation = i;
			parse_last_anim = num_states;
			*((uint32_t*)(parse_mobj_ptr + mobj_anim[i].offset)) = num_states;

			unfinished = 1;
			continue;
		}

		unfinished = 0;

		// it's a sprite
		sprite = check_add_sprite(kw);
		if(sprite < 0)
			I_Error("[DECORATE] Sprite name has wrong length in '%s'!", kw, parse_actor_name);

		// switch to line-based parsing
		tp_enable_newline = 1;
		// reset frame stuff
		flags = 0;
		parse_action_func = NULL;
		parse_action_arg = NULL;

		// get ticks
		kw = tp_get_keyword();
		if(!kw || kw[0] == '\n')
			return 1;

		if(doom_sscanf(kw, "%d", &tics) != 1)
			I_Error("[DECORATE] Unable to parse number '%s' in '%s'!", kw, parse_actor_name);

		// optional 'bright' or action
		kw = tp_get_keyword_lc();
		if(!kw)
			return 1;

		if(!strcmp(kw, "bright"))
		{
			// optional action
			kw = tp_get_keyword_lc();
			if(!kw)
				return 1;
			// set the flag
			flags |= 0x8000;
		}

		if(kw[0] != '\n')
		{
			// action
			kw = action_parser(kw);
			if(!kw)
				return 1;
		}

		// create states
		add_states(sprite, wk, tics, flags);

		if(kw[0] != '\n')
			I_Error("[DECORATE] Expected end of line, found '%s' in '%s'!", kw, parse_actor_name);

		// next
		tp_enable_newline = 0;
	}

	// sanity check
	if(unfinished)
		I_Error("[DECORATE] Unfinised animation in '%s'!", parse_actor_name);

	// create a loopback
	// this is how ZDoom behaves
	if(parse_next_state)
		*parse_next_state = parse_first_state;

	return 0;

error_no_states:
	I_Error("[DECORATE] Missing states for '%s' in '%s'!", kw, parse_actor_name);
}

//
// callbacks

static void cb_count_actors(lumpinfo_t *li)
{
	uint32_t idx;
	uint8_t *kw;
	uint64_t alias;

	tp_load_lump(li);

	while(1)
	{
		kw = tp_get_keyword_lc();
		if(!kw)
			return;

		// must get 'actor'
		if(strcmp(kw, "actor"))
			I_Error("[DECORATE] Expected 'actor', got '%s'!", kw);

		// actor name, case sensitive
		kw = tp_get_keyword();
		if(!kw)
			goto error_end;
		parse_actor_name = kw;

		// skip optional stuff
		while(1)
		{
			kw = tp_get_keyword();
			if(!kw)
				goto error_end;

			if(kw[0] == '{')
				break;
		}

		if(tp_skip_code_block(1))
			goto error_end;

		// check for duplicate
		alias = tp_hash64(parse_actor_name);
		if(mobj_check_type(alias) >= 0)
			I_Error("[DECORATE] Multiple definitions of '%s'!", parse_actor_name);

		// add new type
		idx = num_mobj_types;
		num_mobj_types++;
		mobjinfo = ldr_realloc(mobjinfo, num_mobj_types * sizeof(mobjinfo_t));
		mobjinfo[idx].actor_name = alias;
	}

error_end:
	I_Error("[DECORATE] Incomplete definition!");
}

static void cb_parse_actors(lumpinfo_t *li)
{
	int32_t idx;
	uint8_t *kw;
	mobjinfo_t *info;
	uint64_t alias;

	tp_load_lump(li);

	while(1)
	{
		kw = tp_get_keyword_lc();
		if(!kw)
			return;

		// must get 'actor'
		if(strcmp(kw, "actor"))
			I_Error("[DECORATE] Expected 'actor', got '%s'!", kw);

		// actor name, case sensitive
		kw = tp_get_keyword();
		if(!kw)
			goto error_end;
		parse_actor_name = kw;

		// find mobj
		alias = tp_hash64(kw);
		idx = mobj_check_type(alias);
		if(idx < 0)
			I_Error("[DECORATE] Loading mismatch for '%s'!", kw);
		info = mobjinfo + idx;
		parse_mobj_ptr = info;

		// next, a few options
		kw = tp_get_keyword();
		if(!kw)
			goto error_end;

		if(kw[0] != '{')
		{
			// editor number
			if(doom_sscanf(kw, "%u", &idx) != 1 || (uint32_t)idx > 65535)
				goto error_numeric;

			// next keyword
			kw = tp_get_keyword();
			if(!kw)
				goto error_end;
		}

		if(kw[0] != '{')
			I_Error("[DECORATE] Expected '{', got '%s'!", kw);

		// initialize mobj
		memcpy(info, &default_mobj, sizeof(mobjinfo_t));
		info->doomednum = idx;
		info->actor_name = alias;

		// parse attributes
		while(1)
		{
			kw = tp_get_keyword_lc();
			if(!kw)
				goto error_end;

			// check for end
			if(kw[0] == '}')
				break;

			// check for flags
			if(kw[0] == '-' || kw[0] == '+')
			{
				const dec_flag_t *flag = mobj_flag;
				uint32_t *dest_offs = (uint32_t*)flag_dest;

				// find flag
				while(flag->name)
				{
					if(!flag->name[0])
					{
						// advance destination pointer
						dest_offs++;
					} else
					{
						if(!strcmp(flag->name, kw + 1))
							break;
					}
					flag++;
				}

				if(!flag->name)
					I_Error("[DECORATE] Unknown flag '%s'!", kw + 1);

				// destination
				dest_offs = (uint32_t*)((void*)info + *dest_offs);

				// change
				if(kw[0] == '+')
					*dest_offs |= flag->bits;
				else
					*dest_offs &= ~flag->bits;
			} else
			{
				const dec_attr_t *attr = mobj_attr;

				// find attribute
				while(attr->name)
				{
					if(!strcmp(attr->name, kw))
						break;
					attr++;
				}

				if(!attr->name)
					I_Error("[DECORATE] Unknown attribute '%s' in '%s'!", kw, parse_actor_name);

				// parse it
				if(parse_attr(attr->type, (void*)info + attr->offset))
					I_Error("[DECORATE] Unable to parse value of '%s' in '%s'!", kw, parse_actor_name);
			}
		}
	}

error_end:
	I_Error("[DECORATE] Incomplete definition!");
error_numeric:
	I_Error("[DECORATE] Unable to parse number '%s' in '%s'!", kw, parse_actor_name);
}

//
// API

int32_t mobj_check_type(uint64_t alias)
{
	for(uint32_t i = 0; i < num_mobj_types; i++)
	{
		if(mobjinfo[i].actor_name == alias)
			return i;
	}

	return -1;
}

void init_decorate()
{
	// To avoid unnecessary memory fragmentation, this function does multiple passes.
	doom_printf("[ACE] init DECORATE\n");
	ldr_alloc_message = "Decorate memory allocation failed!";

	// init sprite names // TODO: TNT1
	for(uint32_t i = 0; i < num_spr_names; i++)
		sprite_table[i] = *spr_names[i];

	// mobjinfo
	mobjinfo = ldr_malloc(NUMMOBJTYPES * sizeof(mobjinfo_t));

	// copy original stuff
	for(uint32_t i = 0; i < NUMMOBJTYPES; i++)
	{
		memset(mobjinfo + i, 0, sizeof(mobjinfo_t));
		memcpy(mobjinfo + i, deh_mobjinfo + i, sizeof(deh_mobjinfo_t));
		mobjinfo[i].actor_name = doom_actor_name[i];
		mobjinfo[i].spawnid = doom_spawn_id[i];
	}

	//
	// PASS 1

	// count actors, allocate actor names
	wad_handle_lump("DECORATE", cb_count_actors);

	//
	// PASS 2

	// states
	states = ldr_malloc(NUMSTATES * sizeof(state_t));

	// copy original stuff
	for(uint32_t i = 0; i < NUMSTATES; i++)
	{
		states[i].sprite = deh_states[i].sprite;
		states[i].frame = deh_states[i].frame;
		states[i].arg = NULL;
		states[i].tics = deh_states[i].tics;
		states[i].action = deh_states[i].action;
		states[i].nextstate = deh_states[i].nextstate;
		states[i].misc1 = deh_states[i].misc1;
		states[i].misc2 = deh_states[i].misc2;
	}

	// process actors
	wad_handle_lump("DECORATE", cb_parse_actors);

	//
	// DONE

	// patch CODE - 'mobjinfo'
	for(uint32_t i = 0; i < NUM_INFO_HOOKS; i++)
		hook_mobjinfo[i].value += (uint32_t)mobjinfo;
	utils_install_hooks(hook_mobjinfo, NUM_INFO_HOOKS);

	// patch CODE - 'states'
	for(uint32_t i = 0; i < NUM_STATE_HOOKS; i++)
		hook_states[i].value += (uint32_t)states;
	utils_install_hooks(hook_states, NUM_STATE_HOOKS);
}

//
// hooks

static __attribute((regparm(2),no_caller_saved_registers))
void prepare_mobj(mobj_t *mo)
{
	mo->flags1 = mo->info->flags1;
	mo->netid = mobj_netid++;
	P_SetThingPosition(mo);
}

static __attribute((regparm(2),no_caller_saved_registers))
void touch_mobj(mobj_t *mo, mobj_t *toucher)
{
	// old items; workaround for 'sprite' changes in 'mobj_t'
	uint16_t temp = mo->frame;
	mo->frame = 0;
	P_TouchSpecialThing(mo, toucher);
	mo->frame = temp;
}

//
// hooks

static hook_t hook_mobjinfo[NUM_INFO_HOOKS] =
{
	{0x00031587, CODE_HOOK | HOOK_UINT32, 0}, // P_SpawnMobj
	{0x00031792, CODE_HOOK | HOOK_UINT32, 0x55}, // P_RespawnSpecials
	{0x00030F1E, CODE_HOOK | HOOK_UINT32, offsetof(mobjinfo_t, deathstate)}, // P_ExplodeMissile // TODO: animation
};

static hook_t hook_states[NUM_STATE_HOOKS] =
{
	{0x0002A72F, CODE_HOOK | HOOK_UINT32, 0}, // P_DamageMobj
	{0x0002D062, CODE_HOOK | HOOK_UINT32, 0}, // P_SetPsprite
	{0x00030EC0, CODE_HOOK | HOOK_UINT32, 0}, // P_SetMobjState
	{0x000315D9, CODE_HOOK | HOOK_UINT32, 0}, // P_SpawnMobj
	{0x0002D2FF, CODE_HOOK | HOOK_UINT32, 0x10D8}, // A_WeaponReady
	{0x0002D307, CODE_HOOK | HOOK_UINT32, 0x10F4}, // A_WeaponReady
	{0x0002D321, CODE_HOOK | HOOK_UINT32, 0x0754}, // A_WeaponReady
};

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// import variables
	{0x00015800, DATA_HOOK | HOOK_IMPORT, (uint32_t)&spr_names},
	// replace call to 'P_SetThingPosition' in 'P_SpawnMobj'
	{0x000315FC, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)prepare_mobj},
	// replace call to 'P_TouchSpecialThing' in 'PIT_CheckThing'
	{0x0002B031, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)touch_mobj},
	// change 'mobj_t' size
	{0x00031552, CODE_HOOK | HOOK_UINT32, sizeof(mobj_t)},
	{0x00031563, CODE_HOOK | HOOK_UINT32, sizeof(mobj_t)},
	// change 'mobjinfo_t' size
	{0x00031574, CODE_HOOK | HOOK_UINT8, sizeof(mobjinfo_t)},
	{0x0003178F, CODE_HOOK | HOOK_UINT8, sizeof(mobjinfo_t)},
	{0x00030F10, CODE_HOOK | HOOK_UINT8, sizeof(mobjinfo_t)},
	// change 'sprite' of 'mobj_t' in 'R_PrecacheLevel'
	{0x00034992, CODE_HOOK | HOOK_UINT32, 0x2443B70F},
	{0x00034996, CODE_HOOK | HOOK_UINT32, 0x8B012488},
	// remove old 'frame' set in 'P_SetMobjState'
	{0x00030ED2, CODE_HOOK | HOOK_SET_NOPS, 6},
	// fix 'R_ProjectSprite'; use new 'frame' and 'state', ignore invalid sprites
	{0x00037D45, CODE_HOOK | HOOK_UINT32, 0x2446B70F},
	{0x00037D49, CODE_HOOK | HOOK_UINT32, 0x1072E839},
	{0x00037D4D, CODE_HOOK | HOOK_JMP_DOOM, 0x00037F86},
	{0x00037D6C, CODE_HOOK | HOOK_UINT32, 0x2656B70F},
	{0x00037D70, CODE_HOOK | HOOK_UINT32, 0xE283038B},
	{0x00037D74, CODE_HOOK | HOOK_UINT32, 0x7CC2393F},
	{0x00037D78, CODE_HOOK | HOOK_UINT8, 0x1F},
	{0x00037D79, CODE_HOOK | HOOK_JMP_DOOM, 0x00037F86},
	{0x00037F59, CODE_HOOK | HOOK_UINT8, 0x27},
	// fix 'R_DrawPSprite'; use new 'frame' and 'state', ignore invalid sprites
	{0x00038023, CODE_HOOK | HOOK_UINT32, 0x3910B70F},
	{0x00038027, CODE_HOOK | HOOK_UINT32, 0x1172DA},
	{0x0003802A, CODE_HOOK | HOOK_JMP_DOOM, 0x00038203},
	{0x0003804A, CODE_HOOK | HOOK_UINT32, 0x0258B70F},
	{0x0003804E, CODE_HOOK | HOOK_UINT32, 0xE3833A8B},
	{0x00038052, CODE_HOOK | HOOK_UINT32, 0x7CFB393F},
	{0x00038056, CODE_HOOK | HOOK_UINT8, 0x21},
	{0x00038057, CODE_HOOK | HOOK_JMP_DOOM, 0x00038203},
	{0x000381DD, CODE_HOOK | HOOK_UINT8, 0x03},
};

