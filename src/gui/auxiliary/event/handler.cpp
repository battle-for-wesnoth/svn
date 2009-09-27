/* $Id$ */
/*
   Copyright (C) 2009 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/auxiliary/event/handler.hpp"

#include "foreach.hpp"
#include "video.hpp"
#include "gui/auxiliary/event/dispatcher.hpp"
#include "gui/auxiliary/log.hpp"
#include "gui/widgets/helper.hpp"
#include "gui/widgets/widget.hpp"

#include <cassert>

/*
 * At some point in the future this event handler should become the main event
 * handler. This switch controls the experimental switch for that change.
 */
//#define MAIN_EVENT_HANDLER

/* Since this code is still very experimental it's not enabled yet. */
//#define ENABLE

namespace gui2 {

namespace event {

/***** Static data. *****/
class thandler;
static thandler* handler = NULL;

#ifdef MAIN_EVENT_HANDLER
static unsigned draw_interval = 0;
static unsigned event_poll_interval = 0;

/***** Static functions. *****/

/**
 * SDL_AddTimer() callback for the draw event.
 *
 * When this callback is called it pushes a new draw event in the event queue.
 *
 * @returns                       The new timer interval, 0 to stop.
 */
static Uint32 timer_sdl_draw_event(Uint32, void*)
{
//	DBG_GUI_E << "Pushing draw event in queue.\n";

	SDL_Event event;
	SDL_UserEvent data;

	data.type = DRAW_EVENT;
	data.code = 0;
	data.data1 = NULL;
	data.data2 = NULL;

	event.type = DRAW_EVENT;
	event.user = data;

	SDL_PushEvent(&event);
	return draw_interval;
}

/**
 * SDL_AddTimer() callback for the poll event.
 *
 * When this callback is called it will run the events in the SDL event queue.
 *
 * @returns                       The new timer interval, 0 to stop.
 */
static Uint32 timer_sdl_poll_events(Uint32, void*)
{
	try {
		events::pump();
	} catch(CVideo::quit&) {
		return 0;
	}
	return event_poll_interval;
}
#endif

/***** thandler class. *****/

/**
 * This singleton class handles all events.
 *
 * It's a new experimental class.
 */
class thandler
	: public events::handler
{
public:
	thandler();

	~thandler();

	/** Inherited from events::handler. */
	void handle_event(const SDL_Event& event);

	/**
	 * Connects a dispatcher.
	 *
	 * @param dispatcher              The dispatcher to connect.
	 */
	void connect(tdispatcher* dispatcher);

	/**
	 * Disconnects a dispatcher.
	 *
	 * @param dispatcher              The dispatcher to disconnect.
	 */
	void disconnect(tdispatcher* dispatcher);

	/** The dispatcher that captured the mouse focus. */
	tdispatcher* mouse_focus;

private:

	/***** Handlers *****/

	/** Fires a draw event. */
	void draw();


	/**
	 * Fires a generic mouse event.
	 *
	 * @param event                  The event to fire.
	 * @param position               The position of the mouse.
	 */
	void mouse(const tevent event, const tpoint& position);

	/**
	 * Fires a mouse button up event.
	 *
	 * @param event                  The event to fire.
	 * @param position               The position of the mouse.
	 * @param button                 The SDL id of the button that caused the
	 *                               event.
	 */
	void mouse_button_up(const tpoint& position, const Uint8 button);

	/**
	 * Fires a mouse button down event.
	 *
	 * @param event                  The event to fire.
	 * @param position               The position of the mouse.
	 * @param button                 The SDL id of the button that caused the
	 *                               event.
	 */
	void mouse_button_down(const tpoint& position, const Uint8 button);

	/**
	 * Fires a keyboard event which has no parameters.
	 *
	 * This can happen for example when the mouse wheel is used.
	 *
	 * @param event                  The event to fire.
	 */
	void keyboard(const tevent event);

	/**
	 * The dispatchers.
	 *
	 * The order of the items in the list is also the z-order the front item
	 * being the one completely in the background and the back item the one
	 * completely in the foreground.
	 */
	std::vector<tdispatcher*> dispatchers_;

	/**
	 * Needed to determine which dispatcher gets the keyboard events.
	 *
	 * NOTE the keyboard events aren't really wired in yet so doesn't do much.
	 */
	tdispatcher* keyboard_focus_;
};

thandler::thandler()
	: events::handler(false)
	, mouse_focus(NULL)
	, dispatchers_()
	, keyboard_focus_(NULL)
{
	if(SDL_WasInit(SDL_INIT_TIMER) == 0) {
		if(SDL_InitSubSystem(SDL_INIT_TIMER) == -1) {
			assert(false);
		}
	}

	// The event context is created now we join it.
#ifdef ENABLE
	join();
#endif
}

thandler::~thandler()
{
#ifdef ENABLE
	leave();
#endif
}

void thandler::handle_event(const SDL_Event& event)
{
	/** No dispatchers drop the event. */
	if(dispatchers_.empty()) {
		return;
	}

	switch(event.type) {
		case SDL_MOUSEMOTION:
			mouse(SDL_MOUSE_MOTION, tpoint(event.motion.x, event.motion.y));
			break;

		case SDL_MOUSEBUTTONDOWN:
			mouse_button_down(tpoint(event.button.x, event.button.y)
					, event.button.button);
			break;

		case SDL_MOUSEBUTTONUP:
			mouse_button_up(tpoint(event.button.x, event.button.y)
					, event.button.button);
			break;

		case HOVER_EVENT:
//			hover();
			break;

		case HOVER_REMOVE_POPUP_EVENT:
//			remove_popup();
			break;

		case DRAW_EVENT:
			draw();
			break;

		case CLOSE_WINDOW_EVENT:
//			close_window();
			break;

		case SDL_KEYDOWN:
/*			key_down(event.key.keysym.sym
					, event.key.keysym.mod
					, event.key.keysym.unicode);
*/			break;

		case SDL_VIDEORESIZE:
//			video_resize();
			break;

#if defined(_X11) && !defined(__APPLE__)
			case SDL_SYSWMEVENT: {
/*				DBG_GUI_E << "Event: System event.\n";
				//clipboard support for X11
				system_wm_event(event);
*/				break;
			}
#endif

		default:
			WRN_GUI_E << "Unhandled event "
					<< static_cast<Uint32>(event.type) << ".\n";
			break;
	}
}

void thandler::connect(tdispatcher* dispatcher)
{
	assert(std::find(dispatchers_.begin(), dispatchers_.end(), dispatcher)
			== dispatchers_.end());
#ifdef GUI2_NEW_EVENT_HANDLING
	if(dispatchers_.empty()) {
		join();
	}
#endif
	dispatchers_.push_back(dispatcher);
}

void thandler::disconnect(tdispatcher* dispatcher)
{
	std::vector<tdispatcher*>::iterator itor =
			std::find(dispatchers_.begin(), dispatchers_.end(), dispatcher);
	assert(itor != dispatchers_.end());

	dispatchers_.erase(itor);

	if(dispatcher == mouse_focus) {
		mouse_focus = NULL;
	}
	if(dispatcher == keyboard_focus_) {
		keyboard_focus_ = NULL;
	}

	assert(std::find(dispatchers_.begin(), dispatchers_.end(), dispatcher)
			== dispatchers_.end());
#ifdef GUI2_NEW_EVENT_HANDLING
	if(dispatchers_.empty()) {
		leave();
	}
#endif
}

void thandler::draw()
{
	// Don't display this event since it floods the screen
	//DBG_GUI_E << "Firing " << DRAW << ".\n";

	/** @todo Need to evaluate which windows really to redraw. */
	foreach(tdispatcher* dispatcher, dispatchers_) {
		dispatcher->fire(DRAW, dynamic_cast<twidget&>(*dispatcher));
	}
}

void thandler::mouse(const tevent event, const tpoint& position)
{
	DBG_GUI_E << "Firing: " << event << ".\n";

	if(mouse_focus) {
		mouse_focus->fire(event
				, dynamic_cast<twidget&>(*mouse_focus)
				, position);
	} else {

		for(std::vector<tdispatcher*>::reverse_iterator ritor =
				dispatchers_.rbegin(); ritor != dispatchers_.rend(); ++ritor) {

			if((**ritor).get_mouse_behaviour() == tdispatcher::all) {
				(**ritor).fire(event
						, dynamic_cast<twidget&>(**ritor)
						, position);
				break;
			}
			if((**ritor).get_mouse_behaviour() == tdispatcher::none) {
				continue;
			}
			if((**ritor).is_at(position)) {
				(**ritor).fire(event
						, dynamic_cast<twidget&>(**ritor)
						, position);
				break;
			}
		}
	}
}

void thandler::mouse_button_up(const tpoint& position, const Uint8 button)
{
	switch(button) {
		case SDL_BUTTON_LEFT :
			mouse(SDL_LEFT_BUTTON_UP, position);
			break;
		case SDL_BUTTON_MIDDLE :
			mouse(SDL_MIDDLE_BUTTON_UP, position);
			break;
		case SDL_BUTTON_RIGHT :
			mouse(SDL_RIGHT_BUTTON_UP, position);
			break;

		case SDL_WHEEL_LEFT :
			keyboard(SDL_WHEEL_LEFT);
			break;
		case SDL_WHEEL_RIGHT :
			keyboard(SDL_WHEEL_RIGHT);
			break;
		case SDL_WHEEL_UP :
			keyboard(SDL_WHEEL_UP);
			break;
		case SDL_WHEEL_DOWN :
			keyboard(SDL_WHEEL_DOWN);
			break;

		default:
			WRN_GUI_E << "Unhandled 'mouse button down' event for button "
					<< static_cast<Uint32>(button) << ".\n";
			break;
	}
}

void thandler::mouse_button_down(const tpoint& position, const Uint8 button)
{
	// The wheel buttons generate and up and down event we handle the
	// up event so ignore the mouse if it's a down event. Handle it
	// here to avoid a warning.
	if(button == SDL_BUTTON_WHEELUP
			|| button == SDL_BUTTON_WHEELDOWN
			|| button == SDL_BUTTON_WHEELLEFT
			|| button == SDL_BUTTON_WHEELRIGHT) {

		return;
	}

	switch(button) {
		case SDL_BUTTON_LEFT :
			mouse(SDL_LEFT_BUTTON_DOWN, position);
			break;
		case SDL_BUTTON_MIDDLE :
			mouse(SDL_MIDDLE_BUTTON_DOWN, position);
			break;
		case SDL_BUTTON_RIGHT :
			mouse(SDL_RIGHT_BUTTON_DOWN, position);
			break;
		default:
			WRN_GUI_E << "Unhandled 'mouse button down' event for button "
					<< static_cast<Uint32>(button) << ".\n";
			break;
	}
}

void thandler::keyboard(const tevent //event
		)
{
	/*
	DBG_GUI_E << "Firing: " << event << ".\n";

	assert(!dispatchers_.empty());

	if(keyboard_focus_) {
		keyboard_focus_->fire(event);
	} else {
		dispatchers_.back()->fire(event);
	}
	*/
}

/***** tmanager class. *****/

tmanager::tmanager()
{
	handler = new thandler();

#ifdef MAIN_EVENT_HANDLER
	draw_interval = 30;
	SDL_AddTimer(draw_interval, timer_sdl_draw_event, NULL);

	event_poll_interval = 10;
	SDL_AddTimer(event_poll_interval, timer_sdl_poll_events, NULL);
#endif
}

tmanager::~tmanager()
{
	delete handler;
	handler = NULL;

#ifdef MAIN_EVENT_HANDLER
	draw_interval = 0;
	event_poll_interval = 0;
#endif
}

/***** free functions class. *****/

void connect_dispatcher(tdispatcher* dispatcher)
{
	assert(handler);
	assert(dispatcher);
	handler->connect(dispatcher);
}

void disconnect_dispatcher(tdispatcher* dispatcher)
{
	assert(handler);
	assert(dispatcher);
	handler->disconnect(dispatcher);
}

void capture_mouse(tdispatcher* dispatcher)
{
	assert(handler);
	assert(dispatcher);
	handler->mouse_focus = dispatcher;
}

void release_mouse(tdispatcher* dispatcher)
{
	assert(handler);
	assert(dispatcher);
	if(handler->mouse_focus == dispatcher) {
		handler->mouse_focus = NULL;
	}
}

std::ostream& operator<<(std::ostream& stream, const tevent event)
{
	switch(event) {
		case DRAW                   : stream << "draw"; break;
		case SDL_MOUSE_MOTION       : stream << "SDL mouse motion"; break;
		case MOUSE_ENTER            : stream << "mouse enter"; break;
		case MOUSE_LEAVE            : stream << "mouse leave"; break;
		case MOUSE_MOTION           : stream << "mouse motion"; break;
		case SDL_LEFT_BUTTON_DOWN   : stream << "SDL left button down"; break;
		case SDL_LEFT_BUTTON_UP     : stream << "SDL left button up"; break;
		case SDL_MIDDLE_BUTTON_DOWN : stream << "SDL middle button down"; break;
		case SDL_MIDDLE_BUTTON_UP   : stream << "SDL middle button up"; break;
		case SDL_RIGHT_BUTTON_DOWN  : stream << "SDL right button down"; break;
		case SDL_RIGHT_BUTTON_UP    : stream << "SDL right button up"; break;
		case SDL_WHEEL_LEFT         : stream << "SDL wheel left"; break;
		case SDL_WHEEL_RIGHT        : stream << "SDL wheel right"; break;
		case SDL_WHEEL_UP           : stream << "SDL wheel up"; break;
		case SDL_WHEEL_DOWN         : stream << "SDL wheel down"; break;
		case SDL_KEY_DOWN           : stream << "SDL key down"; break;
		default                     : assert(false);
	}

	return stream;
}

} // namespace event

} // namespace gui2

