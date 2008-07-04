/* $Id$ */
/*
   Copyright (C) 2008 by Pauli Nieminen <paniemin@cc.hut.fi>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include <iostream>
#include "tests/utils/fake_event_source.hpp"

namespace test_utils {
	/**
	 * Base class for all event nodes to be used to fire fake input events
	 **/
	event_node::event_node(const size_t time, const SDL_Event& event) : time_(time), fired_(false), event_(event)
	{}

	event_node::~event_node()
	{ }

	void event_node::fire_event()
	{
		const int number_of_events = 1;
		const Uint32 mask = 0;
		SDL_PeepEvents(&event_, number_of_events, SDL_ADDEVENT, mask);
		fired_ = true;
	}

	/**
	 * @return true if this should stop firing events
	 **/
	bool event_node::test_if_should_fire(const size_t start_time, const size_t time_now) const
	{
		return time_now - start_time >= time_;
	}

	bool event_node::is_fired() const
	{
		return fired_;
	}

	/**
	 * We want the smallestat the top
	 **/
	bool event_node::operator<(const event_node& o) const
	{ return time_ > o.time_; }

	event_node_keyboard::event_node_keyboard(size_t time, SDL_Event& event) : event_node(time,event)
	{}
	void event_node_keyboard::fire_event()
	{
		event_node::fire_event();
		static int num_keys = 300;
		Uint8* key_list = SDL_GetKeyState( &num_keys );
		if (event_.type == SDL_KEYDOWN)
			key_list[SDLK_ESCAPE] = 5;
		else
			key_list[SDLK_ESCAPE] = 0;
	}

	fake_event_source::fake_event_source() : start_time_(SDL_GetTicks())
	{
	}

	fake_event_source::~fake_event_source()
	{
		// send all still queued events
		// so keyboard/mouse state is restored
		while(!queue_.empty())
		{
			queue_.top()->fire_event();
			queue_.pop();
		}
		events::pump();
	}


	void fake_event_source::add_event(const size_t time, const SDL_Event& event)
	{
		event_node_ptr new_node(new event_node(time,event));
		queue_.push(new_node);
	}

	void fake_event_source::add_event(event_node_ptr new_node)
	{
		queue_.push(new_node);
	}

	void fake_event_source::start()
	{
		start_time_ = SDL_GetTicks();
	}	 

	SDL_Event fake_event_source::make_key_event(Uint8 type, const SDLKey key, const SDLMod mod)
	{
		SDL_Event event;
		event.type = type;
		if (type == SDL_KEYDOWN)
			event.key.state = SDL_PRESSED;
		else
			event.key.state = SDL_RELEASED;
		event.key.keysym.sym = key;
		event.key.keysym.scancode = 65; // Always report 65 as scancode
		event.key.keysym.mod = mod;
		event.key.keysym.unicode = 0; // Unicode disabled
		return event;
	}

	event_node_ptr fake_event_source::press_key(const size_t time, const SDLKey key, const SDLMod mod)
	{
		SDL_Event event = make_key_event(SDL_KEYDOWN, key, mod);
		event_node_ptr new_key(new event_node_keyboard(time, event));
		add_event(new_key);
		return new_key;
	}

	event_node_ptr fake_event_source::release_key(const size_t time, const SDLKey key, const SDLMod mod)
	{
		SDL_Event event = make_key_event(SDL_KEYUP, key, mod);
		event_node_ptr new_key(new event_node_keyboard(time, event));
		add_event(new_key);
		return new_key;
	}

	void fake_event_source::process(events::pump_info& /*info*/)
	{
		if (queue_.empty())
			return;
		size_t now = SDL_GetTicks();
		while (!queue_.empty() 
				&& queue_.top()->test_if_should_fire(start_time_, now))
		{
			queue_.top()->fire_event();
			queue_.pop();
		}
	}
}
