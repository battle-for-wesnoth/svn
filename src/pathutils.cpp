#include "global.hpp"

#include "pathutils.hpp"
#include "util.hpp"

size_t distance_between(const gamemap::location& a, const gamemap::location& b)
{
	const size_t hdistance = abs(a.x - b.x);

	const size_t vpenalty = (is_even(a.x) && is_odd(b.x) && a.y < b.y ||
	                         is_even(b.x) && is_odd(a.x) && b.y < a.y) ? 1:0;
	const size_t vdistance = abs(a.y - b.y) + vpenalty;
	const size_t vsavings = minimum<int>(vdistance,hdistance/2 + hdistance%2);

	return hdistance + vdistance - vsavings;
}

void get_adjacent_tiles(const gamemap::location& a, gamemap::location* res)
{
	res->x = a.x;
	res->y = a.y-1;
	++res;
	res->x = a.x+1;
	res->y = a.y - (is_even(a.x) ? 1:0);
	++res;
	res->x = a.x+1;
	res->y = a.y + (is_odd(a.x) ? 1:0);
	++res;
	res->x = a.x;
	res->y = a.y+1;
	++res;
	res->x = a.x-1;
	res->y = a.y + (is_odd(a.x) ? 1:0);
	++res;
	res->x = a.x-1;
	res->y = a.y - (is_even(a.x) ? 1:0);
}

bool tiles_adjacent(const gamemap::location& a, const gamemap::location& b)
{
	//two tiles are adjacent if y is different by 1, and x by 0, or if
	//x is different by 1 and y by 0, or if x and y are each different by 1,
	//and the x value of the hex with the greater y value is even

	const int xdiff = abs(a.x - b.x);
	const int ydiff = abs(a.y - b.y);
	return ydiff == 1 && a.x == b.x || xdiff == 1 && a.y == b.y ||
	       xdiff == 1 && ydiff == 1 && (a.y > b.y ? is_even(a.x) : is_even(b.x));
}

