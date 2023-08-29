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
	uint8_t texture_workaround;
	uint8_t wipe_type;
	// menu
	uint8_t menu_font_height;
	int8_t menu_save_empty;
	int8_t menu_save_valid;
	int8_t menu_save_error;
	int8_t menu_save_mismatch;
	// sound
	uint8_t sound_min_channels;
	// memory
	uint8_t mem_min;
	// game
	uint8_t game_mode;
	uint8_t save_name[8];
	// automap
	uint8_t automap_lockdefs;
	// ZDoom compat
	uint8_t color_fullbright;
	// status bar
	union
	{
		struct
		{
			uint64_t ammo_bullet;
			uint64_t ammo_shell;
			uint64_t ammo_cell;
			uint64_t ammo_rocket;
		};
		uint64_t ammo_type[4];
	};
} mod_config_t;

typedef struct
{
	// player
	uint8_t auto_switch;
	uint8_t auto_aim;
	uint8_t mouse_look;
	uint8_t center_weapon;
	uint16_t player_color;
	// display
	uint8_t show_fps;
	uint8_t wipe_type;
	uint16_t crosshair_color;
	uint8_t crosshair_type;
	// quick inventory
	uint64_t quick_inv_alias;
	uint16_t quick_inv;
	// sound
	uint16_t sound_mix_channels;
	uint8_t sound_sfx_device;
	uint8_t sound_music_device;
	uint32_t sound_s_port;
	uint32_t sound_s_irq;
	uint32_t sound_s_dma;
	uint32_t sound_m_port;
} extra_config_t;

//

extern extra_config_t extra_config;
extern mod_config_t mod_config;

//

void init_config();
void config_postinit();
