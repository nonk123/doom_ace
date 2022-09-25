// ANIMATIONS
#define STATE_SET_ANIMATION(anim,offset)	(0x80000000 | ((anim) << 16) | (offset))

// basic animations
enum
{
	ANIM_SPAWN, // must be zero
	ANIM_SEE,
	ANIM_PAIN,
	ANIM_MELEE,
	ANIM_MISSILE,
	ANIM_DEATH,
	ANIM_XDEATH,
	ANIM_RAISE,
	ANIM_CRUSH,
	ANIM_HEAL,
	//
	NUM_MOBJ_ANIMS
};

// weapon animations
enum
{
	ANIM_W_READY = NUM_MOBJ_ANIMS,
	ANIM_W_LOWER,
	ANIM_W_RAISE,
	ANIM_W_DEADLOW,
	ANIM_W_FIRE,
	ANIM_W_FIRE_ALT,
	ANIM_W_HOLD,
	ANIM_W_HOLD_ALT,
	ANIM_W_FLASH,
	ANIM_W_FLASH_ALT,
};

// custom inventory animations
enum
{
	ANIM_I_PICKUP = NUM_MOBJ_ANIMS,
	ANIM_I_USE,
};

// frame flags
#define FF_FULLBRIGHT	0x8000
#define FF_FAST	0x4000
#define FF_CANRAISE	0x2000

// FLAGS 0
#define MF_SPECIAL	0x00000001
#define MF_SOLID	0x00000002
#define MF_SHOOTABLE	0x00000004
#define MF_NOSECTOR	0x00000008
#define MF_NOBLOCKMAP	0x00000010
#define MF_AMBUSH	0x00000020
#define MF_JUSTHIT	0x00000040
#define MF_JUSTATTACKED	0x00000080
#define MF_SPAWNCEILING	0x00000100
#define MF_NOGRAVITY	0x00000200
#define MF_DROPOFF	0x00000400
#define MF_PICKUP	0x00000800
#define MF_NOCLIP	0x00001000
#define MF_SLIDE	0x00002000
#define MF_FLOAT	0x00004000
#define MF_TELEPORT	0x00008000
#define MF_MISSILE	0x00010000
#define MF_DROPPED	0x00020000
#define MF_SHADOW	0x00040000
#define MF_NOBLOOD	0x00080000
#define MF_CORPSE	0x00100000
#define MF_INFLOAT	0x00200000
#define MF_COUNTKILL	0x00400000
#define MF_COUNTITEM	0x00800000
#define MF_SKULLFLY	0x01000000
#define MF_NOTDMATCH	0x02000000

// FLAGS 1
#define MF1_ISMONSTER	0x00000001
#define MF1_NOTELEPORT	0x00000002
#define MF1_RANDOMIZE	0x00000004
#define MF1_TELESTOMP	0x00000008
#define MF1_NOTARGET	0x00000010
#define MF1_QUICKTORETALIATE	0x00000020
#define MF1_SEEKERMISSILE	0x00000040
#define MF1_NOFORWARDFALL	0x00000080
#define MF1_DONTTHRUST	0x00000100
#define MF1_NODAMAGETHRUST	0x00000200
#define MF1_DONTGIB	0x00000400
#define MF1_INVULNERABLE	0x00000800
#define MF1_BUDDHA	0x00001000
#define MF1_NODAMAGE	0x00002000
#define MF1_REFLECTIVE	0x00004000
#define MF1_BOSS	0x00008000
#define MF1_NORADIUSDMG	0x00010000
#define MF1_EXTREMEDEATH	0x00020000
#define MF1_NOEXTREMEDEATH	0x00040000
#define MF1_SKYEXPLODE	0x00080000
#define MF1_RIPPER	0x00100000
#define MF1_DONTRIP	0x00200000

// FLAGS extra (inventory) [shared]
#define MFE_INVENTORY_QUIET	0x00000001
#define MFE_INVENTORY_IGNORESKILL	0x00000002
#define MFE_INVENTORY_AUTOACTIVATE	0x00000004
#define MFE_INVENTORY_ALWAYSPICKUP	0x00000008
#define MFE_INVENTORY_INVBAR	0x00000010
#define MFE_INVENTORY_HUBPOWER	0x00000020
#define MFE_INVENTORY_PERSISTENTPOWER	0x00000040
#define MFE_INVENTORY_BIGPOWERUP	0x00000080
#define MFE_INVENTORY_NEVERRESPAWN	0x00000100
#define MFE_INVENTORY_KEEPDEPLETED	0x00000200
#define MFE_INVENTORY_ADDITIVETIME	0x00000400
#define MFE_INVENTORY_RESTRICTABSOLUTELY	0x00000800
#define MFE_INVENTORY_NOSCREENFLASH	0x00001000
#define MFE_INVENTORY_TRANSFER	0x00002000
#define MFE_INVENTORY_NOTELEPORTFREEZE	0x00004000
#define MFE_INVENTORY_NOSCREENBLINK	0x00008000
#define MFE_INVENTORY_UNTOSSABLE	0x00010000

// FLAGS extra (weapon)
#define MFE_WEAPON_NOAUTOFIRE	0x00100000
#define MFE_WEAPON_NOALERT	0x00200000
#define MFE_WEAPON_AMMO_OPTIONAL	0x00400000
#define MFE_WEAPON_ALT_AMMO_OPTIONAL	0x00800000
#define MFE_WEAPON_AMMO_CHECKBOTH	0x01000000
#define MFE_WEAPON_PRIMARY_USES_BOTH	0x02000000
#define MFE_WEAPON_ALT_USES_BOTH	0x04000000
#define MFE_WEAPON_NOAUTOAIM	0x08000000

