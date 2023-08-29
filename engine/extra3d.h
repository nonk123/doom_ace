// kgsws' ACE Engine
////

#define E3D_SOLID 1
#define E3D_BLOCK_HITSCAN 2
#define E3D_BLOCK_SIGHT 4
#define E3D_ADDITIVE 8
#define E3D_DRAW_INISIDE 16
#define E3D_WATER 32

#define E3D_SWAP_PLANES 0x8000

typedef struct extraplane_s
{
	struct extraplane_s* next;
	line_t* line;
	sector_t* source;
	fixed_t* height;
	uint16_t* texture;
	uint16_t* pic;
	uint16_t* light;
	fixed_t* rowoffset;
	uint16_t flags;
	uint16_t alpha;
} extraplane_t;

typedef struct extra_height_s
{
	struct extra_height_s* next;
	fixed_t height;
} extra_height_t;

//

extern int16_t* e3d_floorclip;
extern int16_t* e3d_ceilingclip;

extern extra_height_t* e3d_up_height;
extern extra_height_t* e3d_dn_height;

extern fixed_t tmextrafloor;
extern fixed_t tmextraceiling;
extern fixed_t tmextradrop;

extern uint_fast8_t e3d_plane_move;

extern uint_fast8_t e3d_dbg;
extern uint32_t e3d_dbg_splitcount;
extern uint32_t e3d_dbg_planecount;

//

void e3d_init(uint32_t count);
void e3d_create();
void e3d_reset();
void e3d_draw_height(fixed_t);
void e3d_draw_planes();
uint32_t e3d_count_planes();

//

extraplane_t* e3d_check_inside(sector_t* sec, fixed_t z, uint32_t flags);
void e3d_check_heights(mobj_t* mo, sector_t* sec, uint32_t no_step);
void e3d_check_water(mobj_t* mo);
void e3d_check_midtex(mobj_t* mo, line_t* ln, uint32_t no_step);

void e3d_update_planes(sector_t* src, uint32_t planes);

//

visplane_t* e3d_find_plane(fixed_t height, uint32_t picnum, uint16_t light, uint16_t alpha);
visplane_t* e3d_check_plane(visplane_t* pl, int32_t start, int32_t stop);

//

void e3d_add_height(fixed_t height);
