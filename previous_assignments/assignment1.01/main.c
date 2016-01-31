/*
  Joe Avey CS 327

  Requirements of assignment 1.01

  Create and print an 84x21 (col,row) dungeon

  - at least 5 rooms per dungeon
  - rooms measure at least 3 units in the x (horizontal) direction and at least 2 units in the y (vertical) direction
  - there must be at least 1 cell of non-room bewteen each room
  - the outermost cells of the dungeon are immutable (rock)
  - Rooms should be drawn with periods (.)
  - Corridor should be drawn with hashes (#)
  - Rock should be drawn with spaces ( )
  - The dungeon should be fully connected
  - Corridors should not extend into rooms (no hashes should be rendered inside rooms)
 */

// include libraries ///////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

// define statements ///////////////////////////////////
#define ROOM_X_MIN_SIZE 3
#define ROOM_Y_MIN_SIZE 2
#define ROOM_X_MAX_SIZE 15
#define ROOM_Y_MAX_SIZE 15

#define MIN_NUM_ROOMS 5
#define MAX_NUM_ROOMS 8

#define NUM_ROWS 21
#define NUM_COLS 84

// type declarations ///////////////////////////////////
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

    

typedef struct room{
    int x_pos;
    int y_pos;
    int x_size;
    int y_size;
    struct room * next_room;
}room_t;    

typedef struct 
{
    uint8_t type;
    uint8_t rock_hardness; // 0 for non-rocks, 1-99 for rocks, 100 for immutable
    uint8_t is_room_edge; // true if this dungeon node touches a room edge
} dungeon_node_t;
    
typedef struct
{
    int x;
    int y;
}point_t;


// function prototypes /////////////////////////////////
int init_dungeon();
int print_dungeon();
int rand_range(int start, int end);
int try_build_room(int x_pos, int y_pos, int x_size, int y_size);
int connect_rooms(room_t * room1, room_t * room2);

// global vars ////////////////////////////////////////
dungeon_node_t  dungeon[NUM_ROWS][NUM_COLS] = {};
room_t * head_room;
int num_rooms = 0;

// function implementations ///////////////////////////

// initializes the global dungeon variable with random rooms and rock hardnesses
int init_dungeon()
{
  // fill the dungeon with rock
  int i, j;
  for(i = 1; i < NUM_ROWS - 1; i++)
    for(j = 1; j < NUM_COLS - 1; j++)
    {	
      dungeon[i][j].type = ROCK;
      dungeon[i][j].rock_hardness = rand_range(1,99);
    } 
      
  //fill the edges with immutable rock borders
  for(i = 0; i < NUM_COLS; i++)
  {
      dungeon[0][i].type = IMMUTABLE;
      dungeon[0][i].rock_hardness = 100;
      dungeon[20][i].type = IMMUTABLE;
      dungeon[20][i].rock_hardness = 100;
  }

  for(i = 1; i < NUM_ROWS - 1; i++)
  {
      dungeon[i][0].type = IMMUTABLE;
      dungeon[i][0].rock_hardness = 100;
      dungeon[i][83].type = IMMUTABLE;
      dungeon[i][83].rock_hardness = 100;
  }

  // build random placed and sized rooms
  srand(time(NULL));
  int target_number_of_rooms = rand_range(MIN_NUM_ROOMS, MAX_NUM_ROOMS);
  int tries = 0;
  while(num_rooms < target_number_of_rooms && tries < 2000)
  {      
      // try to build a random positioned and sized room
      if(try_build_room(rand_range(1, NUM_COLS-ROOM_X_MAX_SIZE-2), rand_range(1, NUM_ROWS-ROOM_Y_MAX_SIZE-2),
			rand_range(ROOM_X_MIN_SIZE, ROOM_X_MAX_SIZE), rand_range(ROOM_Y_MIN_SIZE, ROOM_Y_MAX_SIZE))
	 == ROOM_BUILD_SUCCESS)
	  num_rooms++;

      tries++;
  }

  // connect the first room to each of the rooms  
  room_t * room_to_connect = head_room;
  
  for(i = 1; i < num_rooms; i++)
  {
      room_to_connect = room_to_connect->next_room;	  

      connect_rooms(head_room, room_to_connect);
  }
  


  return 0;
  
}

// returns ROOM_BUILD_SUCCESS if the parameters given indicate an acceptable, placeable room
// if the room is acceptable and placeable then it is placed in the dungeon and recorded on the rooms list
int try_build_room(int x_pos, int y_pos, int x_size, int y_size)
{
    // size and position checking
    if(x_size > ROOM_X_MAX_SIZE || y_size > ROOM_Y_MAX_SIZE)
	return ROOM_TOO_LARGE;
    if((x_pos < 1 || (x_pos+x_size-1) > NUM_COLS-2) ||  (y_pos < 1 || (y_pos+y_size-1) > NUM_ROWS-2))
	return ROOM_OUT_OF_BOUNDS;

    int i, j;
    
    //room overlap checking
    for(i = y_pos; i < (y_pos + y_size); i++)
	for(j = x_pos; j < (x_pos + x_size); j++)
	{
	    if(dungeon[i][j].type == ROOM || dungeon[i][j].type == IMMUTABLE || dungeon[i][j].is_room_edge)
		return ROOM_OVERLAP;
	}

    // if I've  made it here then it's an acceptable, placeable room so place it in the dungeon
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
		dungeon[corridor_pos.y][corridor_pos.x].type = CORRIDOR;
	    corridor_pos.y += 1;
	}
    }
    if(room1_center.y > room2_center.y) // then room1 reaches above room2
    {
	while(corridor_pos.y > room2_center.y)
	{
	    if(dungeon[corridor_pos.y][corridor_pos.x].type != ROOM && dungeon[corridor_pos.y][corridor_pos.x].type != IMMUTABLE)
		dungeon[corridor_pos.y][corridor_pos.x].type = CORRIDOR;
	    corridor_pos.y -= 1;
	}
    }

    //connect corridors on the right or left of room1 or current corridor building position
    if(room1_center.x < room2_center.x) // then room1 reaches more left then room2
    {
	while(corridor_pos.x < room2_center.x)
	{
	    if(dungeon[corridor_pos.y][corridor_pos.x].type != ROOM && dungeon[corridor_pos.y][corridor_pos.x].type != IMMUTABLE)
		dungeon[corridor_pos.y][corridor_pos.x].type = CORRIDOR;
	    corridor_pos.x += 1;
	}
    }
    if(room1_center.x > room2_center.x) // then room1 reaches more right then room2
    {
	while(corridor_pos.x > room2_center.x)
	{
	    if(dungeon[corridor_pos.y][corridor_pos.x].type != ROOM && dungeon[corridor_pos.y][corridor_pos.x].type != IMMUTABLE)
		dungeon[corridor_pos.y][corridor_pos.x].type = CORRIDOR;
	    corridor_pos.x -= 1;
	}
    }
	
    return CONNECT_ROOMS_SUCCESS;
}



// prints the 2D dungeon to standard out
int print_dungeon()
{
    int i;

    // aesthetic wall edge
    printf("------------------------------------------------------------------------------------\n");
    
    for(i = 1; i < NUM_ROWS - 1; i++)
    {
	char printable_row[85] = { };

	// aesthetic wall edges
	printable_row[0] = '|';
	printable_row[NUM_COLS-1] = '|';
	
	int j;
	for(j = 1; j < NUM_COLS - 1; j++)
	{
	    switch(dungeon[i][j].type)
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
	}
	
	printf("%s\n", printable_row);
    }

    // aesthetic wall edge
    printf("------------------------------------------------------------------------------------\n");
}

// returns a random integer within the range given by ints start and end and including both start and end [start, end]
int rand_range(int start, int end)
{
    return start + (rand() % (end-start+1));
}


int main(int argc, char * argv[])
{
    init_dungeon();

    print_dungeon();
    
    return 0;
}
