#include <stdlib.h>

#include "string.h"

#include "dungeon.h"
#include "pc.h"
#include "utils.h"
#include "move.h"
#include "path.h"

object * pc::drop_item(int slot)
{
	if(this->inventory.size() > ((unsigned) slot))
	{
		std::vector<object *>::iterator it = this->inventory.begin();
		int i;
		for(i=0; i < slot; i++, it++);

		object * ed = (*it);
		this->inventory.erase(it);

		return ed;
	}
	return NULL;
}

object * pc::unequip_item(int slot)
{
	if(this->equipped.size() > ((unsigned) slot))
	{
		std::vector<object *>::iterator it = this->equipped.begin();

		int i;
		for(i = 0; i < slot; i++, it++);

		object * ue = *it;
		this->equipped.erase(it);

		this->speed -= ue->speed;
		this->hitpoints -= ue->hit;

		if(this->hitpoints < 0)
			this->hitpoints = 0;

		if(this->inventory.size() < 10)
			this->inventory.push_back(ue);
		else // drop it
			return ue;

		return NULL;
	}
	return NULL;
}

int pc::equip_item(int slot)
{
	if(this->inventory.size() > ((unsigned) slot))
	{
		object * e = this->inventory[slot];

		unsigned i;
		int equipped_index = -1, ring_count = 0, ring_index1;

		// check if this type is already equipped
		for(i = 0; i < this->equipped.size(); i++)
		{
			if(this->equipped[i]->type == e->type)
			{
				if(e->type == objtype_RING)
				{
					ring_count++;
					if(!(ring_count >= 2))
					{
						ring_index1 = i;
						continue;
					}

					int ring_index2 = i;

					//choose a random ring to swap with
					equipped_index = (rand_range(0,1))? ring_index1 : ring_index2;
					break;
				}
				else
				{
					// swap this item out
					equipped_index = i;
					break;
				}
			}
		}

		// swap items if this type is already equipped
		if(equipped_index != -1)
		{
			this->inventory[slot] = this->equipped[equipped_index];
			this->equipped[equipped_index] = e;
		}else
		{
			this->equipped.push_back(e);

			std::vector<object *>::iterator it = this->inventory.begin();

			int j;
			for(j = 0; j < slot; j++, it++);

			this->inventory.erase(it);
		}

		this->speed += e->speed;
		this->hitpoints += e->hit;

		return 1;
	}

	return 0;
}

void delete_pc(character *the_pc)
{
  delete (pc *) the_pc;
}
uint32_t pc_is_alive(dungeon_t *d)
{
  return ((pc *) d->pc)->alive;
}

void place_pc(dungeon_t *d)
{
  ((pc *) d->pc)->position[dim_y] = rand_range(d->rooms->position[dim_y],
                                               (d->rooms->position[dim_y] +
                                                d->rooms->size[dim_y] - 1));
  ((pc *) d->pc)->position[dim_x] = rand_range(d->rooms->position[dim_x],
                                               (d->rooms->position[dim_x] +
                                                d->rooms->size[dim_x] - 1));

  pc_init_known_terrain(d->pc);
  pc_observe_terrain(d->pc, d);
}

void config_pc(dungeon_t *d)
{
  /* This should be in the PC constructor, now. */
  pc *the_pc;

  the_pc = new pc;
  d->pc = (character *) the_pc;

  the_pc->symbol = '@';

  place_pc(d);

  the_pc->hitpoints = PC_HP;
  the_pc->damage = dice(0,1,4);
  the_pc->speed = PC_SPEED;
  the_pc->next_turn = 0;
  the_pc->alive = 1;
  the_pc->sequence_number = 0;

  the_pc->equipped = std::vector<object *>();
  the_pc->inventory = std::vector<object *>();

  d->charmap[the_pc->position[dim_y]]
            [the_pc->position[dim_x]] = (character *) d->pc;

  dijkstra(d);
  dijkstra_tunnel(d);
}

uint32_t pc_next_pos(dungeon_t *d, pair_t dir)
{
  dir[dim_y] = dir[dim_x] = 0;

  /* Tunnel to the nearest dungeon corner, then move around in hopes *
   * of killing a couple of monsters before we die ourself.          */

  if (in_corner(d, d->pc)) {
    /*
    dir[dim_x] = (mapxy(d->pc.position[dim_x] - 1,
                        d->pc.position[dim_y]) ==
                  ter_wall_immutable) ? 1 : -1;
    */
    dir[dim_y] = (mapxy(((pc *) d->pc)->position[dim_x],
                        ((pc *) d->pc)->position[dim_y] - 1) ==
                  ter_wall_immutable) ? 1 : -1;
  } else {
    dir_nearest_wall(d, d->pc, dir);
  }

  return 0;
}

void pc_learn_terrain(character *the_pc, pair_t pos, terrain_type_t ter)
{
  ((pc *) the_pc)->known_terrain[pos[dim_y]][pos[dim_x]] = ter;
  ((pc *) the_pc)->visible[pos[dim_y]][pos[dim_x]] = 1;
}

void pc_reset_visibility(character *the_pc)
{
  uint32_t y, x;

  for (y = 0; y < DUNGEON_Y; y++) {
    for (x = 0; x < DUNGEON_X; x++) {
      ((pc *) the_pc)->visible[y][x] = 0;
    }
  }
}

terrain_type_t pc_learned_terrain(character *the_pc, int8_t y, int8_t x)
{
  return ((pc *) the_pc)->known_terrain[y][x];
}

void pc_init_known_terrain(character *the_pc)
{
  uint32_t y, x;

  for (y = 0; y < DUNGEON_Y; y++) {
    for (x = 0; x < DUNGEON_X; x++) {
      ((pc *) the_pc)->known_terrain[y][x] = ter_unknown;
      ((pc *) the_pc)->visible[y][x] = 0;
    }
  }
}

void pc_observe_terrain(character *the_pc, dungeon_t *d)
{
  pair_t where;
  pc *p;
  int8_t y_min, y_max, x_min, x_max;

  p = (pc *) the_pc;

  y_min = p->position[dim_y] - PC_VISUAL_RANGE;
  if (y_min < 0) {
    y_min = 0;
  }
  y_max = p->position[dim_y] + PC_VISUAL_RANGE;
  if (y_max > DUNGEON_Y - 1) {
    y_max = DUNGEON_Y - 1;
  }
  x_min = p->position[dim_x] - PC_VISUAL_RANGE;
  if (x_min < 0) {
    x_min = 0;
  }
  x_max = p->position[dim_x] + PC_VISUAL_RANGE;
  if (x_max > DUNGEON_X - 1) {
    x_max = DUNGEON_X - 1;
  }

  for (where[dim_y] = y_min; where[dim_y] <= y_max; where[dim_y]++) {
    where[dim_x] = x_min;
    can_see(d, p->position, where, 1);
    where[dim_x] = x_max;
    can_see(d, p->position, where, 1);
  }
  /* Take one off the x range because we alreay hit the corners above. */
  for (where[dim_x] = x_min - 1; where[dim_x] <= x_max - 1; where[dim_x]++) {
    where[dim_y] = y_min;
    can_see(d, p->position, where, 1);
    where[dim_y] = y_max;
    can_see(d, p->position, where, 1);
  }       
}

int32_t is_illuminated(character *the_pc, int8_t y, int8_t x)
{
  return ((pc *) the_pc)->visible[y][x];
}
