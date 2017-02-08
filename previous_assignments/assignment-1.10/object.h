/*
 * object.h
 *
 *  Created on: Apr 1, 2016
 *      Author: Joe
 */

#ifndef SRC_OBJECT_H_
#define SRC_OBJECT_H_

#include "dice.h"
#include "utils.h"
#include "dims.h"


typedef struct dungeon dungeon_t;

class object {
 public:
  std::string name, description;
  object_type_t type;
  uint32_t color, hit, dodge, defense, weight, speed, attribute, value;
  dice damage;
  char symbol;
  pair_t position;

  object() : name(),    description(), type(objtype_no_type),
                         color(0),  hit(0),
                         dodge(0),   defense(0),     weight(0),
                         speed(0),   attribute(0),   value(0),
						 damage(), symbol('*')
  {
  }
  object(std::string name, std::string description, object_type_t type,
		  uint32_t color, uint32_t hit, uint32_t dodge, uint32_t defense,
		  uint32_t weight, uint32_t speed, uint32_t attr, uint32_t value,
		  dice damage, char symbol)
  : name(name),    description(description), type(type),
    color(color),  hit(hit),
    dodge(dodge),   defense(defense),     weight(weight),
    speed(speed),   attribute(attr),   value(value),
  	damage(damage), symbol(symbol)
  {
	  position[dim_y] = 0;
	  position[dim_x] = 0;
  }
  void set(const std::string &name,
           const std::string &description,
           const object_type_t type,
           const uint32_t color,
           const dice &hit,
           const dice &damage,
           const dice &dodge,
           const dice &defence,
           const dice &weight,
           const dice &speed,
           const dice &attrubute,
           const dice &value);
  std::ostream &print(std::ostream &o);
  /* Need all these accessors because otherwise there is a *
   * circular dependancy that is difficult to get around.  */
  inline const std::string &get_name() const { return name; }
  inline const std::string &get_description() const { return description; }
  inline const object_type_t get_type() const { return type; }
  inline const uint32_t get_color() const { return color; }
  inline const uint32_t &get_hit() const { return hit; }
  inline const dice &get_damage() const { return damage; }
  inline const uint32_t &get_dodge() const { return dodge; }
  inline const uint32_t &get_defense() const { return defense; }
  inline const uint32_t &get_weight() const { return weight; }
  inline const uint32_t &get_speed() const { return speed; }
  inline const uint32_t &get_attribute() const { return attribute; }
  inline const uint32_t &get_value() const { return value; }
};

void gen_objects(dungeon_t *d, uint32_t numobj);

#endif /* SRC_OBJECT_H_ */
