#ifndef PC_H
# define PC_H

# include <stdint.h>
#include <vector>

# include "dims.h"
# include "dungeon.h"

typedef struct dungeon dungeon_t;

uint32_t pc_is_alive(dungeon_t *d);
void config_pc(dungeon_t *d);
uint32_t pc_next_pos(dungeon_t *d, pair_t dir);
void place_pc(dungeon_t *d);
void delete_pc(character *the_pc);
void pc_learn_terrain(character *the_pc, pair_t pos, terrain_type_t ter);
terrain_type_t pc_learned_terrain(character *the_pc, int8_t y, int8_t x);
void pc_init_known_terrain(character *the_pc);
void pc_observe_terrain(character *the_pc, dungeon_t *d);
int32_t is_illuminated(character *the_pc, int8_t y, int8_t x);
void pc_reset_visibility(character *the_pc);
void display_x_y(dungeon_t * d, int x, int y, int radius);
int get_array_of_monsters(dungeon_t * d, character * near, character *** mon_arr, int radius);

# include "character.h"

class pc : public character {
 public:
  terrain_type_t known_terrain[DUNGEON_Y][DUNGEON_X];
  unsigned char visible[DUNGEON_Y][DUNGEON_X];
  std::vector<object *> inventory;
  std::vector<object *> equipped;
  int spell_cooldown;

  int equip_item(int slot);
  object * unequip_item(int slot);
  object * drop_item(int slot);
  int cast_spell(dungeon_t * d);
  int shoot_ranged(dungeon_t * d);
};

#endif
