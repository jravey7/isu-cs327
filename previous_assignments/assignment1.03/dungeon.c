/*
  Joe Avey CS 327

  REQUIREMENTS OF ASSIGNMENT 1.03

  - use Dijkstra's Algorithm for path finding
  - calculate two maps that layout the distances from the PC
	  *mark distances from smallest to largest (0-9, a-z, A-z)
	  *any distances greater than Z(61), render the dungeon normally
  - calculate a distance map for non-tunneling monsters
	  *May only travel on floors; ignore walls
  - calculate a distance map for tunneling monsters
	  *Use cell weights to determine path lengths
	  *cells with hardness 0 have a weight of 1
	  *cells with hardness [1,84] have a weight of 1
	  *cells with hardness [85,170] have a weight of 2
	  *cells with hardness [171,254] have a weight of 3
	  *cells with hardness of 255 are impassable (ignore them)
- render all three view of the dungeon (normal, tunneling, non-tunneling)
*/

// INCLUDE LIBRARIES ///////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <limits.h>
#include "heap.h"

// DEFINE STATEMENTS ///////////////////////////////////
#define ROOM_X_MIN_SIZE 3
#define ROOM_Y_MIN_SIZE 2
#define ROOM_X_MAX_SIZE 10
#define ROOM_Y_MAX_SIZE 10

#define MIN_NUM_ROOMS 5
#define MAX_NUM_ROOMS 8

#define NUM_ROWS 21
#define NUM_COLS 80

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


typedef struct room{
	int x_pos;
	int y_pos;
	int x_size;
	int y_size;
	struct room * next_room;
}room_t;	

typedef struct {
	uint8_t type;
	uint8_t rock_hardness; // 0 for non-rocks, 1-99 for rocks, 100 for immutable
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


// FUNCTION PROTOTYPES /////////////////////////////////
int init_random_dungeon();
int init_dungeon_load(char * filepath);
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
int copy_dungeon(dungeon_node_t src[NUM_ROWS][NUM_COLS], dungeon_node_t dest[NUM_ROWS][NUM_COLS]);

// GLOBAL VARS ////////////////////////////////////////
dungeon_node_t	dungeon[NUM_ROWS][NUM_COLS] = {};
room_t * head_room;
int num_rooms = 0;
uint8_t load_dungeon = 0, save_dungeon = 0;

// FUNCTION IMPLEMENTATIONS ///////////////////////////

static int32_t vertex_cost_cmp(const void * key, const void *with)
{
	return ((vertex_path_t *) key)->cost - ((vertex_path_t *) with)->cost;
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

void build_distances_map(dungeon_node_t d[NUM_ROWS][NUM_COLS], point_t player_position, int tunneling)
{
		copy_dungeon(d, dungeon);
	
	int i, j;
	
	for(i = 1; i < NUM_ROWS - 1; i++)
	for(j = 1; j < NUM_COLS - 1; j++)
	{
		vertex_path_t map[NUM_ROWS][NUM_COLS], *shortest_path;
		
		shortest_path = find_shortest_path(map, player_position.x, player_position.y, j, i, tunneling);
		
		if((shortest_path == NULL) || (shortest_path->cost > 61)) // a path could not be found between (to) and (from) or the path isn't in the range
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
	
	if(position == NULL || x_pos > 78 || x_pos < 1 || y_pos > 19 || y_pos < 1)
	{
		x_pos = rand_range(1, 78);
		y_pos = rand_range(1, 19);
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
		x_pos = rand_range(1, 78);
		y_pos = rand_range(1, 19);
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
				if((dungeon[i][j].type == ROOM) || (dungeon[i][j].type == CORRIDOR) && (dungeon[i][j].rock_hardness < 255))
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
	char printable_row[85] = { };

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
		
		if(d[i][j].has_player)
			printable_row[j] = '@';
		}
	
		printf("%s\n", printable_row);
	}
	
	// aesthetic wall edge
	printf("%s\n", top_bottom_edge);

	free(top_bottom_edge);
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


int main(int argc, char * argv[])
{
	int i = argc - 1, j;
	char * dungeon_to_load_path = NULL;

	srand(time(NULL));
	
	while(i > 0)
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
		
		i--;
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
	
	dungeon_node_t distance_weights_non_tunneling[NUM_ROWS][NUM_COLS] = {};
	dungeon_node_t distance_weights_tunneling[NUM_ROWS][NUM_COLS] = {};
	
	build_distances_map(distance_weights_non_tunneling, player_position, 0);
	build_distances_map(distance_weights_tunneling, player_position, 1);
	
	print_dungeon(dungeon);
	print_dungeon(distance_weights_non_tunneling);
	print_dungeon(distance_weights_tunneling);
	
	//free the rooms list
	room_t * current_room = head_room;
	room_t * helper = current_room;
	while(current_room != NULL)
	{
		helper = current_room->next_room;
		free(current_room);
		current_room = helper;
	}
	
	// use this code later for traversing a path, like a monster traversing a path
	// for(y = i, x = j; (y != player_position.y) || (x != player_position.x); y = shortest_path->y_pos, x = shortest_path->x_pos)
	// {
		// d[y][x].has_player = 1;
		// shortest_path = &map[shortest_path->from_y][shortest_path->from_x];
	// }
	
	return 0;
}
