#ifndef CURSOR_HPP_INCLUDED
#define CURSOR_HPP_INCLUDED

#include "SDL.h"

namespace cursor
{

struct manager
{
	manager();
	~manager();
};

enum CURSOR_TYPE { NORMAL, WAIT, MOVE, ATTACK, NUM_CURSORS };

void use_colour(bool value);

void set(CURSOR_TYPE type);

void draw(SDL_Surface* screen);
void undraw(SDL_Surface* screen);

struct setter
{
	setter(CURSOR_TYPE type);
	~setter();

private:
	CURSOR_TYPE old_;
};

}

#endif
