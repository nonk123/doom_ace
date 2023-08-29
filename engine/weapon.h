// kgsws' ACE Engine
////

//

void weapon_setup(player_t* pl);
uint32_t weapon_fire(player_t* pl, uint32_t secondary, uint32_t refire);
uint32_t weapon_check_ammo(player_t* pl);
uint32_t weapon_has_ammo(mobj_t* mo, mobjinfo_t* info, uint32_t check);

void weapon_lower(player_t* pl)
    __attribute((regparm(2), no_caller_saved_registers));

void weapon_move_pspr(player_t* pl)
    __attribute((regparm(2), no_caller_saved_registers));
void weapon_set_state(player_t* pl, uint32_t idx, mobjinfo_t* info,
                      uint32_t state, uint16_t extra);

void wpn_codeptr(mobj_t* mo, state_t* st, stfunc_t stfunc)
    __attribute((regparm(2), no_caller_saved_registers));
void wpn_sound(mobj_t* mo, state_t* st, stfunc_t stfunc)
    __attribute((regparm(2), no_caller_saved_registers));
