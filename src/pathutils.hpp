#include "map.hpp"

//function which tells if two locations are adjacent.
bool tiles_adjacent(const gamemap::location& a, const gamemap::location& b);

//function which, given a location, will place all adjacent locations in
//res. res must point to an array of 6 location objects.
void get_adjacent_tiles(const gamemap::location& a, gamemap::location* res);

//function which gives the number of hexes between two tiles (i.e. the minimum
//number of hexes that have to be traversed to get from one hex to the other)
size_t distance_between(const gamemap::location& a, const gamemap::location& b);
