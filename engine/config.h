// kgsws' ACE Engine
////

typedef struct
{
	uint16_t visplane_count;
	uint16_t vissprite_count;
	uint16_t drawseg_count;
	uint16_t e3dplane_count;
	uint8_t enable_decorate;
	uint8_t enable_dehacked;
	uint8_t wipe_type;
} mod_config_t;

typedef struct
{
	// player
	uint8_t auto_switch;
	uint8_t auto_aim;
	uint8_t mouse_look;
	uint8_t center_weapon;
	// display
	uint8_t show_fps;
	uint8_t wipe_type;
	uint8_t crosshair_type;
	uint8_t crosshair_red;
	uint8_t crosshair_green;
	uint8_t crosshair_blue;
} extra_config_t;

//

extern extra_config_t extra_config;
extern mod_config_t mod_config;

//

void init_config();

