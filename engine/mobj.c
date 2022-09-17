// kgsws' ACE Engine
////
// MOBJ handling changes.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "dehacked.h"
#include "decorate.h"
#include "inventory.h"
#include "mobj.h"
#include "player.h"
#include "weapon.h"
#include "map.h"
#include "stbar.h"
#include "cheat.h"

uint32_t *consoleplayer;
uint32_t *displayplayer;

thinker_t *thinkercap;

uint32_t mobj_netid;

//

// this only exists because original animations are all over the plase in 'mobjinfo_t'
const uint16_t base_anim_offs[NUM_MOBJ_ANIMS] =
{
	[ANIM_SPAWN] = offsetof(mobjinfo_t, state_spawn),
	[ANIM_SEE] = offsetof(mobjinfo_t, state_see),
	[ANIM_PAIN] = offsetof(mobjinfo_t, state_pain),
	[ANIM_MELEE] = offsetof(mobjinfo_t, state_melee),
	[ANIM_MISSILE] = offsetof(mobjinfo_t, state_missile),
	[ANIM_DEATH] = offsetof(mobjinfo_t, state_death),
	[ANIM_XDEATH] = offsetof(mobjinfo_t, state_xdeath),
	[ANIM_RAISE] = offsetof(mobjinfo_t, state_raise),
	[ANIM_CRUSH] = offsetof(mobjinfo_t, state_crush),
	[ANIM_HEAL] = offsetof(mobjinfo_t, state_heal),
};

//
// state changes

__attribute((regparm(2),no_caller_saved_registers))
uint32_t mobj_set_state(mobj_t *mo, uint32_t state)
{
	// normal state changes
	state_t *st;

	do
	{
		if(state & 0x80000000)
		{
			// change animation
			uint16_t offset;

			offset = state & 0xFFFF;
			mo->animation = (state >> 16) & 0xFF;

			if(mo->animation < NUM_MOBJ_ANIMS)
				state = *((uint16_t*)((void*)mo->info + base_anim_offs[mo->animation]));
			else
				state = mo->info->extra_states[mo->animation - NUM_MOBJ_ANIMS];

			if(state)
				state += offset;

			if(state >= mo->info->state_idx_limit)
				I_Error("[MOBJ] State jump '+%u' is invalid!", offset);
		}

		if(!state)
		{
			P_RemoveMobj(mo);
			return 0;
		}

		st = states + state;
		mo->state = st;
		mo->sprite = st->sprite;
		mo->frame = st->frame;
		mo->tics = st->tics;
		state = st->nextstate;

		if(st->acp)
			st->acp(mo, st, mobj_set_state);

	} while(!mo->tics);

	return 1;
}

__attribute((regparm(2),no_caller_saved_registers))
static uint32_t mobj_inv_change(mobj_t *mo, uint32_t state)
{
	// defer state change for later
	mo->custom_state = state;
}

static uint32_t mobj_inv_loop(mobj_t *mo, uint32_t state)
{
	// state set by custom inventory
	state_t *st;

	mo->custom_state = 0;

	while(1)
	{
		if(state & 0x80000000)
		{
			// change animation
			mobjinfo_t *info = mo->custom_inventory;
			uint16_t offset;
			uint8_t anim;

			offset = state & 0xFFFF;
			anim = (state >> 16) & 0xFF;

			if(anim < NUM_MOBJ_ANIMS)
				state = *((uint16_t*)((void*)info + base_anim_offs[anim]));
			else
				state = info->extra_states[anim - NUM_MOBJ_ANIMS];

			if(state)
				state += offset;

			if(state >= info->state_idx_limit)
				I_Error("[MOBJ] State jump '+%u' is invalid!", offset);
		}

		if(state <= 1)
		{
			mo->custom_inventory = NULL;
			return !state;
		}

		st = states + state;
		state = st->nextstate;

		if(st->acp)
		{
			st->acp(mo, st, mobj_inv_change);
			if(mo->custom_state)
			{
				// apply deferred state change
				state = mo->custom_state;
				mo->custom_state = 0;
			}
		}
	}
}

//
// inventory handling

static uint32_t give_ammo(mobj_t *mo, uint16_t type, uint16_t count, uint32_t dropped)
{
	uint32_t left;

	if(!type)
		return 0;

	if(dropped)
	{
		count /= 2;
		if(!count)
			count = 1;
	}

	if(	!(mobjinfo[type].eflags & MFE_INVENTORY_IGNORESKILL) &&
		(*gameskill == sk_baby || *gameskill == sk_nightmare)
	)
		count *= 2;

	left = inventory_give(mo, type, count);

	return left < count;
}

static uint32_t give_health(mobj_t *mo, uint32_t count, uint32_t max_count)
{
	uint32_t maxhp;

	if(max_count)
		maxhp = max_count;
	else
		maxhp = mo->info->spawnhealth;

	if(mo->player)
	{
		// Voodoo Doll ...
		if(mo->player->health >= maxhp)
			return 0;

		mo->player->health += count;
		if(mo->player->health > maxhp)
			mo->player->health = maxhp;
		mo->health = mo->player->health;

		return 1;
	}

	if(mo->health >= maxhp)
		return 0;

	mo->health += count;
	if(mo->health > maxhp)
		mo->health = maxhp;

	return 1;
}

static uint32_t give_armor(mobj_t *mo, mobjinfo_t *info)
{
	player_t *pl = mo->player;

	if(info->extra_type == ETYPE_ARMOR)
	{
		if(pl->armorpoints >= info->armor.count)
			return 0;

		pl->armorpoints = info->armor.count;
		pl->armortype = info - mobjinfo;

		return 1;
	} else
	if(info->extra_type == ETYPE_ARMOR_BONUS)
	{
		if(pl->armorpoints >= info->armor.max_count)
			return 0;

		pl->armorpoints += info->armor.count;
		if(pl->armorpoints > info->armor.max_count)
			pl->armorpoints = info->armor.max_count;

		if(!pl->armortype)
			pl->armortype = info - mobjinfo;

		return 1;
	}
}

static uint32_t give_power(mobj_t *mo, mobjinfo_t *info)
{
	uint32_t duration;

	if(info->powerup.type >= NUMPOWERS)
		return 1;

	if(info->powerup.duration < 0)
		duration = info->powerup.duration * -35;
	else
		duration = info->powerup.duration;

	if(!duration)
		return 1;

	if(!(info->eflags & MFE_INVENTORY_ALWAYSPICKUP) && mo->player->powers[info->powerup.type])
		return 0;

	if(!mo->player->powers[info->powerup.type])
		powerup_give(mo->player, info);

	if(info->eflags & MFE_INVENTORY_ADDITIVETIME)
		mo->player->powers[info->powerup.type] += duration;
	else
		mo->player->powers[info->powerup.type] = duration;

	return 1;
}

static uint32_t give_special(mobj_t *mo, mobjinfo_t *info)
{
	if(!mo->player)
		return 0;

	switch(info->inventory.special)
	{
		case 0:
			// backpack
			if(!mo->player->backpack)
			{
				mo->player->backpack = 1;
				mo->player->stbar_update |= STU_BACKPACK;
			}
			// give all existing ammo
			for(uint32_t idx = 0; idx < num_mobj_types; idx++)
			{
				mobjinfo_t *info = mobjinfo + idx;
				if(info->extra_type == ETYPE_AMMO)
					inventory_give(mo, idx, info->ammo.count);
			}
		break;
		case 1:
			// map
			if(mo->player->powers[pw_allmap])
				// original behaviour - different than ZDoom
				return 0;
			mo->player->powers[pw_allmap] = 1;
		break;
		case 2:
			// megasphere
			mo->health = dehacked.hp_megasphere;
			mo->player->health = mo->health;
			if(mo->player->armorpoints < 200)
			{
				mo->player->armorpoints = 200;
				mo->player->armortype = 44;
			}
		break;
		case 3:
			// berserk
			give_health(mo, 100, 0);
			mo->player->powers[pw_strength] = 1;
			if(mo->player->readyweapon != mobjinfo + MOBJ_IDX_FIST && inventory_check(mo, MOBJ_IDX_FIST))
				mo->player->pendingweapon = mobjinfo + MOBJ_IDX_FIST;
		break;
	}

	return 1;
}

static uint32_t pick_custom_inv(mobj_t *mo, mobjinfo_t *info)
{
	uint32_t ret;

	if(!info->st_custinv.pickup)
		return 1;

	if(mo->custom_inventory)
		I_Error("Nested CustomInventory is not supported!");

	mo->custom_inventory = info;
	ret = mobj_inv_loop(mo, info->st_custinv.pickup);

	if(info->eflags & MFE_INVENTORY_ALWAYSPICKUP)
		return 1;

	return ret;
}

static uint32_t use_custom_inv(mobj_t *mo, mobjinfo_t *info)
{
	if(!info->st_custinv.use)
		return 1;

	if(mo->custom_inventory)
		I_Error("Nested CustomInventory is not supported!");

	mo->custom_inventory = info;
	return mobj_inv_loop(mo, info->st_custinv.use);
}

//
// hooks

__attribute((regparm(2),no_caller_saved_registers))
void spawn_player(mapthing_t *mt)
{
	player_t *pl;
	int32_t idx = mt->type - 1;
	mobj_t *mo;
	mobjinfo_t *info;

	if(!playeringame[idx])
		return;

	pl = players + idx;
	info = mobjinfo + player_class[0]; // TODO

	// create body
	mo = P_SpawnMobj((fixed_t)mt->x << FRACBITS, (fixed_t)mt->y << FRACBITS, 0x80000000, player_class[0]);

	// check for reset
	if(pl->playerstate == PST_REBORN)
	{
		// cleanup
		uint32_t killcount;
		uint32_t itemcount;
		uint32_t secretcount;

		inventory_destroy(pl->inventory);

		killcount = pl->killcount;
		itemcount = pl->itemcount;
		secretcount = pl->secretcount;

		memset(pl, 0, sizeof(player_t));

		pl->killcount = killcount;
		pl->itemcount = itemcount;
		pl->secretcount = secretcount;

		pl->usedown = 1;
		pl->attackdown = 1;
		pl->playerstate = PST_LIVE;

		if(info == mobjinfo)
			pl->health = dehacked.start_health;
		else
			pl->health = info->spawnhealth;

		pl->pendingweapon = NULL;
		pl->readyweapon = NULL;

		// give 'depleted' original ammo; for status bar
		inventory_give(mo, 63, 0); // Clip
		inventory_give(mo, 69, 0); // Shell
		inventory_give(mo, 67, 0); // Cell
		inventory_give(mo, 65, 0); // RocketAmmo

		// default inventory
		for(plrp_start_item_t *si = info->start_item.start; si < (plrp_start_item_t*)info->start_item.end; si++)
		{
			mobj_give_inventory(mo, si->type, si->count);
			if(!pl->pendingweapon)
			{
				if(mobjinfo[si->type].extra_type == ETYPE_WEAPON)
					pl->pendingweapon = mobjinfo + si->type;
			}
		}

		pl->readyweapon = pl->pendingweapon;

		if(*deathmatch)
		{
			for(uint32_t i = 0; i < num_mobj_types; i++)
			{
				mobjinfo_t *info = mobjinfo + i;
				if(info->extra_type == ETYPE_KEY)
					inventory_give(mo, i, INV_MAX_COUNT);
			}
		}
	} else
		// use existing inventory
		mo->inventory = pl->inventory;

	// TODO: translation not in flags
	mo->flags |= idx << 26;

	mo->angle = ANG45 * (mt->angle / 45);
	mo->player = pl;
	mo->health = pl->health;

	pl->mo = mo;
	pl->playerstate = PST_LIVE;
	pl->refire = 0;
	pl->message = NULL;
	pl->damagecount = 0;
	pl->bonuscount = 0;
	pl->extralight = 0;
	pl->fixedcolormap = 0;
	pl->viewheight = info->player.view_height;
	pl->inventory = NULL;
	pl->stbar_update = 0;
	pl->inv_tick = 0;

	cheat_player_flags(pl);

	weapon_setup(pl);

	if(idx == *consoleplayer)
	{
		stbar_start(pl);
		HU_Start();
		player_viewheight(pl->viewheight);
	}
}

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t set_mobj_animation(mobj_t *mo, uint8_t anim)
{
	mobj_set_state(mo, STATE_SET_ANIMATION(anim, 0));
}

static __attribute((regparm(2),no_caller_saved_registers))
mobjinfo_t *prepare_mobj(mobj_t *mo, uint32_t type)
{
	uint32_t tmp = 8;
	mobjinfo_t *info;

	// find replacement
	while(mobjinfo[type].replacement)
	{
		if(!tmp)
			I_Error("[DECORATE] Too many replacements!");
		type = mobjinfo[type].replacement;
		tmp--;
	}

	info = mobjinfo + type;

	// clear memory
	memset(mo, 0, sizeof(mobj_t));

	// fill in new stuff
	mo->type = type;
	mo->flags1 = info->flags1;
	mo->netid = mobj_netid++;

	// return offset
	return info;
}

static __attribute((regparm(2),no_caller_saved_registers))
uint32_t finish_mobj(mobj_t *mo)
{
	// add thinker
	P_AddThinker(&mo->thinker);

	// ZDoom compatibility
	// teleport fog starts teleport sound
	if(mo->type == 39)
		S_StartSound(mo, 35);
}

static __attribute((regparm(2),no_caller_saved_registers))
void touch_mobj(mobj_t *mo, mobj_t *toucher)
{
	mobjinfo_t *info;
	player_t *pl;
	uint32_t left, given;
	fixed_t diff;

	if(!toucher->player || toucher->player->playerstate != PST_LIVE)
		return;

	diff = mo->z - toucher->z;
	if(diff > toucher->height || diff < -mo->height)
		return;

	pl = toucher->player;
	info = mo->info;

	if(mo->type < NUMMOBJTYPES)
	{
		// old items - type is based on current sprite
		uint16_t type;

		// forced height
		if(diff < 8 * -FRACUNIT)
			return;

		// DEHACKED workaround
		if(mo->sprite < sizeof(deh_pickup_type))
			type = deh_pickup_type[mo->sprite];
		else
			type = 0;

		if(!type)
		{
			// original game would throw an error
			mo->flags &= ~MF_SPECIAL;
			// heheh
			if(!(mo->flags & MF_NOGRAVITY))
				mo->momz += 8 * FRACUNIT;
			return;
		}

		info = mobjinfo + type;
	}

	// new inventory stuff

	switch(info->extra_type)
	{
		case ETYPE_HEALTH:
			// health pickup
			if(!give_health(toucher, info->inventory.count, info->inventory.max_count) && !(info->eflags & MFE_INVENTORY_ALWAYSPICKUP))
				// can't pickup
				return;
		break;
		case ETYPE_INV_SPECIAL:
			// special pickup type
			if(!give_special(toucher, info))
				// can't pickup
				return;
		break;
		case ETYPE_INVENTORY:
			// add to inventory
			if(inventory_give(toucher, mo->type, info->inventory.count) >= info->inventory.count)
				// can't pickup
				return;
		break;
		case ETYPE_INVENTORY_CUSTOM:
			// pickup
			if(!pick_custom_inv(toucher, info))
				// can't pickup
				return;
			// check for 'use'
			if(info->st_custinv.use)
			{
				given = 0;
				// autoactivate
				if(info->eflags & MFE_INVENTORY_AUTOACTIVATE)
					given = use_custom_inv(toucher, info);
				// give as item
				if(!given)
					given = inventory_give(toucher, mo->type, info->inventory.count) < info->inventory.count;
				// check
				if(!given && !(info->eflags & MFE_INVENTORY_ALWAYSPICKUP))
					return;
			}
		break;
		case ETYPE_WEAPON:
			// weapon
			given = inventory_give(toucher, mo->type, info->inventory.count);
			if(!given)
			{
				pl->stbar_update |= STU_WEAPON_NEW; // evil grin
				// TODO: auto-weapon-switch optional
				pl->pendingweapon = info;
				if(!pl->psprites[0].state)
					// fix 'no weapon' state
					weapon_setup(pl);
			}
			given = given < info->inventory.count;
			// primary ammo
			given |= give_ammo(toucher, info->weapon.ammo_type[0], info->weapon.ammo_give[0], mo->flags & MF_DROPPED);
			// secondary ammo
			given |= give_ammo(toucher, info->weapon.ammo_type[1], info->weapon.ammo_give[1], mo->flags & MF_DROPPED);
			// check
			if(!given && !(info->eflags & MFE_INVENTORY_ALWAYSPICKUP))
				return;
		break;
		case ETYPE_AMMO:
		case ETYPE_AMMO_LINK:
			// add ammo to inventory
			if(	!give_ammo(toucher, mo->type, info->inventory.count, mo->flags & MF_DROPPED) &&
				!(info->eflags & MFE_INVENTORY_ALWAYSPICKUP)
			)
				// can't pickup
				return;
		break;
		case ETYPE_KEY:
			// add to inventory
			inventory_give(toucher, mo->type, 1);
		break;
		case ETYPE_ARMOR:
		case ETYPE_ARMOR_BONUS:
			given = 0;
			// autoactivate
			if(info->eflags & MFE_INVENTORY_AUTOACTIVATE)
				given = give_armor(toucher, info);
			// give as item
			if(!given)
				given = inventory_give(toucher, mo->type, info->inventory.count) < info->inventory.count;
			// check
			if(!given && !(info->eflags & MFE_INVENTORY_ALWAYSPICKUP))
				return;
		break;
		case ETYPE_POWERUP:
			given = 0;
			// autoactivate
			if(info->eflags & MFE_INVENTORY_AUTOACTIVATE)
				given = give_power(toucher, info);
			// give as item
			if(!given)
				given = inventory_give(toucher, mo->type, info->inventory.count) < info->inventory.count;
			// check
			if(!given && !(info->eflags & MFE_INVENTORY_ALWAYSPICKUP))
				return;
		break;
		case ETYPE_HEALTH_PICKUP:
			given = 0;
			// autoactivate
			if(info->eflags & MFE_INVENTORY_AUTOACTIVATE)
				given = give_health(toucher, info->spawnhealth, toucher->info->spawnhealth);
			// give as item
			if(!given)
				given = inventory_give(toucher, mo->type, info->inventory.count) < info->inventory.count;
			// check
			if(!given && !(info->eflags & MFE_INVENTORY_ALWAYSPICKUP))
				return;
		break;
		default:
			// this should not be set
			mo->flags &= ~MF_SPECIAL;
		return;
	}

	// count
	if(mo->flags & MF_COUNTITEM)
		pl->itemcount++;

	// remove
	mo->flags &= ~MF_SPECIAL; // disable original item respawn logic
	P_RemoveMobj(mo);

	if(info->eflags & MFE_INVENTORY_QUIET)
		return;

	// flash
	if(!(info->eflags & MFE_INVENTORY_NOSCREENFLASH))
	{
		pl->bonuscount += 6;
		if(pl->bonuscount > 24)
			pl->bonuscount = 24; // new limit
	}

	// sound
	if(pl == players + *consoleplayer)
		S_StartSound(NULL, info->inventory.sound_pickup);

	// message
	if(info->inventory.message)
		pl->message = info->inventory.message;
}

__attribute((regparm(2),no_caller_saved_registers))
static void kill_animation(mobj_t *mo)
{
	// custom death animations can be added

	if(mo->info->state_xdeath && mo->health < -mo->info->spawnhealth)
		set_mobj_animation(mo, ANIM_XDEATH);
	else
		set_mobj_animation(mo, ANIM_DEATH);

	if(mo->tics > 0)
	{
		mo->tics -= P_Random() & 3;
		if(mo->tics <= 0)
			mo->tics = 1;
	}

	if(mo->player)
		mo->player->extralight = 0;
}

//
// API

void mobj_use_item(mobj_t *mo, inventory_t *item)
{
	mobjinfo_t *info = mobjinfo + item->type;
	switch(info->extra_type)
	{
		case ETYPE_INVENTORY_CUSTOM:
			if(!use_custom_inv(mo, info))
				return;
		break;
		case ETYPE_ARMOR:
		case ETYPE_ARMOR_BONUS:
			if(!give_armor(mo, info))
				return;
		break;
		case ETYPE_POWERUP:
			if(!give_power(mo, info))
				return;
		break;
		case ETYPE_HEALTH_PICKUP:
			if(!give_health(mo, info->spawnhealth, mo->info->spawnhealth))
				return;
		break;
	}

	inventory_take(mo, item->type, 1);
}

uint32_t mobj_give_inventory(mobj_t *mo, uint16_t type, uint16_t count)
{
	uint32_t given;
	mobjinfo_t *info = mobjinfo + type;

	if(!count)
		return 0;

	switch(info->extra_type)
	{
		case ETYPE_HEALTH:
			return give_health(mo, (uint32_t)count * (uint32_t)info->inventory.count, info->inventory.max_count);
		case ETYPE_INV_SPECIAL:
			return give_special(mo, info);
		case ETYPE_INVENTORY:
		case ETYPE_WEAPON:
		case ETYPE_AMMO:
		case ETYPE_AMMO_LINK:
		case ETYPE_KEY: // can this fail in ZDoom?
			return inventory_give(mo, type, count) < count;
		case ETYPE_INVENTORY_CUSTOM:
			// pickup
			if(!pick_custom_inv(mo, info))
				return 0;
			// check for 'use'
			if(info->st_custinv.use)
			{
				given = 0;
				// autoactivate
				if(info->eflags & MFE_INVENTORY_AUTOACTIVATE)
				{
					given = use_custom_inv(mo, info);
					if(given)
						count--;
				}
				// give as item(s)
				if(!given || count)
					given |= inventory_give(mo, type, count) < count;
				// check
				if(!given && !(info->eflags & MFE_INVENTORY_ALWAYSPICKUP))
					return 0;
			}
			return 1;
		case ETYPE_ARMOR:
		case ETYPE_ARMOR_BONUS:
			given = 0;
			// autoactivate
			if(info->eflags & MFE_INVENTORY_AUTOACTIVATE)
			{
				given = give_armor(mo, info);
				if(given)
					count--;
			}
			// give as item(s)
			if(!given || count)
				given |= inventory_give(mo, type, count) < count;
			// check
			if(!given && !(info->eflags & MFE_INVENTORY_ALWAYSPICKUP))
				return 0;
			return 1;
		case ETYPE_POWERUP:
			given = 0;
			// autoactivate
			if(info->eflags & MFE_INVENTORY_AUTOACTIVATE)
			{
				given = give_power(mo, info);
				if(given)
					count--;
			}
			// give as item(s)
			if(!given || count)
				given |= inventory_give(mo, type, count) < count;
			// check
			if(!given && !(info->eflags & MFE_INVENTORY_ALWAYSPICKUP))
				return 0;
			return 1;
		case ETYPE_HEALTH_PICKUP:
			given = 0;
			// autoactivate
			if(info->eflags & MFE_INVENTORY_AUTOACTIVATE)
			{
				given = give_health(mo, info->spawnhealth, mo->info->spawnhealth);
				if(given)
					count--;
			}
			// give as item(s)
			if(!given || count)
				given |= inventory_give(mo, type, count) < count;
			// check
			if(!given && !(info->eflags & MFE_INVENTORY_ALWAYSPICKUP))
				return 0;
			return 1;
	}

	return 0;
}

uint32_t mobj_for_each(uint32_t (*cb)(mobj_t*))
{
	if(!thinkercap->next)
		// this happens only before any level was loaded
		return 0;

	for(thinker_t *th = thinkercap->next; th != thinkercap; th = th->next)
	{
		uint32_t ret;

		if(th->function != (void*)0x00031490 + doom_code_segment)
			continue;

		ret = cb((mobj_t*)th);
		if(ret)
			return ret;
	}

	return 0;
}

mobj_t *mobj_by_netid(uint32_t netid)
{
	if(!netid)
		return NULL;

	if(!thinkercap->next)
		return NULL;

	for(thinker_t *th = thinkercap->next; th != thinkercap; th = th->next)
	{
		mobj_t *mo;

		if(th->function != (void*)0x00031490 + doom_code_segment)
			continue;

		mo = (mobj_t*)th;
		if(mo->netid == netid)
			return mo;
	}

	return NULL;
}

__attribute((regparm(2),no_caller_saved_registers))
void explode_missile(mobj_t *mo)
{
	mo->momx = 0;
	mo->momy = 0;
	mo->momz = 0;

	set_mobj_animation(mo, ANIM_DEATH);

	if(mo->flags1 & MF1_RANDOMIZE && mo->tics > 0)
	{
		mo->tics -= P_Random() & 3;
		if(mo->tics <= 0)
			mo->tics = 1;
	}

	mo->flags &= ~MF_MISSILE;

	S_StartSound(mo, mo->info->deathsound);
}

void mobj_damage(mobj_t *target, mobj_t *inflictor, mobj_t *source, uint32_t damage, uint32_t extra)
{
	// target = what is damaged
	// inflictor = damage source (projectile or ...)
	// source = what is responsible
	player_t *player;
	int32_t kickback;
	uint_fast8_t forced;

	if(!(target->flags & MF_SHOOTABLE))
		return;

	if(target->health <= 0)
		return;

	if(target->flags & MF_SKULLFLY)
	{
		target->momx = 0;
		target->momy = 0;
		target->momz = 0;
	}

	forced = damage >= 1000000;
	if(damage > 1000000)
		damage = target->health;

	if(source && source->player && source->player->readyweapon)
		kickback = source->player->readyweapon->weapon.kickback;
	else
		kickback = 100;

	player = target->player;

	if(player && *gameskill == sk_baby)
		damage /= 2;

	if(	inflictor &&
		kickback &&
		!(target->flags1 & MF1_DONTTHRUST) &&
		!(target->flags & MF_NOCLIP) &&
		!(inflictor->flags1 & MF1_NODAMAGETHRUST) // TODO: extra steps for hitscan
	) {
		angle_t angle;
		int32_t thrust;

		thrust = damage > 10000 ? 10000 : damage;

		angle = R_PointToAngle2(inflictor->x, inflictor->y, target->x, target->y);
		thrust = thrust * (FRACUNIT >> 3) * 100 / target->info->mass;

		if(kickback != 100)
			thrust = (thrust * kickback) / 100;

		if(	!(target->flags1 & (MF1_NOFORWARDFALL | MF1_INVULNERABLE | MF1_BUDDHA | MF1_NODAMAGE)) &&
			!(inflictor->flags1 & MF1_NOFORWARDFALL) && // TODO: extra steps for hitscan
			damage < 40 &&
			damage > target->health &&
			target->z - inflictor->z > 64 * FRACUNIT &&
			P_Random() & 1
		) {
			angle += ANG180;
			thrust *= 4;
		}

		angle >>= ANGLETOFINESHIFT;
		target->momx += FixedMul(thrust, finecosine[angle]);
		target->momy += FixedMul(thrust, finesine[angle]);
	}

	if(	target->flags1 & MF1_INVULNERABLE &&
		!forced
	)
		return;

	if(player)
	{
		if(target->subsector->sector->special == 11 && damage >= target->health)
			damage = target->health - 1;

		if(player->armortype)
		{
			uint32_t saved;

			saved = (damage * mobjinfo[player->armortype].armor.percent) / 100;
			if(player->armorpoints <= saved)
			{
				saved = player->armorpoints;
				player->armortype = 0;
			}

			player->armorpoints -= saved;
			damage -= saved;
		}

		if(!(target->flags1 & MF1_NODAMAGE))
		{
			player->health -= damage;
			if(player->health < 0)
				player->health = 0;

			player->damagecount += damage;
			if(player->damagecount > 60)
				player->damagecount = 60; // this is a bit less
		}

		player->attacker = source;

		if(source && source != target && player->cheats & CF_REVENGE)
			mobj_damage(source, NULL, target, 1000000, 0);

		// I_Tactile ...
	}

	if(!(target->flags1 & MF1_NODAMAGE))
	{
		target->health -= damage;
		if(target->health <= 0)
		{
			if(	target->flags1 & MF1_BUDDHA &&
				!forced
			)
			{
				target->health = 1;
				if(player)
					player->health = 1;
			} else
			{
				P_KillMobj(source, target);
				return;
			}
		}
	}

	if(	P_Random() < target->info->painchance &&
		!(target->flags & MF_SKULLFLY)
	) {
		target->flags |= MF_JUSTHIT;
		mobj_set_state(target, STATE_SET_ANIMATION(ANIM_PAIN, 0));
	}

	target->reactiontime = 0;

	if(	!dehacked.no_infight &&
		(!target->threshold || target->flags1 & MF1_QUICKTORETALIATE) &&
		source && source != target &&
		!(source->flags1 & MF1_NOTARGET)
	) {
		target->target = source;
		target->threshold = 100;
		if(target->info->state_see && target->state == states + target->info->state_spawn)
			mobj_set_state(target, STATE_SET_ANIMATION(ANIM_SEE, 0));
	}
}

//
// hooks

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// import variables
	{0x0002B3D8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&displayplayer},
	{0x0002B3DC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&consoleplayer},
	{0x0002CF74, DATA_HOOK | HOOK_IMPORT, (uint32_t)&thinkercap},
	// replace 'P_SpawnPlayer'
	{0x000317F0, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)spawn_player},
	// replace call to 'memset' in 'P_SpawnMobj'
	{0x00031569, CODE_HOOK | HOOK_UINT16, 0xEA89},
	{0x00031571, CODE_HOOK | HOOK_UINT32, 0x90C38900},
	{0x0003156D, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)prepare_mobj},
	// use 'mobjinfo' pointer from new 'prepare_mobj'
	{0x00031585, CODE_HOOK | HOOK_SET_NOPS, 6},
	// update 'P_SpawnMobj'; disable '->type'
	{0x00031579, CODE_HOOK | HOOK_SET_NOPS, 3},
	// replace call to 'P_AddThinker' in 'P_SpawnMobj'
	{0x00031647, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)finish_mobj},
	// replace call to 'P_TouchSpecialThing' in 'PIT_CheckThing'
	{0x0002B031, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)touch_mobj},
	// replace call to 'P_SetMobjState' in 'P_MobjThinker'
	{0x000314F0, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)mobj_set_state},
	// fix jump condition in 'P_MobjThinker'
	{0x000314E4, CODE_HOOK | HOOK_UINT8, 0x7F},
	// replace 'P_SetMobjState'
	{0x00030EA0, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)mobj_set_state},
	// replace 'P_DamageMobj' - use trampoline
	{0x0002A460, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)hook_mobj_damage},
	// extra stuff in 'P_KillMobj' - replaces animation change
	{0x0002A3C8, CODE_HOOK | HOOK_UINT16, 0xD889},
	{0x0002A3CA, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)kill_animation},
	{0x0002A3CF, CODE_HOOK | HOOK_JMP_DOOM, 0x0002A40D},
	// replace 'P_ExplodeMissile'
	{0x00030F00, CODE_HOOK | HOOK_JMP_ACE, (uint32_t)explode_missile},
	// change 'mobj_t' size
	{0x00031552, CODE_HOOK | HOOK_UINT32, sizeof(mobj_t)},
	// fix 'P_SpawnMobj'; disable old 'frame'
	{0x000315F9, CODE_HOOK | HOOK_SET_NOPS, 3},
	// fix player check in 'PIT_CheckThing'
	{0x0002AFC5, CODE_HOOK | HOOK_UINT16, 0xBB83},
	{0x0002AFC7, CODE_HOOK | HOOK_UINT32, offsetof(mobj_t, player)},
	{0x0002AFCB, CODE_HOOK | HOOK_UINT32, 0x427400},
	{0x0002AFCE, CODE_HOOK | HOOK_SET_NOPS, 6},
	// replace 'P_SetMobjState' with new animation system
	{0x00027776, CODE_HOOK | HOOK_UINT32, 0x909000b2 | (ANIM_SEE << 8)}, // A_Look
	{0x00027779, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation}, // A_Look
	{0x0002782A, CODE_HOOK | HOOK_UINT32, 0x909000b2 | (ANIM_SPAWN << 8)}, // A_Chase
	{0x0002782D, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation}, // A_Chase
	{0x0002789D, CODE_HOOK | HOOK_UINT32, 0x909000b2 | (ANIM_MELEE << 8)}, // A_Chase
	{0x000278A0, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation}, // A_Chase
	{0x000278DD, CODE_HOOK | HOOK_UINT32, 0x909000b2 | (ANIM_MISSILE << 8)}, // A_Chase
	{0x000278E0, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation}, // A_Chase
	// replace 'P_SetMobjState' with new animation system (PIT_CheckThing, MF_SKULLFLY)
	{0x0002AF2F, CODE_HOOK | HOOK_UINT32, 0x909000b2 | (ANIM_SPAWN << 8)},
	{0x0002AF32, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation},
	// replace 'P_SetMobjState' with new animation system (PIT_ChangeSector, gibs)
	{0x0002BEBA, CODE_HOOK | HOOK_UINT8, ANIM_CRUSH},
	{0x0002BEC0, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation},
	// replace 'P_SetMobjState' with new animation system (P_MovePlayer)
	{0x0003324B, CODE_HOOK | HOOK_UINT16, 0xB880},
	{0x0003324D, CODE_HOOK | HOOK_UINT32, offsetof(mobj_t, animation)},
	{0x00033251, CODE_HOOK | HOOK_UINT8, ANIM_SPAWN}, // check for
	{0x00033255, CODE_HOOK | HOOK_UINT32, ANIM_SEE}, // replace with
	{0x00033259, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)set_mobj_animation},
};

