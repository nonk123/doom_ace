// kgsws' ACE Engine
////
// TERRAIN lump
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "render.h"
#include "wadfile.h"
#include "textpars.h"
#include "ldr_flat.h"
#include "decorate.h"
#include "sound.h"
#include "terrain.h"

// these must fit into 12288 bytes; 2 * 255 * 8 = 4080
#define TERRAIN_STATE_STORAGE	((custom_state_t*)ptr_drawsegs)
#define MAX_NAMES	(12288 / sizeof(uint64_t))
#define MAX_SPLASH_COUNT	255
#define MAX_TERRAIN_COUNT	255
#define terrain_alias	((uint64_t*)TERRAIN_STATE_STORAGE)
#define splash_alias	((uint64_t*)TERRAIN_STATE_STORAGE + MAX_TERRAIN_COUNT)

//

enum
{
	IT_U8,
	IT_U16,
	IT_FIXED,
	IT_SOUND,
	IT_MOBJ_TYPE,
	IT_SPLASH,
	IT_DAMAGE_TYPE,
	IT_FLAG,
};

typedef struct
{
	const uint8_t *name;
	uint32_t offset;
	uint8_t flag;
	uint8_t type;
} attr_t;

//

uint32_t num_terrain_splash;
terrain_splash_t *terrain_splash;
uint32_t num_terrain;
terrain_terrain_t *terrain;

uint8_t *flatterrain;

static const terrain_splash_t default_splash =
{
	.sx = 8,
	.sy = 8,
	.sz = 8,
	.bz = 1 * FRACUNIT,
	.smallclip = 12,
};

// splash attributes
static const attr_t splash_attr[] =
{
	{.name = "smallclass", .type = IT_MOBJ_TYPE, .offset = offsetof(terrain_splash_t, smallclass)},
	{.name = "smallsound", .type = IT_SOUND, .offset = offsetof(terrain_splash_t, smallsound)},
	{.name = "smallclip", .type = IT_U8, .offset = offsetof(terrain_splash_t, smallclip)},
	{.name = "baseclass", .type = IT_MOBJ_TYPE, .offset = offsetof(terrain_splash_t, smallclass)},
	{.name = "chunkclass", .type = IT_MOBJ_TYPE, .offset = offsetof(terrain_splash_t, smallclass)},
	{.name = "chunkxvelshift", .type = IT_U8, .offset = offsetof(terrain_splash_t, sx)},
	{.name = "chunkyvelshift", .type = IT_U8, .offset = offsetof(terrain_splash_t, sy)},
	{.name = "chunkzvelshift", .type = IT_U8, .offset = offsetof(terrain_splash_t, sz)},
	{.name = "chunkbasezvel", .type = IT_FIXED, .offset = offsetof(terrain_splash_t, bz)},
	{.name = "sound", .type = IT_SOUND, .offset = offsetof(terrain_splash_t, sound)},
	{.name = "noalert", .type = IT_FLAG, .offset = offsetof(terrain_splash_t, flags), .flag = TRN_SPLASH_NOALERT},
	// terminator
	{.name = NULL}
};
// terrain attributes
static const attr_t terrain_attr[] =
{
	{.name = "splash", .type = IT_SPLASH, .offset = offsetof(terrain_terrain_t, splash)},
	{.name = "friction", .type = IT_FIXED, .offset = offsetof(terrain_terrain_t, friction)},
	{.name = "damagetype", .type = IT_DAMAGE_TYPE, .offset = offsetof(terrain_terrain_t, damagetype)},
	{.name = "damageamount", .type = IT_U16, .offset = offsetof(terrain_terrain_t, damageamount)},
	{.name = "damagetimemask", .type = IT_U8, .offset = offsetof(terrain_terrain_t, damagetimemask)},
	{.name = "liquid", .type = IT_FLAG, .offset = offsetof(terrain_terrain_t, flags), .flag = TRN_SPLASH_NOALERT},
	{.name = "allowprotection", .type = IT_FLAG, .offset = offsetof(terrain_terrain_t, flags), .flag = TRN_FLAG_PROTECT},
	// terminator
	{.name = NULL}
};

//
// funcs

static uint32_t find_splash(uint8_t *name)
{
	uint64_t alias = tp_hash64(name);

	for(uint32_t i = 0; i < num_terrain_splash; i++)
	{
		if(splash_alias[i] == alias)
			return i;
	}

	engine_error("TERRAIN", "Unknown splash %s!", name);
}

static uint32_t find_terrain(uint8_t *name)
{
	uint64_t alias = tp_hash64(name);

	for(uint32_t i = 0; i < num_terrain; i++)
	{
		if(terrain_alias[i] == alias)
			return i;
	}

	engine_error("TERRAIN", "Unknown terrain %s!", name);
}

//
// parser

static uint32_t parse_attributes(const attr_t *attr_def, void *dest)
{
	const attr_t *attr;
	uint8_t *kw, *kv;
	union
	{
		uint32_t u32;
		int32_t s32;
	} value;

	kw = tp_get_keyword();
	if(!kw)
		return 1;

	if(kw[0] != '{')
		engine_error("TERRAIN", "Expected '%c' found '%s'!", '{', kw);

	while(1)
	{
		// get attribute name
		kw = tp_get_keyword_lc();
		if(!kw)
			return 1;

		if(kw[0] == '}')
			return 0;

		// find this attribute
		attr = attr_def;
		while(attr->name)
		{
			if(!strcmp(attr->name, kw))
				break;
			attr++;
		}

		if(!attr->name)
			engine_error("TERRAIN", "Unknown attribute '%s'!", kw);

		if(attr->type != IT_FLAG)
		{
			kv = tp_get_keyword();
			if(!kv)
				return 1;
		}

		switch(attr->type)
		{
			case IT_U8:
				if(doom_sscanf(kv, "%u", &value.u32) != 1 || value.u32 > 255)
					engine_error("TERRAIN", "Unable to parse number '%s'!", kv);
				*((uint8_t*)(dest + attr->offset)) = value.u32;
			break;
			case IT_U16:
				if(doom_sscanf(kv, "%u", &value.u32) != 1 || value.u32 > 65535)
					engine_error("TERRAIN", "Unable to parse number '%s'!", kv);
				*((uint16_t*)(dest + attr->offset)) = value.u32;
			break;
			case IT_FIXED:
				if(tp_parse_fixed(kv, &value.s32))
					engine_error("TERRAIN", "Unable to parse number '%s'!", kv);
				*((fixed_t*)(dest + attr->offset)) = value.s32;
			break;
			case IT_SOUND:
				strlwr(kv);
				*((uint16_t*)(dest + attr->offset)) = sfx_by_name(kv);
			break;
			case IT_MOBJ_TYPE:
				value.s32 = mobj_check_type(tp_hash64(kv));
				if(value.s32 < 0)
					value.s32 = 0;
				*((uint16_t*)(dest + attr->offset)) = value.s32;
			break;
			case IT_SPLASH:
				strlwr(kv);
				*((uint8_t*)(dest + attr->offset)) = find_splash(kv);
			break;
			case IT_DAMAGE_TYPE:
				strlwr(kv);
				*((uint8_t*)(dest + attr->offset)) = dec_get_custom_damage(kv, "TERRAIN");
			break;
			case IT_FLAG:
				*((uint8_t*)(dest + attr->offset)) |= attr->flag;
			break;
		}
	}
}

//
// lump callbacks

static void cb_count_stuff(lumpinfo_t *li)
{
	uint8_t *kw;

	tp_load_lump(li);

	while(1)
	{
		kw = tp_get_keyword_lc();
		if(!kw)
			return;

		if(!strcmp("splash", kw))
		{
			// name
			kw = tp_get_keyword_lc();
			if(!kw)
				break;

			if(num_terrain_splash >= MAX_TERRAIN_COUNT)
				engine_error("TERRAIN", "Too many %s definitions!", "splash");

			splash_alias[num_terrain_splash++] = tp_hash64(kw);

			goto skip_block;
		}

		if(!strcmp("terrain", kw))
		{
			// name
			kw = tp_get_keyword_lc();
			if(!kw)
				break;

			if(num_terrain >= MAX_TERRAIN_COUNT)
				engine_error("TERRAIN", "Too many %s definitions!", "terrain");

			terrain_alias[num_terrain++] = tp_hash64(kw);

skip_block:
			// block entry
			kw = tp_get_keyword();
			if(!kw)
				break;

			if(kw[0] != '{')
				engine_error("TERRAIN", "Broken syntax!");

			if(tp_skip_code_block(1))
				break;

			continue;
		}

		if(!strcmp("floor", kw))
		{
			// flat name
			kw = tp_get_keyword();
			if(!kw)
				break;

			// terrain name
			kw = tp_get_keyword();
			if(!kw)
				break;

			continue;
		}

		engine_error("TERRAIN", "Unknown block '%s'!", kw);
	}
}

static void cb_terrain(lumpinfo_t *li)
{
	uint8_t *kw;

	tp_load_lump(li);

	while(1)
	{
		kw = tp_get_keyword_lc();
		if(!kw)
			return;

		if(!strcmp("splash", kw))
		{
			// name
			kw = tp_get_keyword();
			if(!kw)
				break;

			// defaults
			terrain_splash[num_terrain_splash] = default_splash;

			// get properties
			if(parse_attributes(splash_attr, terrain_splash + num_terrain_splash))
				break;

			num_terrain_splash++;

			continue;
		}

		if(!strcmp("terrain", kw))
		{
			// name
			kw = tp_get_keyword();
			if(!kw)
				break;

			terrain[num_terrain].splash = 255;

			// get properties
			if(parse_attributes(terrain_attr, terrain + num_terrain))
				break;

			num_terrain++;

			continue;
		}

		if(!strcmp("floor", kw))
		{
			uint32_t flat;

			// flat name
			kw = tp_get_keyword();
			if(!kw)
				break;

			flat = flat_num_get(kw);
			if(flat >= numflats + num_texture_flats)
				engine_error("TERRAIN", "Unknown flat %s!", kw);

			// terrain name
			kw = tp_get_keyword_lc();
			if(!kw)
				break;

			flatterrain[flat] = find_terrain(kw);

			continue;
		}

		engine_error("TERRAIN", "Unknown block '%s'!", kw);
	}
}

//
// API

void init_terrain()
{
	void *ptr;
	uint32_t size;

	ldr_alloc_message = "Terrain";

	//
	// PASS 1

	wad_handle_lump("TERRAIN", cb_count_stuff);

	if(!num_terrain)
		// no terrain definitions
		return;

	// terrain detection
	flatterrain = ldr_malloc(numflats + num_texture_flats);
	memset(flatterrain, 0xFF, numflats + num_texture_flats);

	size = num_terrain * sizeof(terrain_terrain_t) + num_terrain_splash * sizeof(terrain_splash_t);
	ptr = ldr_malloc(size);
	memset(ptr, 0, size);
	terrain = ptr;
	terrain_splash = ptr + num_terrain * sizeof(terrain_terrain_t);

	//
	// PASS 2

	num_terrain_splash = 0;
	num_terrain = 0;

	wad_handle_lump("TERRAIN", cb_terrain);
}

