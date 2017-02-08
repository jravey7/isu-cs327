#include <stdlib.h>
#include <ncurses.h>

#include "string.h"

#include "dungeon.h"
#include "pc.h"
#include "utils.h"
#include "move.h"
#include "path.h"
#include "io.h"

void display_x_y(dungeon_t * d, int x, int y, int radius)
{
	if(radius > 0) {

		char border_top[radius * 2 + 3 + 1] = {};
		memset(border_top, '%', radius * 2 + 3);

		if(y + radius + 1 < 21)
		{
			mvprintw(y + radius + 2, x - radius - 1, "%s", border_top);
		}
		if(y - radius - 1 > 0)
		{
			mvprintw(y - radius , x - radius - 1, "%s", border_top);
		}

		int k;
		for(k = 0; k <= radius * 2 + 1; k++)
		{
			if((y - radius + k > 0) && (y - radius + k < 21))
			{
				if(x - radius - 1 > 0)
					mvprintw(y - radius + k, x - radius - 1, "%c", '%');

				if(x + radius + 1 < 80)
					mvprintw(y - radius + k, x + radius + 1, "%c", '%');
			}
		}

		int i,j;
		for(i=y - radius; i < y + radius; i++) {
			for(j=x -  radius; j < x + radius; j++) {
				if(i > 0 && j > 0 && i < 21 && j < 80)
				{
					// show monsters in radius
					if(d->charmap[i][j] && d->charmap[i][j] != d->pc)
					{
						if(is_valid_color(d->charmap[i][j]->color))
							attron(COLOR_PAIR(d->charmap[i][j]->color));

						mvprintw(i + 1,j, "%c", d->charmap[i][j]->symbol);

						if(is_valid_color(d->charmap[i][j]->color))
							attroff(COLOR_PAIR(d->charmap[i][j]->color));
					}
				}
			}
		}
	}
	else
	{
		if(d->charmap[y][x] && d->charmap[y][x] != d->pc)
		{
			if(is_valid_color(d->charmap[y][x]->color))
				attron(COLOR_PAIR(d->charmap[y][x]->color));

			mvprintw(y + 1, x, "%c", d->charmap[y][x]->symbol);

			if(is_valid_color(d->charmap[y][x]->color))
				attroff(COLOR_PAIR(d->charmap[y][x]->color));
		}
		if(y < 20)
			mvprintw(y+2, x, "^");
		else
			mvprintw(y, x, "\\");
	}

	refresh();
}

int pc::shoot_ranged(dungeon_t * d)
{
	character ** mon_arr;
	// get list of monsters in radius 10 of PC
	int mons_in_radius = get_array_of_monsters(d, d->pc, &mon_arr, 10);

	if(mons_in_radius > 0)
	{
		//select monster to cast spell
		mvprintw(0,0, "Use 8 and 2 to scroll through targets. Space bar to select target. 'c' to cancel.");
		refresh();

		character * toAttack = mon_arr[0];
		display_x_y(d, toAttack->position[dim_x], toAttack->position[dim_y], 0);
		refresh();
		char selection = getch();
		int i = 0;
		while(selection != ' ' && selection != 'c')
		{
			if(selection == '8')
			{
				i++;
				if(i >= mons_in_radius)
					i = 0;
				toAttack = mon_arr[i];
			}
			else if(selection == '2')
			{
				i--;
				if(i < 0)
					i = mons_in_radius -1;
				toAttack = mon_arr[i];
			}
			io_display(d);
			mvprintw(0,0, "Use 8 and 2 to scroll through targets. Space bar to select target. 'c' to cancel.");
			display_x_y(d, toAttack->position[dim_x], toAttack->position[dim_y], 0);
			refresh();
			selection = getch();
			while(selection != ' ' && selection != 'c' && selection != '8' && selection != '2')
				selection = getch();
		}

		if(selection == 'c')
		{
			free(mon_arr);
			return -1;
		}

		// do damage here (toAttack)
		std::vector<object *>::iterator it;
		object *ranged;
		int found_ranged = 0;
		for(it = ((pc *)d->pc)->equipped.begin(); it != ((pc *)d->pc)->equipped.end(); it++)
		{
			if((*it)->type == objtype_RANGED)
			{
				found_ranged = 1;
				ranged = *it;
				break;
			}
		}

		if(found_ranged)
		{
			toAttack->hitpoints -= ranged->damage.roll();

			if(toAttack->hitpoints < 0)
				if(character_is_alive(toAttack))
				{
					character_die(toAttack);
					d->num_monsters--;
				}


			char msg[50] = {};
			sprintf(msg,"Attacked %c with bow!", toAttack->symbol);
			io_queue_message(msg);
			refresh();
		}
		else
		{
			char msg[50] = {};
			sprintf(msg,"No ranged weapon!");
			io_queue_message(msg);
			refresh();
			return -1;
		}
	}
	else
	{
		char msg[50] = {};
		sprintf(msg,"No monsters in range (10) for ranged attack!");
		io_queue_message(msg);
		refresh();
		return -1;
	}


	free(mon_arr);

	return 0;
}

int pc::cast_spell(dungeon_t * d)
{
	character ** mon_arr;
	// get list of monsters in radius 10 of PC
	int mons_in_radius = get_array_of_monsters(d, d->pc, &mon_arr, 5);

	if(mons_in_radius > 0)
	{
		//select monster to cast spell
		mvprintw(0,0, "Use 8 and 2 to scroll through targets. Space bar to select target. 'c' to cancel.");
		refresh();

		character * toAttack = mon_arr[0];
		display_x_y(d, toAttack->position[dim_x], toAttack->position[dim_y], 2);
		refresh();
		char selection = getch();
		int i = 0;
		while(selection != ' ' && selection != 'c')
		{
			if(selection == '8')
			{
				i++;
				if(i >= mons_in_radius)
					i = 0;
				toAttack = mon_arr[i];
			}
			else if(selection == '2')
			{
				i--;
				if(i < 0)
					i = mons_in_radius -1;
				toAttack = mon_arr[i];
			}
			io_display(d);
			mvprintw(0,0, "Use 8 and 2 to scroll through targets. Space bar to select target. 'c' to cancel.");
			display_x_y(d, toAttack->position[dim_x], toAttack->position[dim_y], 2);
			refresh();
			selection = getch();
			while(selection != ' ' && selection != 'c' && selection != '8' && selection != '2')
				selection = getch();
		}

		if(selection == 'c')
		{
			free(mon_arr);
			return -1;
		}

		character ** surrounding_mons;

		int num_surrounding_mons = get_array_of_monsters(d, toAttack, &surrounding_mons, 2);

		for(i = 0; i < num_surrounding_mons; i++)
		{
			// do damage here (toAttack) and surrounding monsters
			surrounding_mons[i]->hitpoints -= 150;

			if(toAttack->hitpoints < 0)
				if(character_is_alive(toAttack))
				{
					character_die(toAttack);
					d->num_monsters--;
				}
		}

		free(mon_arr);
		free(surrounding_mons);

		char msg[50] = {};
		sprintf(msg,"Attacked %c with area of effect magic spell hitting %d monster(s)!", toAttack->symbol, num_surrounding_mons);
		io_queue_message(msg);
		refresh();
	}
	else
	{
		char msg[50] = {};
		sprintf(msg,"No monsters in range (5) for spell!");
		io_queue_message(msg);
		refresh();
		return -1;
	}

	return 0;
}

int get_array_of_monsters(dungeon_t * d, character * near, character *** mon_arr, int radius)
{
	int x, y, count = 0;

	*mon_arr = (character **) malloc(d->num_monsters * sizeof (*mon_arr));

	if(radius <= 0) {
	  /* Get a linear list of all monsters */
	  for (y = 1; y < DUNGEON_Y - 1; y++) {
	    for (x = 1; x < DUNGEON_X - 1; x++) {
	      if (d->charmap[y][x] && d->charmap[y][x] != d->pc) {
	        (*mon_arr)[count++] = d->charmap[y][x];
	      }
	    }
	  }
	}
	else {
		/* Get a linear list of monsters within pc radius*/
		for (y = near->position[dim_y] - radius; y < near->position[dim_y] + radius + 1; y++) {
			for (x = near->position[dim_x] - radius; x < near->position[dim_x] + radius + 1; x++) {
				if(x > 0 && y > 0 && x < 80 && y < 21)
				{
					if (d->charmap[y][x] && d->charmap[y][x] != d->pc) {
						(*mon_arr)[count++] = d->charmap[y][x];
					}
				}
			}
		}
	}

	return count;
}



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
  the_pc->spell_cooldown = 0;

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
