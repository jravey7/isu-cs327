#include <unistd.h>
#include <ncurses.h>
#include <ctype.h>
#include <stdlib.h>
#include <cstring>

#include "io.h"
#include "move.h"
#include "path.h"
#include "pc.h"
#include "utils.h"
#include "dungeon.h"
#include "object.h"

/* Same ugly hack we did in path.c */
static dungeon_t *dungeon;

typedef struct io_message {
  /* Will print " --more-- " at end of line when another message follows. *
   * Leave 10 extra spaces for that.                                      */
  char msg[71];
  struct io_message *next;
} io_message_t;

static io_message_t *io_head, *io_tail;

void io_init_terminal(void)
{
  initscr();
  raw();
  noecho();
  curs_set(0);
  //keypad(stdscr, TRUE);
  start_color();
  init_pair(COLOR_RED, COLOR_RED, COLOR_BLACK);
  init_pair(COLOR_GREEN, COLOR_GREEN, COLOR_BLACK);
  init_pair(COLOR_YELLOW, COLOR_YELLOW, COLOR_BLACK);
  init_pair(COLOR_BLUE, COLOR_BLUE, COLOR_BLACK);
  init_pair(COLOR_MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
  init_pair(COLOR_CYAN, COLOR_CYAN, COLOR_BLACK);
  init_pair(COLOR_WHITE, COLOR_WHITE, COLOR_BLACK);
}

void io_reset_terminal(void)
{
	//clean up input buffer
	nodelay(stdscr, TRUE);
	while(getch() != -1);
	nodelay(stdscr, FALSE);


	endwin();

	while (io_head) {
	io_tail = io_head;
	io_head = io_head->next;
	free(io_tail);
	}
	io_tail = NULL;
}

void io_queue_message(const char *format, ...)
{
  io_message_t *tmp;
  va_list ap;

  if (!(tmp = (io_message_t *) malloc(sizeof (*tmp)))) {
    perror("malloc");
    exit(1);
  }

  tmp->next = NULL;

  va_start(ap, format);

  vsnprintf(tmp->msg, sizeof (tmp->msg), format, ap);

  va_end(ap);

  if (!io_head) {
    io_head = io_tail = tmp;
  } else {
    io_tail->next = tmp;
    io_tail = tmp;
  }
}

static void io_print_message_queue(uint32_t y, uint32_t x)
{
  while (io_head) {
    io_tail = io_head;
    attron(COLOR_PAIR(COLOR_CYAN));
    mvprintw(y, x, "%-80s", io_head->msg);
    attroff(COLOR_PAIR(COLOR_CYAN));
    io_head = io_head->next;
    if (io_head) {
      attron(COLOR_PAIR(COLOR_CYAN));
      mvprintw(y, x + 70, "%10s", " --more-- ");
      attroff(COLOR_PAIR(COLOR_CYAN));
      refresh();
      getch();
    }
    free(io_tail);
  }
  io_tail = NULL;
}

static char distance_to_char[] = {
  '0',
  '1',
  '2',
  '3',
  '4',
  '5',
  '6',
  '7',
  '8',
  '9',
  'a',
  'b',
  'c',
  'd',
  'e',
  'f',
  'g',
  'h',
  'i',
  'j',
  'k',
  'l',
  'm',
  'n',
  'o',
  'p',
  'q',
  'r',
  's',
  't',
  'u',
  'v',
  'w',
  'x',
  'y',
  'z',
  'A',
  'B',
  'C',
  'D',
  'E',
  'F',
  'G',
  'H',
  'I',
  'J',
  'K',
  'L',
  'M',
  'N',
  'O',
  'P',
  'Q',
  'R',
  'S',
  'T',
  'U',
  'V',
  'W',
  'X',
  'Y',
  'Z'
};

void io_display_tunnel(dungeon_t *d)
{
  uint32_t y, x;
  clear();
  for (y = 0; y < DUNGEON_Y; y++) {
    for (x = 0; x < DUNGEON_X; x++) {
      mvaddch(y + 1, x, (d->pc_tunnel[y][x] < 62              ?
                         distance_to_char[d->pc_tunnel[y][x]] :
                         '*'));
    }
  }
  refresh();
}

void io_display_distance(dungeon_t *d)
{
  uint32_t y, x;
  clear();
  for (y = 0; y < DUNGEON_Y; y++) {
    for (x = 0; x < DUNGEON_X; x++) {
      mvaddch(y + 1, x, (d->pc_distance[y][x] < 62              ?
                         distance_to_char[d->pc_distance[y][x]] :
                         '*'));
    }
  }
  refresh();
}

void io_display_hardness(dungeon_t *d)
{
  uint32_t y, x;
  clear();
  for (y = 0; y < DUNGEON_Y; y++) {
    for (x = 0; x < DUNGEON_X; x++) {
      /* Maximum hardness is 255.  We have 62 values to display it, but *
       * we only want one zero value, so we need to cover [1,255] with  *
       * 61 values, which gives us a divisor of 254 / 61 = 4.164.       *
       * Generally, we want to avoid floating point math, but this is   *
       * not gameplay, so we'll make an exception here to get maximal   *
       * hardness display resolution.                                   */
      mvaddch(y + 1, x, (d->hardness[y][x]                             ?
                         distance_to_char[1 + (d->hardness[y][x] / 5)] :
                         '0'));
    }
  }
  refresh();
}

void io_display_all(dungeon_t *d)
{
  uint32_t y, x;

  clear();
  for (y = 0; y < DUNGEON_Y; y++) {
    for (x = 0; x < DUNGEON_X; x++) {
      if (d->charmap[y][x]) {
    	if(is_valid_color(d->charmap[y][x]->color))
    		attron(COLOR_PAIR(d->charmap[y][x]->color));

    	mvaddch(y + 1, x, character_get_symbol(d->charmap[y][x]));

    	if(is_valid_color(d->charmap[y][x]->color))
    		attroff(COLOR_PAIR(d->charmap[y][x]->color));
      }
      else if(position_has_object(d, y, x)){
    	object * o = get_object_at_position(d, y, x);

    	if(is_valid_color(o->color))
      		attron(COLOR_PAIR(o->color));

    	  mvaddch(y + 1, x, o->symbol);

      	if(is_valid_color(o->color))
      		attroff(COLOR_PAIR(o->color));
      }
      else {
        switch (mapxy(x, y)) {
        case ter_wall:
        case ter_wall_immutable:
          mvaddch(y + 1, x, ' ');
          break;
        case ter_floor:
        case ter_floor_room:
          mvaddch(y + 1, x, '.');
          break;
        case ter_floor_hall:
          mvaddch(y + 1, x, '#');
          break;
        case ter_debug:
          mvaddch(y + 1, x, '*');
          break;
        case ter_stairs_up:
          mvaddch(y + 1, x, '<');
          break;
        case ter_stairs_down:
          mvaddch(y + 1, x, '>');
          break;
        default:
 /* Use zero as an error symbol, since it stands out somewhat, and it's *
  * not otherwise used.                                                 */
          mvaddch(y + 1, x, '0');
        }
      }
    }
  }

  io_print_message_queue(0, 0);

  refresh();
}

void io_display(dungeon_t *d)
{
  uint32_t y, x;
  uint32_t illuminated;
  clear();
  for (y = 0; y < DUNGEON_Y; y++) {
    for (x = 0; x < DUNGEON_X; x++) {
      if ((illuminated = is_illuminated(d->pc, y, x))) {
        attron(A_BOLD);
      }
      int seeable_object = 0;
      object * o = NULL;
      if(position_has_object(d, y, x))
      {
    	  o = get_object_at_position(d, y, x);
    	  pair_t obj_pos;
    	  obj_pos[dim_y] = o->position[dim_y];
    	  obj_pos[dim_x] = o->position[dim_x];
    	  if(can_see(d,
                  character_get_pos(d->pc),
                  obj_pos,
                  1)){
    		  seeable_object = 1;
    	  }
      }

      if (d->charmap[y][x] &&
          can_see(d,
                  character_get_pos(d->pc),
                  character_get_pos(d->charmap[y][x]),
                  1)) {
      	if(is_valid_color(d->charmap[y][x]->color))
      		attron(COLOR_PAIR(d->charmap[y][x]->color));

      	mvaddch(y + 1, x, character_get_symbol(d->charmap[y][x]));

      	if(is_valid_color(d->charmap[y][x]->color))
      		attroff(COLOR_PAIR(d->charmap[y][x]->color));;
      }
      else if(seeable_object){

      	if(is_valid_color(o->color))
        		attron(COLOR_PAIR(o->color));

      	  mvaddch(y + 1, x, o->symbol);

        	if(is_valid_color(o->color))
        		attroff(COLOR_PAIR(o->color));
      }
      else {
        switch (pc_learned_terrain(d->pc, y, x)) {
        case ter_wall:
        case ter_wall_immutable:
        case ter_unknown:
          mvaddch(y + 1, x, ' ');
          break;
        case ter_floor:
        case ter_floor_room:
          mvaddch(y + 1, x, '.');
          break;
        case ter_floor_hall:
          mvaddch(y + 1, x, '#');
          break;
        case ter_debug:
          mvaddch(y + 1, x, '*');
          break;
        case ter_stairs_up:
          mvaddch(y + 1, x, '<');
          break;
        case ter_stairs_down:
          mvaddch(y + 1, x, '>');
          break;
        default:
 /* Use zero as an error symbol, since it stands out somewhat, and it's *
  * not otherwise used.                                                 */
          mvaddch(y + 1, x, '0');
        }
      }
      if (illuminated) {
        attroff(A_BOLD);
      }
    }
  }

  io_print_message_queue(0, 0);

  refresh();
}

void io_display_monster_list(dungeon_t *d)
{
  mvprintw(11, 33, " HP:    XXXXX ");
  mvprintw(12, 33, " Speed: XXXXX ");
  mvprintw(14, 27, " Hit any key to continue. ");
  refresh();
  getch();
}

uint32_t io_teleport_pc(dungeon_t *d)
{
  /* Just for fun. */
  pair_t dest;

  do {
    dest[dim_x] = rand_range(1, DUNGEON_X - 2);
    dest[dim_y] = rand_range(1, DUNGEON_Y - 2);
  } while (charpair(dest));

  d->charmap[character_get_y(d->pc)][character_get_x(d->pc)] = NULL;
  d->charmap[dest[dim_y]][dest[dim_x]] = d->pc;

  character_set_y(d->pc, dest[dim_y]);
  character_set_x(d->pc, dest[dim_x]);

  if (mappair(dest) < ter_floor) {
    mappair(dest) = ter_floor;
  }

  pc_observe_terrain(d->pc, d);

  dijkstra(d);
  dijkstra_tunnel(d);

  return 0;
}
/* Adjectives to describe our monsters */
static const char *adjectives[] = {
  "A menacing ",
  "A threatening ",
  "A horrifying ",
  "An intimidating ",
  "An aggressive ",
  "A frightening ",
  "A terrifying ",
  "A terrorizing ",
  "An alarming ",
  "A frightening ",
  "A dangerous ",
  "A glowering ",
  "A glaring ",
  "A scowling ",
  "A chilling ",
  "A scary ",
  "A creepy ",
  "An eerie ",
  "A spooky ",
  "A slobbering ",
  "A drooling ",
  " A horrendous ",
  "An unnerving ",
  "A cute little ",  /* Even though they're trying to kill you, */
  "A teeny-weenie ", /* they can still be cute!                 */
  "A fuzzy ",
  "A fluffy white ",
  "A kawaii ",       /* For our otaku */
  "Hao ke ai de "    /* And for our Chinese */
  /* And there's one special case (see below) */
};

static void io_scroll_monster_list(char (*s)[40], uint32_t count)
{
  uint32_t offset;
  uint32_t i;

  offset = 0;

  while (1) {
    for (i = 0; i < 13; i++) {
      mvprintw(i + 6, 19, " %-40s ", s[i + offset]);
    }
    switch (getch()) {
    case KEY_UP:
      if (offset) {
        offset--;
      }
      break;
    case KEY_DOWN:
      if (offset < (count - 13)) {
        offset++;
      }
      break;
    case 27:
      return;
    }

  }
}

static void io_list_monsters_display(dungeon_t *d,
                                     character **c,
                                     uint32_t count)
{
  uint32_t i;
  char (*s)[40]; /* pointer to array of 40 char */

  s = (char (*)[40]) malloc(count * sizeof (*s));

  mvprintw(3, 19, " %-40s ", "");
  /* Borrow the first element of our array for this string: */
  snprintf(s[0], 40, "You know of %d monsters:", count);
  mvprintw(4, 19, " %-40s ", s);
  mvprintw(5, 19, " %-40s ", "");

  for (i = 0; i < count; i++) {
    snprintf(s[i], 40, "%16s%c: %2d %s by %2d %s",
             (character_get_symbol(c[i]) == 'd' ? "A tenacious " :
              adjectives[rand() % (sizeof (adjectives) /
                                   sizeof (adjectives[0]))]),
             character_get_symbol(c[i]),
             abs(character_get_y(c[i]) - character_get_y(d->pc)),
             ((character_get_y(c[i]) - character_get_y(d->pc)) <= 0 ?
              "North" : "South"),
             abs(character_get_x(c[i]) - character_get_x(d->pc)),
             ((character_get_x(c[i]) - character_get_x(d->pc)) <= 0 ?
              "East" : "West"));
    if (count <= 13) {
      /* Handle the non-scrolling case right here. *
       * Scrolling in another function.            */
      mvprintw(i + 6, 19, " %-40s ", s[i]);
    }
  }

  if (count <= 13) {
    mvprintw(count + 6, 19, " %-40s ", "");
    mvprintw(count + 7, 19, " %-40s ", "Hit escape to continue.");
    while (getch() != 27 /* escape */)
      ;
  } else {
    mvprintw(19, 19, " %-40s ", "");
    mvprintw(20, 19, " %-40s ",
             "Arrows to scroll, escape to continue.");
    io_scroll_monster_list(s, count);
  }

  free(s);
}

static int compare_monster_distance(const void *v1, const void *v2)
{
  const character *const *c1 = (const character * const *) v1;
  const character *const *c2 = (const character * const *) v2;

  return (dungeon->pc_distance[character_get_y(*c1)][character_get_x(*c1)] -
          dungeon->pc_distance[character_get_y(*c2)][character_get_x(*c2)]);
}

static void io_list_pc_equipment(dungeon_t *d)
{

	  mvprintw(2, 20, "%-40s", "PC Equipped:");
	  mvprintw(3, 20, "%-40s", "");

	  char i;
	  for(i = 0; i < 12; i++)
	  {
		  char str[51] = {};
		  str[0] = i + 97;
		  str[1] = ' ';
		  if(i < ((pc*)d->pc)->equipped.size())
		  {
			  strcat(str, ((pc*)d->pc)->equipped[i]->name.c_str());
		  }
		  mvprintw(4 + i, 20, "%-50s", str);
	  }
	  refresh();

	  mvprintw(16, 20, "%-40s", "");
	  mvprintw(17, 20, "%-40s", "Hit ESC to continue");

	  while (getch() != 27 /* escape */);

	  io_display_all(d);
}

static void io_list_pc_inventory(dungeon_t *d)
{

	  mvprintw(2, 20, "%-40s", "PC Inventory:");
	  mvprintw(3, 20, "%-40s", "");

	  char i;
	  for(i = 0; i < 10; i++)
	  {
		  char str[51] = {};
		  str[0] = i + 48;
		  str[1] = ' ';
		  if(i < ((pc*)d->pc)->inventory.size())
		  {
			  strcat(str, ((pc*)d->pc)->inventory[i]->name.c_str());
		  }
		  mvprintw(4 + i, 20, "%-50s", str);
	  }
	  refresh();

	  mvprintw(14, 20, "%-40s", "");
	  mvprintw(15, 20, "%-40s", "Hit ESC to continue");

	  while (getch() != 27 /* escape */);

	  io_display_all(d);
}

static void pc_equip(dungeon_t *d)
{

	  mvprintw(2, 20, "%-40s", "Equip Item:");
	  mvprintw(3, 20, "%-40s", "");

	  char i;
	  for(i = 0; i < 10; i++)
	  {
		  char str[51] = {};
		  str[0] = i + 48;
		  str[1] = ' ';
		  if(i < ((pc*)d->pc)->inventory.size())
		  {
			  strcat(str, ((pc*)d->pc)->inventory[i]->name.c_str());
		  }
		  mvprintw(4 + i, 20, "%-40s", str);
	  }
	  refresh();

	  mvprintw(14, 20, "%-40s", "");
	  mvprintw(15, 20, "%-40s", "Choose an item to equip (0-9)");
	  mvprintw(16, 20, "%-40s", "or hit any other key to exit");

	  int slot = getch();
	  slot -= 48;

	  if(slot >= 0 && slot <= 9)
	  {
		  if(((pc*)d->pc)->equip_item(slot))
		  {
			  mvprintw(1, 20, "%-40s", "**Succesfully equipped item**");
			  refresh();
			  sleep(1);
		  }
	  }

	  io_display_all(d);
}

static void pc_unequip(dungeon_t *d)
{

	  mvprintw(2, 20, "%-40s", "Unequip Item:");
	  mvprintw(3, 20, "%-40s", "");

	  char i;
	  for(i = 0; i < 12; i++)
	  {
		  char str[51] = {};
		  str[0] = i + 97;
		  str[1] = ' ';
		  if(i < ((pc*)d->pc)->equipped.size())
		  {
			  strcat(str, ((pc*)d->pc)->equipped[i]->name.c_str());
		  }
		  mvprintw(4 + i, 20, "%-40s", str);
	  }
	  refresh();

	  mvprintw(16, 20, "%-40s", "");
	  mvprintw(17, 20, "%-40s", "Choose an item to unequip (a-l)");
	  mvprintw(18, 20, "%-40s", "or hit any other key to exit");

	  int slot = getch();
	  slot -= 97;

	  if(slot >= 0 && slot <= 11)
	  {
		  //drop item if unequip() returns it
		  object * ue = ((pc*)d->pc)->unequip_item(slot);
		  if(ue)
		  {
			  ue->position[dim_y] = d->pc->position[dim_y];
			  ue->position[dim_x] = d->pc->position[dim_x];
			  d->objects.push_back(ue);
			  mvprintw(1, 20, "%-40s", "**Dropped item. Lacking inventory space**");
			  refresh();
			  sleep(1);
		  }
	  }

	  io_display_all(d);
}

static void pc_drop(dungeon_t *d)
{

	  mvprintw(2, 20, "%-40s", "Drop Item:");
	  mvprintw(3, 20, "%-40s", "");

	  char i;
	  for(i = 0; i < 10; i++)
	  {
		  char str[51] = {};
		  str[0] = i + 48;
		  str[1] = ' ';
		  if(i < ((pc*)d->pc)->inventory.size())
		  {
			  strcat(str, ((pc*)d->pc)->inventory[i]->name.c_str());
		  }
		  mvprintw(4 + i, 20, "%-40s", str);
	  }
	  refresh();

	  mvprintw(14, 20, "%-40s", "");
	  mvprintw(15, 20, "%-40s", "Choose an item to drop (0-9)");
	  mvprintw(16, 20, "%-40s", "or hit any other key to exit");

	  int slot = getch();
	  slot -= 48;

	  if(slot >= 0 && slot <= 9)
	  {
		  object * ed = ((pc*)d->pc)->drop_item(slot);
		  if(ed)
		  {
			  ed->position[dim_y] = d->pc->position[dim_y];
			  ed->position[dim_x] = d->pc->position[dim_x];
			  d->objects.push_back(ed);
			  mvprintw(1, 20, "%-40s", "**Succesfully dropped item**");
			  refresh();
			  sleep(1);
		  }
	  }

	  io_display_all(d);
}

static void pc_expunge(dungeon_t *d)
{
	  mvprintw(2, 20, "%-40s", "Expunge Item:");
	  mvprintw(3, 20, "%-40s", "");

	  char i;
	  for(i = 0; i < 10; i++)
	  {
		  char str[51] = {};
		  str[0] = i + 48;
		  str[1] = ' ';
		  if(i < ((pc*)d->pc)->inventory.size())
		  {
			  strcat(str, ((pc*)d->pc)->inventory[i]->name.c_str());
		  }
		  mvprintw(4 + i, 20, "%-40s", str);
	  }
	  refresh();

	  mvprintw(14, 20, "%-40s", "");
	  mvprintw(15, 20, "%-40s", "Choose an item to expunge (0-9)");
	  mvprintw(16, 20, "%-40s", "or hit any other key to exit");

	  int slot = getch();
	  slot -= 48;

	  if(slot >= 0 && slot <= 9)
	  {
		  object * ed = ((pc*)d->pc)->drop_item(slot);
		  if(ed)
		  {
			  delete ed;
			  mvprintw(1, 20, "%-40s", "**Succesfully expunged item**");
			  refresh();
			  sleep(1);
		  }
	  }

	  io_display_all(d);
}

static void inspect_item(dungeon_t *d)
{

	  mvprintw(2, 20, "%-40s", "Inspect Item:");
	  mvprintw(3, 20, "%-40s", "");

	  char i;
	  for(i = 0; i < 10; i++)
	  {
		  char str[51] = {};
		  str[0] = i + 48;
		  str[1] = ' ';
		  if(i < ((pc*)d->pc)->inventory.size())
		  {
			  strcat(str, ((pc*)d->pc)->inventory[i]->name.c_str());
		  }
		  mvprintw(4 + i, 20, "%-40s", str);
	  }
	  refresh();

	  mvprintw(14, 20, "%-40s", "");
	  mvprintw(15, 20, "%-40s", "Choose an item to inspect (0-9)");
	  mvprintw(16, 20, "%-40s", "or hit any other key to exit");

	  int slot = getch();
	  slot -= 48;

	  if(slot >= 0 && slot <= 9)
	  {
		  if(((pc *)d->pc)->inventory.size() > slot)
		  {
			  std::vector<object *>::iterator it = ((pc *)d->pc)->inventory.begin();
			  for(;slot > 0; slot--, it++);
			  object * o = (*it);

			  if(o)
			  {
				  for(i=0; i < 24; i++)
					  mvprintw(i, 0, "%-80s", "");;

				  mvprintw(0, 0, "%-40s", "Hit ESC to continue");
				  mvprintw(2, 0, "name: %s", o->name.c_str());
				  mvprintw(3, 0, "speed: %d", o->speed);
				  mvprintw(4, 0, "damage: %d+%dd%d", o->damage.get_base(),
						  o->damage.get_number(), o->damage.get_sides());
				  mvprintw(5, 0, "description: %s", o->description.c_str());
				  refresh();
				  while(getch() != 27);
			  }
		  }
	  }

	  io_display_all(d);
}

static void io_list_monsters(dungeon_t *d)
{
  character **c;
  uint32_t x, y, count;

  c = (character **) malloc(d->num_monsters * sizeof (*c));

  /* Get a linear list of monsters */
  for (count = 0, y = 1; y < DUNGEON_Y - 1; y++) {
    for (x = 1; x < DUNGEON_X - 1; x++) {
      if (d->charmap[y][x] && d->charmap[y][x] != d->pc) {
        c[count++] = d->charmap[y][x];
      }
    }
  }

  /* Sort it by distance from PC */
  dungeon = d;
  qsort(c, count, sizeof (*c), compare_monster_distance);

  /* Display it */
  io_list_monsters_display(d, c, count);
  free(c);

  /* And redraw the dungeon */
  io_display_all(d);
}

void io_handle_input(dungeon_t *d)
{
  uint32_t fail_code;
  int key;

  do {
    switch (key = getch()) {
    case '7':
    case 'y':
    case KEY_HOME:
      fail_code = move_pc(d, 7);
      break;
    case '8':
    case 'k':
    case KEY_UP:
      fail_code = move_pc(d, 8);
      break;
    case '9':
    case 'u':
    case KEY_PPAGE:
      fail_code = move_pc(d, 9);
      break;
    case '6':
    case 'l':
    case KEY_RIGHT:
      fail_code = move_pc(d, 6);
      break;
    case '3':
    case 'n':
    case KEY_NPAGE:
      fail_code = move_pc(d, 3);
      break;
    case '2':
    case 'j':
    case KEY_DOWN:
      fail_code = move_pc(d, 2);
      break;
    case '1':
    case 'b':
    case KEY_END:
      fail_code = move_pc(d, 1);
      break;
    case '4':
    case 'h':
    case KEY_LEFT:
      fail_code = move_pc(d, 4);
      break;
    case '5':
    case ' ':
    case KEY_B2:
      fail_code = 0;
      break;
    case '>':
      fail_code = move_pc(d, '>');
      break;
    case '<':
      fail_code = move_pc(d, '<');
      break;
    case 'S':
      d->save_and_exit = 1;
      character_reset_turn(d->pc);
      fail_code = 0;
      break;
    case 'Q':
      /* Extra command, not in the spec.  Quit without saving.          */
      d->quit_no_save = 1;
      fail_code = 0;
      break;
    case 'T':
      /* New command.  Display the distances for tunnelers.             */
      io_display_tunnel(d);
      fail_code = 1;
      break;
    case 'D':
      /* New command.  Display the distances for non-tunnelers.         */
      io_display_distance(d);
      fail_code = 1;
      break;
    case 'H':
      /* New command.  Display the hardnesses.                          */
      io_display_hardness(d);
      fail_code = 1;
      break;
    case 's':
      /* New command.  Return to normal display after displaying some   *
       * special screen.                                                */
      io_display(d);
      fail_code = 1;
      break;
    case 'g':
      /* Teleport the PC to a random place in the dungeon.              */
      io_teleport_pc(d);
      fail_code = 0;
      break;
    case 'm':
      io_list_monsters(d);
      fail_code = 1;
      break;
    case 'e': // list pc equipment
        io_list_pc_equipment(d);
        fail_code = 1;
    	break;
    case 'i': // list pc inventory
        io_list_pc_inventory(d);
        fail_code = 1;
    	break;
    case 'I': // Inspect item (prints a carry slot's description)
    	inspect_item(d);
        fail_code = 1;
    	break;
    case 'x': // expunge an item from the game
    	pc_expunge(d);
        fail_code = 1;
    	break;
    case 'd': // drop an item
        pc_drop(d);
        fail_code = 1;
    	break;
    case 't': // take off an item
        pc_unequip(d);
        fail_code = 1;
    	break;
    case 'w': // wear an item
        pc_equip(d);
        fail_code = 1;
    	break;
    case 'a':
      io_display_all(d);
      fail_code = 1;
      break;
    case 'q':
      /* Demonstrate use of the message queue.  You can use this for *
       * printf()-style debugging (though gdb is probably a better   *
       * option.  Not that it matterrs, but using this command will  *
       * waste a turn.  Set fail_code to 1 and you should be able to *
       * figure out why I did it that way.                           */
      io_queue_message("This is the first message.");
      io_queue_message("Since there are multiple messages, "
                       "you will see \"more\" prompts.");
      io_queue_message("You can use any key to advance through messages.");
      io_queue_message("Normal gameplay will not resume until the queue "
                       "is empty.");
      io_queue_message("Long lines will be truncated, not wrapped.");
      io_queue_message("io_queue_message() is variadic and handles "
                       "all printf() conversion specifiers.");
      io_queue_message("Did you see %s?", "what I did there");
      io_queue_message("When the last message is displayed, there will "
                       "be no \"more\" prompt.");
      io_queue_message("Have fun!  And happy printing!");
      fail_code = 0;
      break;
    default:
      /* Also not in the spec.  It's not always easy to figure out what *
       * key code corresponds with a given keystroke.  Print out any    *
       * unhandled key here.  Not only does it give a visual error      *
       * indicator, but it also gives an integer value that can be used *
       * for that key in this (or other) switch statements.  Printed in *
       * octal, with the leading zero, because ncurses.h lists codes in *
       * octal, thus allowing us to do reverse lookups.  If a key has a *
       * name defined in the header, you can use the name here, else    *
       * you can directly use the octal value.                          */
      mvprintw(0, 0, "Unbound key: %#o ", key);
      fail_code = 1;
    }
  } while (fail_code);
}
