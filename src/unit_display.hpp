#ifndef UNIT_DISPLAY_HPP_INCLUDED
#define UNIT_DISPLAY_HPP_INCLUDED

#include "display.hpp"
#include "unit.hpp"

///the unit_display namespace contains a number of free functions
///which display units performing various on-screen actions - moving,
///attacking, and dying
namespace unit_display
{
///a function to display a unit moving along a given path
void move_unit(display& disp, const gamemap& map, const std::vector<gamemap::location>& path, unit& u);

///a function to show a unit fading out. Note that this only shows the effect, it doesn't
///actually kill the unit.
void unit_die(display& disp, const gamemap::location& loc, const unit& u);

///a function to make the unit on tile 'a' attack the unit on tile 'b'.
///the 'damage' will be subtracted from the unit's hitpoints, and a die effect will be
///displayed if the unit dies.
///true is returned if the defending unit is dead, and should be removed from the
///playing field.
bool unit_attack(display& disp, unit_map& units, const gamemap& map,
                 const gamemap::location& a, const gamemap::location& b, int damage,
                 const attack_type& attack);

}

#endif