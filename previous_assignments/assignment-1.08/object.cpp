/*
 * object.cpp
 *
 *  Created on: Apr 1, 2016
 *      Author: Joe
 */

#include "object.h"
#include "dungeon.h"

int position_has_object(dungeon_t * d, uint8_t y, uint8_t x)
{
	uint32_t i;
	for(i = 0; i < d->objects.size(); i++)
	{
		if(d->objects[i]->position[dim_y] == y &&
				d->objects[i]->position[dim_x] == x)
			return 1;
	}
	return 0;
}

object * get_object_at_position(dungeon_t * d, uint8_t y, uint8_t x)
{
	uint32_t i;
	for(i = 0; i < d->objects.size(); i++)
	{
		if(d->objects[i]->position[dim_y] == y &&
				d->objects[i]->position[dim_x] == x)
			return d->objects[i];
	}
	return NULL;
}

void gen_objects(dungeon_t *d, uint32_t numobj)
{
	  uint32_t i;
	  uint32_t room;
	  pair_t p;
	  object * o;

	  for (i = 0; i < numobj; i++) {

		// generate an object from a random description
		o = d->object_descriptions[rand_range(0, d->object_descriptions.size() - 1)].generate_instance();

		// I do not want stacked objects for now
		do {
		room = rand_range(1, d->num_rooms - 1);
		p[dim_y] = rand_range(d->rooms[room].position[dim_y],
							  (d->rooms[room].position[dim_y] +
							   d->rooms[room].size[dim_y] - 1));
		p[dim_x] = rand_range(d->rooms[room].position[dim_x],
						  (d->rooms[room].position[dim_x] +
						   d->rooms[room].size[dim_x] - 1));
		}while(position_has_object(d, p[dim_y], p[dim_x]));

		o->position[dim_y] = p[dim_y];
		o->position[dim_x] = p[dim_x];

		d->objects.push_back(o);

	  }

}


