/*
  Joe Avey CS 327

  REQUIREMENTS OF ASSIGNMENT 1.04

	- add a switch, --nummon [int], that sets the number of monsters to scatter
	
	- implement monsters:
		- abilities:
			- intelligence : move on the shortest path toward the PC if they
			  know where the PC is; they remember last PC position
			  
			- telepathy : always know where the PC is, always move toward the PC
			  non-tele monsters only move toward a visible or remebered PC position
			  
			- tunneling ability : can tunnel through rock. when attempting to move 
			  through rock they subtract 85 from the rock's hardness until it reaches 0
			  when the hardness reaches 0, that position becomes a CORRIDOR, which means
			  that tunneling monsters can go to that spot. 			
			
			- erratic behavior : have a 50% chance of moving as per their other characteristics
			  otherwise, they move to a random neighboring cell (tunnelers can tunnel, non-tunneler cannot)
		- speed:
			- represented by an integer between 5-20 (PC gets 10)
			- each character goes into a priority queue, prioritized on the time of their next turn
			- each character moves every 100/speed turns
			- the game turn starts at 0 and advances to the value at the front of the priority queue every
			  time the queue is dequeued (discrete event simulator).
			- it is not necessary to keep track of the game turn since the next value in the priority queue
			  is the game turn
			  
	- The PC should move around (however I want it to) and the dungeon should be redrawn after each PC move.

	- when two characters occupy the same space, the one who moved most recently to that space, kills the
	  other. After the PC dies, or all monsters die print the win/lost status where a win is only if the
	  PC is the last remaining character.
	
*/

// INCLUDE LIBRARIES ///////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include "heap.h"

// DEFINE STATEMENTS ///////////////////////////////////
#define ROOM_X_MIN_SIZE 3
#define ROOM_Y_MIN_SIZE 2
#define ROOM_X_MAX_SIZE 10
#define ROOM_Y_MAX_SIZE 10

#define MIN_NUM_ROOMS 5
#define MAX_NUM_ROOMS 8

#define STATUS_DEAD 0
#define STATUS_ALIVE 1

#define NUM_ROWS 21
#define NUM_COLS 80

#define GAME_RUNNING 0
#define GAME_OVER 1

#define SHORTEST_PATH_RANGE 1500

#define DEFAULT_NUMMON 1

// monster ability bitfield definitions
#define ABIL_INTL 0x01
#define ABIL_TELE 0x02
#define ABIL_TUNN 0x04
#define ABIL_ERTC 0x08
#define ABIL_ISPC 0x10

// TYPE DECLARATIONS ///////////////////////////////////
enum { // dungeon node types
	ROCK,
	ROOM,
	PLAYER,
	CORRIDOR,
	IMMUTABLE
};

enum { // room building return messages
	ROOM_BUILD_SUCCESS,
	ROOM_TOO_LARGE,
	ROOM_OUT_OF_BOUNDS,
	ROOM_OVERLAP
};

enum { // room connecting return message
	CONNECT_ROOMS_SUCCESS,
	ROOM_NULL_ERROR
};

enum { // dungeon building return message
	DUNGEON_BUILD_SUCCESS,
	DUNGEON_FILEPATH_NULL,
	DUNGEON_WRONG_FILE_TYPE,
	DUNGEON_UNABLE_TO_LOAD_FILE,
	DUNGEON_NO_ROOMS_BUILT
};

enum { // dungeon saving return message
	DUNGEON_SAVE_SUCCESS,
	DUNGEON_SAVE_FILEPATH_NULL,
	DUNGEON_UNABLE_TO_SAVE_FILE,
	DUNGEON_NO_ROOMS_SAVED
};

enum { // char map init return message
	CHAR_MAP_INIT_SUCCESS,
	CHAR_MAP_PC_OOB
};

enum { // run turn queue return message
	NO_DEAD,
	PC_DEAD,
	MON_DEAD
};

typedef struct room{
	int x_pos;
	int y_pos;
	int x_size;
	int y_size;
	struct room * next_room;
}room_t;	

typedef struct {
	uint8_t type;
	uint8_t rock_hardness; // 0 for non-rocks, 1-254 for rocks, 255 for immutable
	uint8_t is_room_edge; // true if this dungeon node touches a room edge
	uint8_t has_player;
	uint8_t override_symbol;
	char overriden_char;
} dungeon_node_t;
	
typedef struct{
	int x;
	int y;
}point_t;

typedef struct {
	heap_node_t * heap_node;
	uint8_t x_pos;
	uint8_t y_pos;
	uint8_t from_x;
	uint8_t from_y;
	int32_t cost;
}vertex_path_t;	   

typedef struct {
	uint8_t speed;
	uint8_t abilities;
	point_t position;
	long next_turn;
	uint8_t status;
	point_t last_known_PC_position;
}character_t;


// FUNCTION PROTOTYPES /////////////////////////////////
int run_turn_queue(heap_t ** turn_queue);
int move_character(character_t ** cp);
int init_random_dungeon();
int init_dungeon_load(char * filepath);
int init_character_map(character_t * char_map[NUM_ROWS][NUM_COLS], point_t player_position, int nummons);
int print_dungeon(dungeon_node_t d[NUM_ROWS][NUM_COLS]);
int rand_range(int start, int end);
int try_build_room(int x_pos, int y_pos, int x_size, int y_size);
int connect_rooms(room_t * room1, room_t * room2);
int save_dungeon_to_disk(char * filepath);
void reverse_byte_order(uint8_t * array, int length);
point_t place_character(point_t * position);
void build_distances_map(dungeon_node_t d[NUM_ROWS][NUM_COLS], point_t player_position, int tunneling);
vertex_path_t * find_shortest_path(vertex_path_t map[NUM_ROWS][NUM_COLS], int from_x, int from_y, int to_x, int to_y, int tunneling);
static int32_t vertex_cost_cmp(const void *key, const void *with);
static int32_t char_turn_cmp(const void * key, const void *with);
int copy_dungeon(dungeon_node_t src[NUM_ROWS][NUM_COLS], dungeon_node_t dest[NUM_ROWS][NUM_COLS]);
void cleanup_dungeon();
point_t movement_erratic(character_t * cp);
point_t movement_shortest_path(character_t * cp, point_t to);

// GLOBAL VARS ////////////////////////////////////////
dungeon_node_t	dungeon[NUM_ROWS][NUM_COLS] = {};
dungeon_node_t distance_weights_non_tunneling[NUM_ROWS][NUM_COLS] = {};
dungeon_node_t distance_weights_tunneling[NUM_ROWS][NUM_COLS] = {};
character_t * char_map[NUM_ROWS][NUM_COLS] = {};
room_t * head_room;
int num_rooms = 0;
uint8_t load_dungeon = 0, save_dungeon = 0;
int nummon;
heap_t * turn_queue;
point_t pc_pos = {};

// FUNCTION IMPLEMENTATIONS ///////////////////////////

static int32_t vertex_cost_cmp(const void * key, const void *with)
{
	return ((vertex_path_t *) key)->cost - ((vertex_path_t *) with)->cost;
}

static int32_t char_turn_cmp(const void * key, const void *with)
{
	return ((character_t *)key)->next_turn - ((character_t *)with)->next_turn;
}
// initializes the global dungeon variable with random rooms and rock hardnesses
int init_random_dungeon()
{
  // fill the dungeon with rock
  int i, j;
  for(i = 1; i < NUM_ROWS - 1; i++)
	for(j = 1; j < NUM_COLS - 1; j++)
	{
	  dungeon[i][j].type = ROCK;
	  dungeon[i][j].rock_hardness = rand_range(1,254);
	}

  //fill the edges with immutable rock borders
  for(i = 0; i < NUM_COLS; i++)
  {
	  dungeon[0][i].type = IMMUTABLE;
	  dungeon[0][i].rock_hardness = 255;
	  dungeon[NUM_ROWS-1][i].type = IMMUTABLE;
	  dungeon[NUM_ROWS-1][i].rock_hardness = 255;
  }

  for(i = 1; i < NUM_ROWS - 1; i++)
  {
	  dungeon[i][0].type = IMMUTABLE;
	  dungeon[i][0].rock_hardness = 255;
	  dungeon[i][NUM_COLS-1].type = IMMUTABLE;
	  dungeon[i][NUM_COLS-1].rock_hardness = 255;
  }

  // build random placed and sized rooms
  int target_number_of_rooms = rand_range(MIN_NUM_ROOMS, MAX_NUM_ROOMS);
  int tries = 0;
  while(num_rooms < target_number_of_rooms && tries < 2000)
  {
	  // try to build a random positioned and sized room
	  try_build_room(rand_range(1, NUM_COLS-ROOM_X_MAX_SIZE-2), rand_range(1, NUM_ROWS-ROOM_Y_MAX_SIZE-2),
			 rand_range(ROOM_X_MIN_SIZE, ROOM_X_MAX_SIZE), rand_range(ROOM_Y_MIN_SIZE, ROOM_Y_MAX_SIZE));
	  tries++;
  }

  // connect the first room to each of the rooms
  room_t * room_to_connect = head_room;

  for(i = 1; i < num_rooms; i++)
  {
	  room_to_connect = room_to_connect->next_room;

	  connect_rooms(head_room, room_to_connect);
  }

  return DUNGEON_BUILD_SUCCESS;

}

int run_turn_queue(heap_t ** turn_queue)
{
	int i, j, ret;
	
	if(*turn_queue == NULL)
	{	
		// the size of the priority queue is the number of monsters plus the PC
		*turn_queue = malloc(sizeof(character_t) * (nummon + 1));
		heap_init(*turn_queue, char_turn_cmp, NULL);
		
		for(i = 0; i < NUM_ROWS; i++)
			for(j = 0; j < NUM_COLS; j++)
			{
				// if there is a character in this spot then make it do its first turn
				
				if(char_map[i][j] == NULL)
					continue;
				
				character_t * next_char = char_map[i][j];
			
				ret = move_character(&next_char);

				// print every move to see every character moving
				usleep(125000);
					print_dungeon(dungeon); //redraw dungeon

				// this shouldn't happen often on first turn but its possible
				if(ret == MON_DEAD)
					nummon--;
				// this shouldn't happen often on first turn but its possible
				if(ret == PC_DEAD)
					return PC_DEAD;
				
				next_char->next_turn += 100/next_char->speed;
				
				heap_insert(*turn_queue, next_char);
			}
	}
	else
	{
		character_t * next_char = heap_remove_min(*turn_queue);
		
		if(next_char->status != STATUS_DEAD)
		{
			ret = move_character(&next_char);

			if(ret == MON_DEAD)
				nummon--;

			// print every move to see every character moving
			usleep(125000);
			print_dungeon(dungeon); // redraw dungeon

			next_char->next_turn += 100/next_char->speed;

			heap_insert(*turn_queue, next_char);
		}
	}

	return ret;
}

// initializes the character map containing the PC in the position
// player_position and nummons number of monsters in random places
// you must initialize the dungeon before calling init_character_map
int init_character_map(character_t * char_map[NUM_ROWS][NUM_COLS], point_t player_position, int nummons)
{
	if((player_position.x >= NUM_COLS - 1) || (player_position.y >= NUM_COLS - 1) || (player_position.x < 1) || (player_position.y < 1))
		return CHAR_MAP_PC_OOB;

	if(!((dungeon[player_position.y][player_position.x].type == ROOM) || 
		(dungeon[player_position.y][player_position.x].type == CORRIDOR)))
		return CHAR_MAP_PC_OOB;
		
	// set the character
	char_map[player_position.y][player_position.x] = malloc(sizeof(character_t));
	char_map[player_position.y][player_position.x]->speed = 10;
	char_map[player_position.y][player_position.x]->abilities = ABIL_ISPC;
	char_map[player_position.y][player_position.x]->position = player_position;
	char_map[player_position.y][player_position.x]->next_turn = 0;
	char_map[player_position.y][player_position.x]->status = STATUS_ALIVE;
	char_map[player_position.y][player_position.x]->last_known_PC_position.x = 0;
	char_map[player_position.y][player_position.x]->last_known_PC_position.y = 0;
	pc_pos.x = char_map[player_position.y][player_position.x]->position.x;
	pc_pos.y = char_map[player_position.y][player_position.x]->position.y;
	
	while(nummons > 0)
	{
		point_t mon = {rand_range(1, NUM_COLS-2), rand_range(1, NUM_ROWS-2)};
		
		if((dungeon[mon.y][mon.x].type == ROOM) || (dungeon[mon.y][mon.x].type == CORRIDOR))
		{
			int chance_for_ability;
			
			// set the monster
			char_map[mon.y][mon.x] = malloc(sizeof(character_t));
			char_map[mon.y][mon.x]->speed = 20;//rand_range(5,20);
			char_map[mon.y][mon.x]->position = mon;
			char_map[mon.y][mon.x]->next_turn = 0;
			char_map[mon.y][mon.x]->status = STATUS_ALIVE;

			// give the monster a 50% chance to be intelligent
			chance_for_ability = rand_range(1,10);
			if(chance_for_ability < 6)
				char_map[mon.y][mon.x]->abilities |= ABIL_INTL;

			// give the monster a 50% chance to be telepathic
			chance_for_ability = rand_range(1,10);
			if(chance_for_ability < 6)
				char_map[mon.y][mon.x]->abilities |= ABIL_TELE;

			// give the monster a 50% chance to be tunneling
			chance_for_ability = rand_range(1,10);
			if(chance_for_ability < 6)
				char_map[mon.y][mon.x]->abilities |= ABIL_TUNN;

			// give the monster a 50% chance to be erratic
			chance_for_ability = rand_range(1,10);
			if(chance_for_ability < 6)
				char_map[mon.y][mon.x]->abilities |= ABIL_ERTC;
			
			if((char_map[mon.y][mon.x]->abilities & ABIL_TELE) == ABIL_TELE)
				char_map[mon.y][mon.x]->last_known_PC_position = char_map[player_position.y][player_position.x]->position;

			nummons--;
		}
	}
	return CHAR_MAP_INIT_SUCCESS;
}

int move_character(character_t ** cp)
{
	if(cp == NULL)
		return NO_DEAD;
	if(*cp == NULL)
		return NO_DEAD;

	char_map[(*cp)->position.y][(*cp)->position.x] = NULL;

	if(((*cp)->abilities & ABIL_ISPC) == ABIL_ISPC)
	{ // do PC movement

		// for now the PC moves erratically
		point_t move = movement_erratic(*cp);
		(*cp)->position.x += move.x;
		(*cp)->position.y += move.y;

		pc_pos.x = (*cp)->position.x;
		pc_pos.y = (*cp)->position.y;

		build_distances_map(distance_weights_non_tunneling, pc_pos, 0);

		//check character collisions here 4
		character_t * collisioned_char = char_map[(*cp)->position.y][(*cp)->position.x];

		char_map[(*cp)->position.y][(*cp)->position.x] = *cp;
		
		if(collisioned_char != NULL)
		{
			collisioned_char->status = STATUS_DEAD;
			return MON_DEAD;
		}
	}
	else
	{ // do monster movement

		// monster is intelligent (maybe telepathic, tunneling, and/or erratic)
		if(((*cp)->abilities & ABIL_INTL) == ABIL_INTL)
		{
			// monster is intelligent and telepatic (maybe tunneling and/or erratic)
			if(((*cp)->abilities & ABIL_TELE) == ABIL_TELE)
			{
				//monster is intelligent, telepathic, and tunneling (maybe erratic)
				if(((*cp)->abilities & ABIL_TUNN) == ABIL_TUNN)
				{
					//monster is intelligent, telepathic, tunneling, and erratic
					if(((*cp)->abilities & ABIL_ERTC) == ABIL_ERTC)
					{
						//f
						int erratic_chance = rand_range(1, 10);
						if(erratic_chance < 6)
						{
							// do erratic stuff
							point_t move = movement_erratic(*cp);
							(*cp)->position.x += move.x;
							(*cp)->position.y += move.y;
						}
						else
						{
							// do normal abilities
							point_t move = movement_shortest_path(*cp, pc_pos);
							(*cp)->position.x += move.x;
							(*cp)->position.y += move.y;
						}
					}
					//monster is intelligent, telepathic, and tunneling
					else
					{
						//7
						point_t move = movement_shortest_path(*cp, pc_pos);
						(*cp)->position.x += move.x;
						(*cp)->position.y += move.y;
					}
				}
				//monster is intelligent, telepathic, and erratic
				else if (((*cp)->abilities & ABIL_ERTC) == ABIL_ERTC)
				{
					//b
					int erratic_chance = rand_range(1, 10);
					if(erratic_chance < 6)
					{
						// do erratic stuff
						point_t move = movement_erratic(*cp);
						(*cp)->position.x += move.x;
						(*cp)->position.y += move.y;
					}
					else
					{
						// do normal abilities
						point_t move = movement_shortest_path(*cp, pc_pos);
						(*cp)->position.x += move.x;
						(*cp)->position.y += move.y;
					}
				}
				else //monster is intelligent and telepathic only
				{
					//3
					point_t move = movement_shortest_path(*cp, pc_pos);
					(*cp)->position.x += move.x;
					(*cp)->position.y += move.y;
				}
			}
			// monster is intelligent and tunneling (maybe erratic)
			else if(((*cp)->abilities & ABIL_TUNN) == ABIL_TUNN)
			{
				//monster is intelligent, tunneling, and erratic
				if(((*cp)->abilities & ABIL_ERTC) == ABIL_ERTC)
				{
					//d
					int erratic_chance = rand_range(1, 10);
					if(erratic_chance < 6)
					{
						// do erratic stuff
						point_t move = movement_erratic(*cp);
						(*cp)->position.x += move.x;
						(*cp)->position.y += move.y;
					}
					else
					{
						// do normal abilities
						point_t move = movement_shortest_path(*cp, pc_pos);
						(*cp)->position.x += move.x;
						(*cp)->position.y += move.y;
					}
				}
				// monster is intelligent and tunneling only
				else
				{
					//5
					point_t move = movement_shortest_path(*cp, pc_pos);
					(*cp)->position.x += move.x;
					(*cp)->position.y += move.y;
				}
			}
			// monster is intelligent and erratic
			else if(((*cp)->abilities & ABIL_ERTC) == ABIL_ERTC)
			{
				//9
				int erratic_chance = rand_range(1, 10);
				if(erratic_chance < 6)
				{
					// do erratic stuff
					point_t move = movement_erratic(*cp);
					(*cp)->position.x += move.x;
					(*cp)->position.y += move.y;
				}
				else
				{
					// do normal abilities
					point_t move = movement_shortest_path(*cp, pc_pos);
					(*cp)->position.x += move.x;
					(*cp)->position.y += move.y;
				}
			}
			else // monster is only intelligent
			{
				//1
				point_t move = movement_shortest_path(*cp, pc_pos);
				(*cp)->position.x += move.x;
				(*cp)->position.y += move.y;
			}
		}
		// monster is telepathic but not intelligent (maybe tunneling and/or erratic)
		else if(((*cp)->abilities & ABIL_TELE) == ABIL_TELE)
		{
			// monster is telepathic and tunneling (maybe erratic)
			if(((*cp)->abilities & ABIL_TUNN) == ABIL_TUNN)
			{
				// monster is telepathic, tunneling, and erratic
				if(((*cp)->abilities & ABIL_ERTC) == ABIL_ERTC)
				{
					//e
					int erratic_chance = rand_range(1, 10);
					if(erratic_chance < 6)
					{
						// do erratic stuff
						point_t move = movement_erratic(*cp);
						(*cp)->position.x += move.x;
						(*cp)->position.y += move.y;
					}
					else
					{
						// do normal abilities
						point_t move = movement_shortest_path(*cp, pc_pos);
						(*cp)->position.x += move.x;
						(*cp)->position.y += move.y;
					}
				}
				// monster is telepathic and tunneling only
				else
				{
					//6
					point_t move = movement_shortest_path(*cp, pc_pos);
					(*cp)->position.x += move.x;
					(*cp)->position.y += move.y;
				}
			}
			// monster is telepathic and erratic only
			else if(((*cp)->abilities & ABIL_ERTC) == ABIL_ERTC)
			{
				//a
				int erratic_chance = rand_range(1, 10);
				if(erratic_chance < 6)
				{
					// do erratic stuff
					point_t move = movement_erratic(*cp);
					(*cp)->position.x += move.x;
					(*cp)->position.y += move.y;
				}
				else
				{
					// do normal abilities
					point_t move = movement_shortest_path(*cp, pc_pos);
					(*cp)->position.x += move.x;
					(*cp)->position.y += move.y;
				}
			}
			else // monster is only telepathic
			{
				//2
				point_t move = movement_shortest_path(*cp, pc_pos);
				(*cp)->position.x += move.x;
				(*cp)->position.y += move.y;
			}
		}
		// monster tunnels but is not intelligent or telepathic
		else if(((*cp)->abilities & ABIL_TUNN) == ABIL_TUNN)
		{
			// monster is both tunneling and erratic
			if(((*cp)->abilities & ABIL_ERTC) == ABIL_ERTC)
			{
				//c
				int erratic_chance = rand_range(1, 10);
				if(erratic_chance < 6)
				{
					// do erratic stuff
					point_t move = movement_erratic(*cp);
					(*cp)->position.x += move.x;
					(*cp)->position.y += move.y;
				}
				else
				{
					// do normal abilities
					point_t move = movement_erratic(*cp);
					(*cp)->position.x += move.x;
					(*cp)->position.y += move.y;
				}
			}
			else // monster is just tunneling
			{
				//4
				point_t move = movement_erratic(*cp);
				(*cp)->position.x += move.x;
				(*cp)->position.y += move.y;
			}
		}
		//monster has no abilities, except maybe erratic, but they will move randomly anyway
		else
		{
			//8 & 0
			point_t move = movement_erratic(*cp);
			(*cp)->position.x += move.x;
			(*cp)->position.y += move.y;
		}
		
		//check character collisions here
		character_t * collisioned_char = char_map[(*cp)->position.y][(*cp)->position.x];

		char_map[(*cp)->position.y][(*cp)->position.x] = *cp;

		if(collisioned_char != NULL)
		{
			collisioned_char->status = STATUS_DEAD;
			if((collisioned_char->abilities & ABIL_ISPC) == ABIL_ISPC)
				return PC_DEAD;

			return MON_DEAD;
		}
	}
		
	return NO_DEAD;
}

point_t movement_shortest_path(character_t * cp, point_t to)
{
	int i, j;
	point_t move = {0, 0};
	int min_path = 5000;
	if((cp->abilities & ABIL_TUNN) == ABIL_TUNN)
	{
		dungeon_node_t * dn = &dungeon[cp->position.y][cp->position.x];
		for(i = -1; i <= 1; i++)
		{
			for(j = -1; j <= 1; j++)
			{
				if((i == 0) &&(j == 0))
					continue;
				int current_path = distance_weights_tunneling[cp->position.y + i][cp->position.x + j].overriden_char;
				if(current_path <= '9')
					current_path -= 48;
				else if(current_path <= 'Z')
					current_path -= 29;
				else if(current_path <= 'z')
					current_path -= 87;

				if(current_path < min_path)
				{
					dn = &dungeon[cp->position.y + i][cp->position.x + j];
					move.y = i;
					move.x = j;
					min_path = current_path;
				}
			}
		}
		if(dn->type == ROCK)
		{
			if(dn->rock_hardness <= 85)
				dn->rock_hardness = 0;
			else
				dn->rock_hardness -= 85;
			if(dn->rock_hardness <= 0)
			{
				dn->rock_hardness = 0;
				dn->type = CORRIDOR;
				build_distances_map(distance_weights_non_tunneling, pc_pos, 0);
			}
			else
			{
				move.y = 0;
				move.x = 0;
			}
			build_distances_map(distance_weights_tunneling, pc_pos, 1);
		}
	}
	else // non-tunneling
	{
		for(i = -1; i <= 1; i++)
		{
			for(j = -1; j <= 1; j++)
			{
				if((i == 0) &&(j == 0))
					continue;
				if(!(distance_weights_non_tunneling[cp->position.y + i][cp->position.x + j].override_symbol))
					continue;
				int current_path = distance_weights_non_tunneling[cp->position.y + i][cp->position.x + j].overriden_char;
				if(current_path <= '9') // 57
					current_path -= 48;
				else if(current_path <= 'Z') // 90
					current_path -= 29;
				else if(current_path <= 'z') // 122
					current_path -= 87;

				if(current_path < min_path)
				{
					move.y = i;
					move.x = j;
					min_path = current_path;
				}
			}
		}
	}

	if((dungeon[cp->position.y + move.y][cp->position.x + move.x].type == IMMUTABLE) || (dungeon[cp->position.y + move.y][cp->position.x + move.x].type == ROCK))
	{
		move.x = 0;
		move.y = 0;
	}

	// dont move out of bounds
	if((cp->position.y + move.y <= 0) || (cp->position.x + move.x <= 0))
	{
		move.x = 0;
		move.y = 0;
	}

	return move;
}

point_t movement_erratic(character_t * cp)
{
	point_t movement = {};

	int rand_move = rand_range(0, 7);

	if(rand_move == 0)
	{
		if(((dungeon[cp->position.y][cp->position.x - 1].type == ROOM) || (dungeon[cp->position.y][cp->position.x - 1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.x -= 1;
		}
		else if(((dungeon[cp->position.y-1][cp->position.x].type == ROOM) || (dungeon[cp->position.y - 1][cp->position.x].type == CORRIDOR))|| ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y -= 1;
		}
		else if(((dungeon[cp->position.y][cp->position.x + 1].type == ROOM) || (dungeon[cp->position.y][cp->position.x + 1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.x += 1;
		}
		else if(((dungeon[cp->position.y+1][cp->position.x].type == ROOM) || (dungeon[cp->position.y + 1][cp->position.x].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y += 1;
		}
		else if(((dungeon[cp->position.y+1][cp->position.x+1].type == ROOM) || (dungeon[cp->position.y+1][cp->position.x+1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y += 1;
			movement.x += 1;
		}
		else if(((dungeon[cp->position.y+1][cp->position.x-1].type == ROOM) || (dungeon[cp->position.y+1][cp->position.x-1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y += 1;
			movement.x -= 1;
		}
		else if(((dungeon[cp->position.y-1][cp->position.x+1].type == ROOM) || (dungeon[cp->position.y-1][cp->position.x+1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y -= 1;
			movement.x += 1;
		}
		else if(((dungeon[cp->position.y-1][cp->position.x-1].type == ROOM) || (dungeon[cp->position.y-1][cp->position.x-1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y -= 1;
			movement.x -= 1;
		}

	}

	if(rand_move == 1)
	{
		if(((dungeon[cp->position.y-1][cp->position.x].type == ROOM) || (dungeon[cp->position.y-1][cp->position.x].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y -= 1;
		}
		else if(((dungeon[cp->position.y][cp->position.x - 1].type == ROOM) || (dungeon[cp->position.y][cp->position.x-1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.x -= 1;
		}
		else if(((dungeon[cp->position.y][cp->position.x + 1].type == ROOM) || (dungeon[cp->position.y][cp->position.x + 1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.x += 1;
		}
		else if(((dungeon[cp->position.y+1][cp->position.x].type == ROOM) || (dungeon[cp->position.y+1][cp->position.x].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y += 1;
		}
		else if(((dungeon[cp->position.y+1][cp->position.x+1].type == ROOM) || (dungeon[cp->position.y+1][cp->position.x+1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y += 1;
			movement.x += 1;
		}
		else if(((dungeon[cp->position.y+1][cp->position.x-1].type == ROOM) || (dungeon[cp->position.y+1][cp->position.x-1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y += 1;
			movement.x -= 1;
		}
		else if(((dungeon[cp->position.y-1][cp->position.x+1].type == ROOM) || (dungeon[cp->position.y-1][cp->position.x+1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y -= 1;
			movement.x += 1;
		}
		else if(((dungeon[cp->position.y-1][cp->position.x-1].type == ROOM) || (dungeon[cp->position.y-1][cp->position.x-1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y -= 1;
			movement.x -= 1;
		}
	}

	if(rand_move == 2)
	{
		if(((dungeon[cp->position.y][cp->position.x + 1].type == ROOM) || (dungeon[cp->position.y][cp->position.x + 1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.x += 1;
		}
		else if(((dungeon[cp->position.y][cp->position.x - 1].type == ROOM) || (dungeon[cp->position.y][cp->position.x - 1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.x -= 1;
		}
		else if((dungeon[cp->position.y-1][cp->position.x].type == ROOM) || (dungeon[cp->position.y-1][cp->position.x].type == CORRIDOR))
		{
			movement.y -= 1;
		}
		else if(((dungeon[cp->position.y+1][cp->position.x].type == ROOM) || (dungeon[cp->position.y + 1][cp->position.x].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y += 1;
		}
		else if(((dungeon[cp->position.y+1][cp->position.x+1].type == ROOM) || (dungeon[cp->position.y+1][cp->position.x+1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y += 1;
			movement.x += 1;
		}
		else if(((dungeon[cp->position.y+1][cp->position.x-1].type == ROOM) || (dungeon[cp->position.y+1][cp->position.x-1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y += 1;
			movement.x -= 1;
		}
		else if(((dungeon[cp->position.y-1][cp->position.x+1].type == ROOM) || (dungeon[cp->position.y-1][cp->position.x+1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y -= 1;
			movement.x += 1;
		}
		else if(((dungeon[cp->position.y-1][cp->position.x-1].type == ROOM) || (dungeon[cp->position.y-1][cp->position.x-1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y -= 1;
			movement.x -= 1;
		}
	}

	if(rand_move == 3)
	{
		if(((dungeon[cp->position.y+1][cp->position.x].type == ROOM) || (dungeon[cp->position.y + 1][cp->position.x].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y += 1;
		}
		else if(((dungeon[cp->position.y][cp->position.x - 1].type == ROOM) || (dungeon[cp->position.y][cp->position.x - 1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.x -= 1;
		}
		else if(((dungeon[cp->position.y-1][cp->position.x].type == ROOM) || (dungeon[cp->position.y - 1][cp->position.x].type == CORRIDOR))|| ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y -= 1;
		}
		else if(((dungeon[cp->position.y][cp->position.x + 1].type == ROOM) || (dungeon[cp->position.y][cp->position.x + 1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.x += 1;
		}
		else if(((dungeon[cp->position.y+1][cp->position.x+1].type == ROOM) || (dungeon[cp->position.y+1][cp->position.x+1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y += 1;
			movement.x += 1;
		}
		else if(((dungeon[cp->position.y+1][cp->position.x-1].type == ROOM) || (dungeon[cp->position.y+1][cp->position.x-1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y += 1;
			movement.x -= 1;
		}
		else if(((dungeon[cp->position.y-1][cp->position.x+1].type == ROOM) || (dungeon[cp->position.y-1][cp->position.x+1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y -= 1;
			movement.x += 1;
		}
		else if(((dungeon[cp->position.y-1][cp->position.x-1].type == ROOM) || (dungeon[cp->position.y-1][cp->position.x-1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y -= 1;
			movement.x -= 1;
		}
	}
	if(rand_move == 4)
	{
		if(((dungeon[cp->position.y+1][cp->position.x-1].type == ROOM) || (dungeon[cp->position.y+1][cp->position.x-1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y += 1;
			movement.x -= 1;
		}
		else if(((dungeon[cp->position.y+1][cp->position.x].type == ROOM) || (dungeon[cp->position.y + 1][cp->position.x].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y += 1;
		}
		else if(((dungeon[cp->position.y][cp->position.x - 1].type == ROOM) || (dungeon[cp->position.y][cp->position.x - 1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.x -= 1;
		}
		else if(((dungeon[cp->position.y-1][cp->position.x].type == ROOM) || (dungeon[cp->position.y - 1][cp->position.x].type == CORRIDOR))|| ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y -= 1;
		}
		else if(((dungeon[cp->position.y][cp->position.x + 1].type == ROOM) || (dungeon[cp->position.y][cp->position.x + 1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.x += 1;
		}
		else if(((dungeon[cp->position.y+1][cp->position.x+1].type == ROOM) || (dungeon[cp->position.y+1][cp->position.x+1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y += 1;
			movement.x += 1;
		}
		else if(((dungeon[cp->position.y-1][cp->position.x+1].type == ROOM) || (dungeon[cp->position.y-1][cp->position.x+1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y -= 1;
			movement.x += 1;
		}
		else if(((dungeon[cp->position.y-1][cp->position.x-1].type == ROOM) || (dungeon[cp->position.y-1][cp->position.x-1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y -= 1;
			movement.x -= 1;
		}
	}
	if(rand_move == 5)
	{
		if(((dungeon[cp->position.y-1][cp->position.x-1].type == ROOM) || (dungeon[cp->position.y-1][cp->position.x-1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y -= 1;
			movement.x -= 1;
		}
		else if(((dungeon[cp->position.y+1][cp->position.x-1].type == ROOM) || (dungeon[cp->position.y+1][cp->position.x-1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y += 1;
			movement.x -= 1;
		}
		else if(((dungeon[cp->position.y+1][cp->position.x].type == ROOM) || (dungeon[cp->position.y + 1][cp->position.x].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y += 1;
		}
		else if(((dungeon[cp->position.y][cp->position.x - 1].type == ROOM) || (dungeon[cp->position.y][cp->position.x - 1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.x -= 1;
		}
		else if(((dungeon[cp->position.y-1][cp->position.x].type == ROOM) || (dungeon[cp->position.y - 1][cp->position.x].type == CORRIDOR))|| ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y -= 1;
		}
		else if(((dungeon[cp->position.y][cp->position.x + 1].type == ROOM) || (dungeon[cp->position.y][cp->position.x + 1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.x += 1;
		}
		else if(((dungeon[cp->position.y+1][cp->position.x+1].type == ROOM) || (dungeon[cp->position.y+1][cp->position.x+1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y += 1;
			movement.x += 1;
		}
		else if(((dungeon[cp->position.y-1][cp->position.x+1].type == ROOM) || (dungeon[cp->position.y-1][cp->position.x+1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y -= 1;
			movement.x += 1;
		}
	}
	if(rand_move == 7)
	{
		if(((dungeon[cp->position.y+1][cp->position.x+1].type == ROOM) || (dungeon[cp->position.y+1][cp->position.x+1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y += 1;
			movement.x += 1;
		}
		else if(((dungeon[cp->position.y-1][cp->position.x+1].type == ROOM) || (dungeon[cp->position.y-1][cp->position.x+1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y -= 1;
			movement.x += 1;
		}
		else if(((dungeon[cp->position.y-1][cp->position.x-1].type == ROOM) || (dungeon[cp->position.y-1][cp->position.x-1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y -= 1;
			movement.x -= 1;
		}
		else if(((dungeon[cp->position.y+1][cp->position.x-1].type == ROOM) || (dungeon[cp->position.y+1][cp->position.x-1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y += 1;
			movement.x -= 1;
		}
		else if(((dungeon[cp->position.y+1][cp->position.x].type == ROOM) || (dungeon[cp->position.y + 1][cp->position.x].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y += 1;
		}
		else if(((dungeon[cp->position.y][cp->position.x - 1].type == ROOM) || (dungeon[cp->position.y][cp->position.x - 1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.x -= 1;
		}
		else if(((dungeon[cp->position.y-1][cp->position.x].type == ROOM) || (dungeon[cp->position.y - 1][cp->position.x].type == CORRIDOR))|| ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.y -= 1;
		}
		else if(((dungeon[cp->position.y][cp->position.x + 1].type == ROOM) || (dungeon[cp->position.y][cp->position.x + 1].type == CORRIDOR)) || ((cp->abilities & ABIL_TUNN) == ABIL_TUNN))
		{
			movement.x += 1;
		}

	}
	// dont move out of bounds
	if((cp->position.y + movement.y <= 0) || (cp->position.x + movement.x <= 0))
	{
		movement.x = 0;
		movement.y = 0;
		return movement;
	}

	if(((cp->abilities & ABIL_TUNN) == ABIL_TUNN) && (dungeon[cp->position.y + movement.y][cp->position.x + movement.x].type == ROCK))
	{
		dungeon_node_t * dn = &dungeon[cp->position.y + movement.y][cp->position.x + movement.x];
		if(dn->rock_hardness <= 85)
			dn->rock_hardness = 0;
		else
			dn->rock_hardness -= 85;
		if(dn->rock_hardness <= 0)
		{
			dn->rock_hardness = 0;
			dn->type = CORRIDOR;
			build_distances_map(distance_weights_non_tunneling, pc_pos, 0);
		}
		else
		{
			movement.y = 0;
			movement.x = 0;
		}
		build_distances_map(distance_weights_tunneling, pc_pos, 1);
	}

	return movement;
}

// creates a map of distances from player_position in the parameter d. The distances are represented as 
// as integers in number of steps away from the character
void build_distances_map(dungeon_node_t d[NUM_ROWS][NUM_COLS], point_t player_position, int tunneling)
{
	copy_dungeon(d, dungeon);
	
	int i, j;
	
	for(i = 1; i < NUM_ROWS - 1; i++)
	for(j = 1; j < NUM_COLS - 1; j++)
	{
		vertex_path_t map[NUM_ROWS][NUM_COLS], *shortest_path;
		
		shortest_path = find_shortest_path(map, player_position.x, player_position.y, j, i, tunneling);
		
		if((shortest_path == NULL) || (shortest_path->cost > SHORTEST_PATH_RANGE)) // a path could not be found between (to) and (from) or the path isn't in the range
			continue;
		
		d[i][j].override_symbol = 1;
		if(shortest_path->cost < 10)
			d[i][j].overriden_char = shortest_path->cost + 48;
		else if(shortest_path->cost < 36)
			d[i][j].overriden_char = shortest_path->cost + 87;
		else
			d[i][j].overriden_char = shortest_path->cost + 29;
	}
}

// attempts to place the PC in a given point_t dungeon spot
// if the spot is unacceptable, or position == NULL then the PC is placed randomly
// acceptable spots include ROOM and CORRIDOR
point_t place_character(point_t * position)
{
	int x_pos, y_pos;
	
	if(position == NULL)
	{
		x_pos = rand_range(1, NUM_COLS-2);
		y_pos = rand_range(1, NUM_ROWS-2);
	}
	else if (position->x > NUM_COLS-2 || position->x < 1 || position->y > NUM_ROWS-2 || position->y < 1)
	{
		x_pos = rand_range(1, NUM_COLS-2);
		y_pos = rand_range(1, NUM_ROWS-2);
	}
	else
	{
		x_pos = position->x;
		y_pos = position->y;
	}
	do
	{
		if(dungeon[y_pos][x_pos].type == ROOM || dungeon[y_pos][x_pos].type == CORRIDOR)
		{
			dungeon[y_pos][x_pos].has_player = 1;
			break;
		}
		x_pos = rand_range(1, NUM_COLS-2);
		y_pos = rand_range(1, NUM_ROWS-2);
	}while(1);
	
	point_t ret = {x_pos, y_pos};
	
	return ret;
}

// loads the dungeon represented by the file with path 'filepath'
int init_dungeon_load(char * filepath)
{
	if(filepath == NULL)
	return DUNGEON_FILEPATH_NULL;

	FILE * dungeon_file = fopen(filepath, "r");
	
	if(dungeon_file	 == NULL)
	return DUNGEON_UNABLE_TO_LOAD_FILE;

	// file-type marker is 6 bytes + NULL
	char file_type_marker[7] = {};

	// check the file type marker
	fgets(file_type_marker, 7, dungeon_file);
	if(strcmp(file_type_marker, "RLG327"))
	return DUNGEON_WRONG_FILE_TYPE;

	// get the file version number
	char file_version_marker_str[5] = {};
	fgets(file_version_marker_str, 5, dungeon_file);
	reverse_byte_order(file_version_marker_str, 4);
	uint32_t * file_version_number = (uint32_t *) file_version_marker_str;
	

	// get the size of the file
	char file_size_str[5] = {};
	fgets(file_size_str, 5, dungeon_file);
	reverse_byte_order(file_size_str, 4); // reverse byte ordering for big-endianess
	uint32_t * file_size = (uint32_t *) file_size_str;

	// read in the dungeon cell values
	int i,j;
	for(i = 1; i < (NUM_ROWS-1); i++)
	{
	for(j = 1; j < (NUM_COLS-1); j++)
	{
		char current_cell_str[2] = {};
		uint8_t current_cell_hardness = fgetc(dungeon_file);

		// interpret all cells of 0 rock hardness as corridors
		// rooms are added later
		if(current_cell_hardness == 0)		
		dungeon[i][j].type = CORRIDOR;
				
		else 
		dungeon[i][j].type = ROCK;		

		dungeon[i][j].rock_hardness = current_cell_hardness;
		dungeon[i][j].is_room_edge = 0;

	}
	
	}

	//fill the edges with immutable rock borders
	for(i = 0; i < NUM_COLS; i++)
	{
	dungeon[0][i].type = IMMUTABLE;
	dungeon[0][i].rock_hardness = 255;
	dungeon[NUM_ROWS-1][i].type = IMMUTABLE;
	dungeon[NUM_ROWS-1][i].rock_hardness = 255;
	}
	for(i = 1; i < NUM_ROWS - 1; i++)
	{
	dungeon[i][0].type = IMMUTABLE;
	dungeon[i][0].rock_hardness = 255;
	dungeon[i][NUM_COLS-1].type = IMMUTABLE;
	dungeon[i][NUM_COLS-1].rock_hardness = 255;
	}

	//build a temporary list of rooms
	room_t * temp_room_list = NULL;
	room_t * current_room = temp_room_list;

	if(feof(dungeon_file))
	{
	fclose(dungeon_file);
	return DUNGEON_NO_ROOMS_BUILT;
	}

	char current_room_x_pos_str[2] = {};	

	int temp_num_rooms = 0;
	
	//read in the room data
	while(fgets(current_room_x_pos_str, 2, dungeon_file) != NULL)
	{
	//read current x position
	uint8_t * current_room_x_pos = (uint8_t *) current_room_x_pos_str;

	//read current y position
	char current_room_y_pos_str[2] = {};
	if(fgets(current_room_y_pos_str, 2, dungeon_file) == NULL)
		printf("prob2\n");
	uint8_t * current_room_y_pos = (uint8_t *) current_room_y_pos_str;

	//read in width
	char current_room_width_str[2] = {};
	if(fgets(current_room_width_str, 2, dungeon_file) == NULL)
		printf("prob3\n");
	uint8_t * current_room_x_size = (uint8_t *) current_room_width_str;

	//read in height
	char current_room_height_str[2] = {};
	if(fgets(current_room_height_str, 2, dungeon_file) == NULL)
		printf("prob4\n");
	uint8_t * current_room_y_size = (uint8_t *) current_room_height_str;

	if(current_room != NULL)
	{
	
		while(current_room->next_room != NULL)
		current_room = current_room->next_room;
	
		current_room->next_room = malloc(sizeof(room_t));
		current_room->next_room->x_pos = *current_room_x_pos;
		current_room->next_room->y_pos = *current_room_y_pos;
		current_room->next_room->x_size = *current_room_x_size;
		current_room->next_room->y_size = *current_room_y_size;
		current_room->next_room->next_room = NULL;
	}
	else // current room is NULL (a.k.a. it's the first room)
	{
		temp_room_list = malloc(sizeof(room_t));
	
		temp_room_list->x_pos = *current_room_x_pos;
		temp_room_list->y_pos = *current_room_y_pos;
		temp_room_list->x_size = *current_room_x_size;
		temp_room_list->y_size = *current_room_y_size;
		temp_room_list->next_room = NULL;
		current_room = temp_room_list;
	}
		temp_num_rooms++;
	}
	
	current_room = temp_room_list;

	if(current_room == NULL)
	{
	fclose(dungeon_file);
	return DUNGEON_NO_ROOMS_BUILT;
	}
	
	
	while(current_room != NULL)
	{
	int room_error = try_build_room(current_room->x_pos, current_room->y_pos, current_room->x_size, current_room->y_size);
	
	if(room_error != ROOM_BUILD_SUCCESS)
		printf("%d\n", room_error);
	
	   current_room = current_room->next_room;
	}

	// the rooms do not need to be connected by this function
	// because the corridors are already given in the loaded dungeon file
	
	//free the temporary rooms list
	current_room = temp_room_list;
	room_t * helper = current_room;
	while(current_room != NULL)
	{
	helper = current_room->next_room;
	free(current_room);
	current_room = helper;
	}

	fclose(dungeon_file);

	return DUNGEON_BUILD_SUCCESS;
}

int save_dungeon_to_disk(char * filepath)
{
	if(filepath == NULL)
	return DUNGEON_SAVE_FILEPATH_NULL;

	FILE * dungeon_file = fopen(filepath, "w");
	fpos_t save_pos;
	  
	if(dungeon_file	 == NULL)
	return DUNGEON_UNABLE_TO_SAVE_FILE;

	// calculate size of this file
	uint32_t file_size = 6 /*RLG327*/+ 4 /*version marker*/ + 4 /*file size*/ + ((NUM_COLS - 2) * (NUM_ROWS -2)) /*dungeon size*/
	+ (num_rooms * 4)  /*total rooms size*/;

	uint32_t version_marker = 0;
	
	if(fprintf(dungeon_file, "RLG327") < 0) printf("Error: fprintf returned negative.\n"); // file type marker
	reverse_byte_order((uint8_t *) &file_size, 4); reverse_byte_order((uint8_t *) &version_marker, 4); // reverse byte order for big-endianess
	if(fwrite(&version_marker, 4, 1, dungeon_file) < 0) printf("Error: fwrite returned negative.\n"); // version marker
	if(fwrite(&file_size, 4, 1, dungeon_file) < 0) printf("Error: fwrite returned negative.\n"); // file size

	// write the dungeon cell hardnesses (for the class requirements this should be 1482 bytes total)
	int i,j;

	for(i = 1; i < (NUM_ROWS - 1); i++)
	for(j = 1; j < (NUM_COLS - 1); j++)
	{
		if(fwrite(&(dungeon[i][j].rock_hardness), 1, 1, dungeon_file) < 0)
		printf("Error: fwrite returned negative.\n");
	}

	//write the list of rooms
	room_t * current_room = head_room;

	if(current_room == NULL)
	{
	fclose(dungeon_file);
	return DUNGEON_NO_ROOMS_SAVED;
	}
	
	while(current_room != NULL)
	{
		if(fwrite(&(current_room->x_pos), 1, 1, dungeon_file) < 0) printf("Error: fwrite returned negative.\n"); // x pos
		if(fwrite(&(current_room->y_pos), 1, 1, dungeon_file) < 0) printf("Error: fwrite returned negative.\n"); // y pos
		if(fwrite(&(current_room->x_size), 1, 1, dungeon_file) < 0) printf("Error: fwrite returned negative.\n"); // width
		if(fwrite(&(current_room->y_size), 1, 1, dungeon_file) < 0) printf("Error: fwrite returned negative.\n"); // height

		current_room = current_room->next_room;
	}
	
	fclose(dungeon_file);
	
	return DUNGEON_SAVE_SUCCESS;
}

// returns a shortest path given a starting point (from) and an end point (to)
// param 'map' should be an empty vertex_path_t[][] and can be used to traverse a returned shortest path
// 'tunneling' 	=> 0 returns a non-tunneling shortest_path (if it exists otherwise it returns NULL)
//				=> 1 returns a tunneling shortest_path
vertex_path_t * find_shortest_path(vertex_path_t map[NUM_ROWS][NUM_COLS], int from_x, int from_y, int to_x, int to_y, int tunneling)
{
		int i, j;

	// fill all vertexes with "infinity" or max int in this case
	for(i = 0; i < NUM_ROWS; i++)
	{
		for(j = 0; j < NUM_COLS; j++)
		{
			map[i][j].x_pos = j;
			map[i][j].y_pos = i;
			map[i][j].cost = INT_MAX;
		}
	}

	// now set the PC cell to 0
	map[from_y][from_x].cost = 0;

	// init priority queue/heap
	heap_t heap;
	heap_init(&heap, vertex_cost_cmp, NULL);

	// fill the priority queue
	for(i = 0; i < NUM_ROWS; i++)
		for(j = 0; j < NUM_COLS; j++)
		{
			if(tunneling)
				if((dungeon[i][j].type != IMMUTABLE) && (dungeon[i][j].rock_hardness < 255))
					map[i][j].heap_node = heap_insert(&heap, &map[i][j]);
				else
					map[i][j].heap_node = NULL;
			else
				if(((dungeon[i][j].type == ROOM) || (dungeon[i][j].type == CORRIDOR)) && (dungeon[i][j].rock_hardness < 255))
					map[i][j].heap_node = heap_insert(&heap, &map[i][j]);
				else
					map[i][j].heap_node = NULL;
		}

	vertex_path_t * v;
	
	int count = heap.size;
	
	while((v = heap_remove_min(&heap)))
	{
		count--;
		v->heap_node = NULL;

		// check if the path has been found
		if((v->x_pos == to_x) &&(v->y_pos == to_y))
		{
			heap_delete(&heap);
			return v;
		}

		//explore top cell
		if((map[v->y_pos - 1][v->x_pos].heap_node) && (map[v->y_pos - 1][v->x_pos].cost > 
			(v->cost + ((dungeon[v->y_pos - 1][v->x_pos].rock_hardness < 85 || !tunneling)? 1 : 
			((dungeon[v->y_pos - 1][v->x_pos].rock_hardness < 171)? 2 : 3)))))
		{
			map[v->y_pos - 1][v->x_pos].cost = (v->cost + ((dungeon[v->y_pos - 1][v->x_pos].rock_hardness < 85 || !tunneling)? 1 : 
											((dungeon[v->y_pos - 1][v->x_pos].rock_hardness < 171)? 2 : 3)));
			
			map[v->y_pos - 1][v->x_pos].from_y = v->y_pos;
			map[v->y_pos - 1][v->x_pos].from_x = v->x_pos;
			heap_decrease_key_no_replace(&heap, map[v->y_pos - 1][v->x_pos].heap_node);
		}
		
		//explore top-left cell
		if((map[v->y_pos - 1][v->x_pos - 1].heap_node) && (map[v->y_pos - 1][v->x_pos - 1].cost >
			(v->cost + ((dungeon[v->y_pos - 1][v->x_pos - 1].rock_hardness < 85 || !tunneling)? 1 : 
			((dungeon[v->y_pos - 1][v->x_pos - 1].rock_hardness < 171)? 2 : 3)))))
		{
			map[v->y_pos - 1][v->x_pos - 1].cost = (v->cost + ((dungeon[v->y_pos - 1][v->x_pos - 1].rock_hardness < 85 || !tunneling)? 1 : 
											((dungeon[v->y_pos - 1][v->x_pos - 1].rock_hardness < 171)? 2 : 3)));
			map[v->y_pos - 1][v->x_pos - 1].from_y = v->y_pos;
			map[v->y_pos - 1][v->x_pos - 1].from_x = v->x_pos;
			heap_decrease_key_no_replace(&heap, map[v->y_pos - 1][v->x_pos - 1].heap_node);
		}
		
		//explore left cell
		if((map[v->y_pos][v->x_pos - 1].heap_node) && (map[v->y_pos][v->x_pos - 1].cost >
			(v->cost + ((dungeon[v->y_pos][v->x_pos - 1].rock_hardness < 85 || !tunneling)? 1 : 
			((dungeon[v->y_pos][v->x_pos - 1].rock_hardness < 171)? 2 : 3)))))
		{
			map[v->y_pos][v->x_pos - 1].cost = (v->cost + ((dungeon[v->y_pos][v->x_pos - 1].rock_hardness < 85 || !tunneling)? 1 : 
											((dungeon[v->y_pos][v->x_pos - 1].rock_hardness < 171)? 2 : 3)));
			map[v->y_pos][v->x_pos - 1].from_y = v->y_pos;
			map[v->y_pos][v->x_pos - 1].from_x = v->x_pos;
			heap_decrease_key_no_replace(&heap, map[v->y_pos][v->x_pos - 1].heap_node);
		}
		
		//explore bottom-left cell
		if((map[v->y_pos + 1][v->x_pos - 1].heap_node) && (map[v->y_pos + 1][v->x_pos - 1].cost > 
			(v->cost + ((dungeon[v->y_pos + 1][v->x_pos - 1].rock_hardness < 85 || !tunneling)? 1 : 
			((dungeon[v->y_pos + 1][v->x_pos - 1].rock_hardness < 171)? 2 : 3)))))
		{
			map[v->y_pos + 1][v->x_pos - 1].cost = (v->cost + ((dungeon[v->y_pos + 1][v->x_pos - 1].rock_hardness < 85 || !tunneling)? 1 : 
											((dungeon[v->y_pos + 1][v->x_pos - 1].rock_hardness < 171)? 2 : 3)));
			map[v->y_pos + 1][v->x_pos - 1].from_y = v->y_pos;
			map[v->y_pos + 1][v->x_pos - 1].from_x = v->x_pos;
			heap_decrease_key_no_replace(&heap, map[v->y_pos + 1][v->x_pos - 1].heap_node);
		}
		
		//explore bottom cell
		if((map[v->y_pos + 1][v->x_pos].heap_node) && (map[v->y_pos + 1][v->x_pos].cost >
			(v->cost + ((dungeon[v->y_pos + 1][v->x_pos].rock_hardness < 85 || !tunneling)? 1 : 
			((dungeon[v->y_pos + 1][v->x_pos].rock_hardness < 171)? 2 : 3)))))
		{
			map[v->y_pos + 1][v->x_pos].cost = (v->cost + ((dungeon[v->y_pos + 1][v->x_pos].rock_hardness < 85 || !tunneling)? 1 : 
											((dungeon[v->y_pos + 1][v->x_pos].rock_hardness < 171)? 2 : 3)));
			map[v->y_pos + 1][v->x_pos].from_y = v->y_pos;
			map[v->y_pos + 1][v->x_pos].from_x = v->x_pos;
			heap_decrease_key_no_replace(&heap, map[v->y_pos + 1][v->x_pos].heap_node);
		}
		
		//explore bottom-right cell
		if((map[v->y_pos + 1][v->x_pos + 1].heap_node) && (map[v->y_pos + 1][v->x_pos + 1].cost >
			(v->cost + ((dungeon[v->y_pos + 1][v->x_pos + 1].rock_hardness < 85 || !tunneling)? 1 : 
			((dungeon[v->y_pos + 1][v->x_pos + 1].rock_hardness < 171)? 2 : 3)))))
		{
			map[v->y_pos + 1][v->x_pos + 1].cost = (v->cost + ((dungeon[v->y_pos + 1][v->x_pos + 1].rock_hardness < 85 || !tunneling)? 1 : 
											((dungeon[v->y_pos + 1][v->x_pos + 1].rock_hardness < 171)? 2 : 3)));
			map[v->y_pos + 1][v->x_pos + 1].from_y = v->y_pos;
			map[v->y_pos + 1][v->x_pos + 1].from_x = v->x_pos;
			heap_decrease_key_no_replace(&heap, map[v->y_pos + 1][v->x_pos + 1].heap_node);
		}
		
		//explore right cell
		if((map[v->y_pos][v->x_pos + 1].heap_node) && (map[v->y_pos][v->x_pos + 1].cost >
			(v->cost + ((dungeon[v->y_pos ][v->x_pos + 1].rock_hardness < 85 || !tunneling)? 1 : 
			((dungeon[v->y_pos][v->x_pos + 1].rock_hardness < 171)? 2 : 3)))))
		{
			map[v->y_pos][v->x_pos + 1].cost = (v->cost + ((dungeon[v->y_pos][v->x_pos + 1].rock_hardness < 85 || !tunneling)? 1 : 
											((dungeon[v->y_pos][v->x_pos + 1].rock_hardness < 171)? 2 : 3)));
			map[v->y_pos][v->x_pos + 1].from_y = v->y_pos;
			map[v->y_pos][v->x_pos + 1].from_x = v->x_pos;
			heap_decrease_key_no_replace(&heap, map[v->y_pos][v->x_pos + 1].heap_node);
		}
		
		//explore top-right cell
		if((map[v->y_pos - 1][v->x_pos + 1].heap_node) && (map[v->y_pos - 1][v->x_pos + 1].cost >
			(v->cost + ((dungeon[v->y_pos - 1][v->x_pos + 1].rock_hardness < 85 || !tunneling)? 1 : 
			((dungeon[v->y_pos - 1][v->x_pos + 1].rock_hardness < 171)? 2 : 3)))))
		{
			map[v->y_pos - 1][v->x_pos + 1].cost = (v->cost + ((dungeon[v->y_pos - 1][v->x_pos + 1].rock_hardness < 85 || !tunneling)? 1 : 
											((dungeon[v->y_pos - 1][v->x_pos + 1].rock_hardness < 171)? 2 : 3)));
			map[v->y_pos - 1][v->x_pos + 1].from_y = v->y_pos;
			map[v->y_pos - 1][v->x_pos + 1].from_x = v->x_pos;
			heap_decrease_key_no_replace(&heap, map[v->y_pos - 1][v->x_pos + 1].heap_node);
		}
	
	}
	
	heap_delete(&heap);
	return v;
	
}

// returns ROOM_BUILD_SUCCESS if the parameters given indicate an acceptable, placeable room
// if the room is acceptable and placeable then it is placed in the dungeon and recorded on the rooms list
int try_build_room(int x_pos, int y_pos, int x_size, int y_size)
{
	// size and position checking
	if(!load_dungeon) // allow loaded dungeons to have arbitary size
	if(x_size > ROOM_X_MAX_SIZE || y_size > ROOM_Y_MAX_SIZE)
		return ROOM_TOO_LARGE;
	if((x_pos < 1 || (x_pos+x_size-1) > NUM_COLS-2) ||	(y_pos < 1 || (y_pos+y_size-1) > NUM_ROWS-2))
	return ROOM_OUT_OF_BOUNDS;

	int i, j;
	
	//room overlap checking
	if(!load_dungeon) // don't check for overlap when loading dungeons
	for(i = y_pos; i < (y_pos + y_size); i++)
		for(j = x_pos; j < (x_pos + x_size); j++)
		{
		if(dungeon[i][j].type == ROOM || dungeon[i][j].type == IMMUTABLE || dungeon[i][j].is_room_edge)
			return ROOM_OVERLAP;
		}

	// if I've	made it here then it's an acceptable, placeable room so place it in the dungeon
	for(i = y_pos; i < (y_pos + y_size); i++)
	for(j = x_pos; j < (x_pos + x_size); j++)
	{
		dungeon[i][j].type = ROOM;
		dungeon[i][j].rock_hardness = 0;
	}

	// record this room in the rooms list
	room_t * current_room = head_room;

	if(current_room != NULL)
	{
	
	while(current_room->next_room != NULL)
		current_room = current_room->next_room;
	
	current_room->next_room = malloc(sizeof(room_t));
	current_room->next_room->x_pos = x_pos;
	current_room->next_room->y_pos = y_pos;
	current_room->next_room->x_size = x_size;
	current_room->next_room->y_size = y_size;
	current_room->next_room->next_room = NULL;
	}
	else // current room is NULL (a.k.a. it's the first room)
	{
		head_room = malloc(sizeof(room_t));
	
		head_room->x_pos = x_pos;
	head_room->y_pos = y_pos;
	head_room->x_size = x_size;
	head_room->y_size = y_size;
	head_room->next_room = NULL;
	}

	//set room edge flags

	//left edge
	for(i = y_pos; i < (y_pos+y_size); i++)
	dungeon[i][x_pos-1].is_room_edge = 1;

	//right edge
	for(i = y_pos; i < (y_pos+y_size); i++)
	dungeon[i][x_pos+x_size].is_room_edge = 1;

	//top edge
	for(i = x_pos; i < (x_pos+x_size+1); i++)
	dungeon[y_pos-1][i].is_room_edge = 1;

	//bottom edge
	for(i = x_pos; i < (x_pos+x_size+1); i++)
	dungeon[y_pos+y_size][i].is_room_edge = 1;

	num_rooms++;
	
	return ROOM_BUILD_SUCCESS;
}

// connects two rooms with corridors
int connect_rooms(room_t * room1, room_t * room2)
{
	if(room1 == NULL || room2 == NULL)
	return ROOM_NULL_ERROR;

	point_t room1_center = {room1->x_pos + room1->x_size/2, room1->y_pos + room1->y_size/2};
	point_t room2_center = {room2->x_pos + room2->x_size/2, room2->y_pos + room2->y_size/2};

	// find the direction to build corridors from room1 to room2
	point_t corridor_pos = room1_center;
	
	//connect corridors on top or bottom of room1
	if(room1_center.y < room2_center.y) // then room1 reaches below room2
	{
	while(corridor_pos.y < room2_center.y)
	{
		if(dungeon[corridor_pos.y][corridor_pos.x].type != ROOM && dungeon[corridor_pos.y][corridor_pos.x].type != IMMUTABLE)
		{
		dungeon[corridor_pos.y][corridor_pos.x].rock_hardness = 0.0;
		dungeon[corridor_pos.y][corridor_pos.x].type = CORRIDOR;
		}
		corridor_pos.y += 1;
	}
	}
	if(room1_center.y > room2_center.y) // then room1 reaches above room2
	{
	while(corridor_pos.y > room2_center.y)
	{
		if(dungeon[corridor_pos.y][corridor_pos.x].type != ROOM && dungeon[corridor_pos.y][corridor_pos.x].type != IMMUTABLE)
		{
		dungeon[corridor_pos.y][corridor_pos.x].rock_hardness = 0.0;
		dungeon[corridor_pos.y][corridor_pos.x].type = CORRIDOR;
		}
		corridor_pos.y -= 1;
	}
	}

	//connect corridors on the right or left of room1 or current corridor building position
	if(room1_center.x < room2_center.x) // then room1 reaches more left then room2
	{
	while(corridor_pos.x < room2_center.x)
	{
		if(dungeon[corridor_pos.y][corridor_pos.x].type != ROOM && dungeon[corridor_pos.y][corridor_pos.x].type != IMMUTABLE)
		{
		dungeon[corridor_pos.y][corridor_pos.x].rock_hardness = 0.0;
		dungeon[corridor_pos.y][corridor_pos.x].type = CORRIDOR;
		}
		corridor_pos.x += 1;
	}
	}
	if(room1_center.x > room2_center.x) // then room1 reaches more right then room2
	{
	while(corridor_pos.x > room2_center.x)
	{
		if(dungeon[corridor_pos.y][corridor_pos.x].type != ROOM && dungeon[corridor_pos.y][corridor_pos.x].type != IMMUTABLE)
		{
		dungeon[corridor_pos.y][corridor_pos.x].rock_hardness = 0.0;
		dungeon[corridor_pos.y][corridor_pos.x].type = CORRIDOR;
		}
		corridor_pos.x -= 1;
	}
	}
	
	return CONNECT_ROOMS_SUCCESS;
}



// prints the 2D dungeon to standard out
int print_dungeon(dungeon_node_t d[NUM_ROWS][NUM_COLS])
{
	int i;

	// aesthetic wall edge
	char * top_bottom_edge = malloc(NUM_COLS + 1);
	memset(top_bottom_edge, '-', NUM_COLS);
	top_bottom_edge[NUM_COLS] = 0;
	
	printf("%s\n", top_bottom_edge);
	
	for(i = 1; i < NUM_ROWS - 1; i++)
	{
	char printable_row[NUM_COLS + 1] = { };

	// aesthetic wall edges
	printable_row[0] = '|';
	printable_row[NUM_COLS-1] = '|';
	
	int j;
	for(j = 1; j < NUM_COLS - 1; j++)
	{
		if(d[i][j].override_symbol)
		{
			printable_row[j] = d[i][j].overriden_char;
			continue;
		}
		
		if(char_map[i][j] != NULL)
		{
			if(char_map[i][j]->abilities == ABIL_ISPC)
				printable_row[j] = '@';
			else
			{
				if(char_map[i][j]->abilities <= 9)
					printable_row[j] = char_map[i][j]->abilities + 48;
				else
					printable_row[j] = char_map[i][j]->abilities + 87;
			}
		}			
		else
			switch(d[i][j].type)
			{
				case ROCK:
					printable_row[j] = ' ';
					break;
				case ROOM:
					printable_row[j] = '.';
					break;
				case CORRIDOR:
					printable_row[j] = '#';
					break;
				default:
					printable_row[j] = '?';		
			}
		
		// if(d[i][j].has_player)
			// printable_row[j] = '@';
	}
	
		printf("%s\n", printable_row);
	}
	
	// aesthetic wall edge
	printf("%s\n", top_bottom_edge);

	free(top_bottom_edge);

	return 0;
}

int copy_dungeon(dungeon_node_t dest[NUM_ROWS][NUM_COLS], dungeon_node_t src[NUM_ROWS][NUM_COLS])
{
	return (memcpy(dest, src, NUM_ROWS * NUM_COLS * sizeof(dungeon_node_t)) == NULL)? -1 : 0;
	
	// int i, j;
	
	// for(i = 0; i < NUM_ROWS; i++)
		// for(j = 0; j < NUM_COLS; j++)
		
}

// returns a random integer within the range given by ints start and end and including both start and end [start, end]
int rand_range(int start, int end)
{
	return start + (rand() % (end-start+1));
}

// reverses the byte ordering of 'array' (used to read/write big-endian)
void reverse_byte_order(uint8_t * array, int length)
{
	int i;

	for(i = 0; i < length/2; i++)
	{
	uint8_t temp = *(array + i);
	*(array + i) = *(array + length - i - 1);
	*(array + length - i - 1) = temp;
	}
}

void cleanup_dungeon()
{
	int i, j;
	
	//free the rooms list
	room_t * current_room = head_room;
	room_t * helper = current_room;
	while(current_room != NULL)
	{
		helper = current_room->next_room;
		free(current_room);
		current_room = helper;
	}
	
	//free the character map
	for(i = 0; i < NUM_ROWS; i++)
		for(j = 0; j < NUM_COLS; j++)
			if(char_map[i][j] != NULL)
				free(char_map[i][j]);
			
	heap_delete(turn_queue);
}


int main(int argc, char * argv[])
{
	int i = 0, j;
	char * dungeon_to_load_path = NULL;
	
	srand(time(NULL));
	nummon = DEFAULT_NUMMON;
	
	while(i < argc)
	{
		if(!strcmp("--save", argv[i]))
			save_dungeon = 1;
		
		if(!strcmp("--load", argv[i]))
		{
			load_dungeon = 1;
			dungeon_to_load_path = malloc(200);
			strcpy(dungeon_to_load_path, getenv("HOME"));
			strcat(dungeon_to_load_path, "/.rlg327/dungeon");
		}
		
		if(!strcmp("--nummon", argv[i]))
		{
			if((i + 1) < argc)
				nummon = atoi(argv[i+1]);
			else
			{
				fprintf(stderr, "Usage --nummon [int]\n");
				exit(-1);
			}
		}
		
		i++;
	} 

	if(dungeon_to_load_path != NULL)
	{
		int dungeon_build_error = init_dungeon_load(dungeon_to_load_path);

		free(dungeon_to_load_path);
			
		if(dungeon_build_error == DUNGEON_UNABLE_TO_LOAD_FILE)
		{
			printf("Unable to load dungeon file. Exiting...\n");
			exit(1);
		}
		else if(dungeon_build_error == DUNGEON_WRONG_FILE_TYPE)
		{
			printf("Dungeon file has the wrong file-type marker. Exiting...\n");
			exit(2);
		}
		else if(dungeon_build_error == DUNGEON_NO_ROOMS_BUILT)
		{
			printf("Problem building rooms. The issue could be that the dungeon file doesn't contain rooms. Exiting...\n");
			exit(3);
		}
		
	}
	else
		init_random_dungeon();

	if(save_dungeon)
	{
		char dungeon_save_path[200] = {};
		strcpy(dungeon_save_path, getenv("HOME"));
		strcat(dungeon_save_path, "/.rlg327/dungeon");
		int save_error = save_dungeon_to_disk(dungeon_save_path);

		if(save_error == DUNGEON_NO_ROOMS_SAVED)
		{
			printf("Save error: No rooms saved in dungeon.\n");
		}
		else if(save_error == DUNGEON_SAVE_FILEPATH_NULL)
		{
			printf("Save error: Unable to save file at the given save path.\n");
		}
		else if(save_error == DUNGEON_UNABLE_TO_SAVE_FILE)
		{
			printf("Save error: Dungeon was unable to be saved.\n");
		}
		
	}

	point_t player_position = place_character(NULL);	

	build_distances_map(distance_weights_non_tunneling, player_position, 0);
	build_distances_map(distance_weights_tunneling, player_position, 1);
	
	init_character_map(char_map, player_position, nummon);
	
	print_dungeon(dungeon);
	//print_dungeon(distance_weights_non_tunneling);
	//print_dungeon(distance_weights_tunneling);
	
	sleep(1);

	int game_status = GAME_RUNNING;
	int game_result;

	while(game_status != GAME_OVER)
	{
		game_result = run_turn_queue(&turn_queue);

		if(nummon <= 0)
			game_status = GAME_OVER;

		if(game_result == PC_DEAD)
			game_status = GAME_OVER;
	}

	if(game_result == PC_DEAD)
		printf("Game lost!\n");
	else
		printf( "***********************\n"
				" Congrats, the PC won!\n"
				"***********************\n");
	
	cleanup_dungeon();
	
	// use this code later for traversing a shortest path, like for a monster
	// for(y = i, x = j; (y != player_position.y) || (x != player_position.x); y = shortest_path->y_pos, x = shortest_path->x_pos)
	// {
		// d[y][x].has_player = 1;
		// shortest_path = &map[shortest_path->from_y][shortest_path->from_x];
	// }
	
	return 0;
}
