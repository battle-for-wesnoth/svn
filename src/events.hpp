#ifndef EVENTS_HPP_INCLUDED
#define EVENTS_HPP_INCLUDED

#include "SDL.h"

//our user-defined double-click event type
#define DOUBLE_CLICK_EVENT SDL_USEREVENT

namespace events
{

//an object which prevents resizing of the screen occuring during
//its lifetime.
struct resize_lock {
	resize_lock();
	~resize_lock();
};

//any classes that derive from this class will automatically
//receive sdl events through the handle function for their lifetime,
//while the event context they were created in is active.
//
//NOTE: an event_context object must be initialized before a handler object
//can be initialized, and the event_context must be destroyed after
//the handler is destroyed.
class handler
{
public:
	virtual void handle_event(const SDL_Event& event) = 0;
	virtual void process() {}
	virtual void draw() const {}
protected:
	handler();
	virtual ~handler();

private:
	int unicode_;
};

//event_context objects control the handler objects that SDL events are sent
//to. When an event_context is created, it will become the current event context.
//event_context objects MUST be created in LIFO ordering in relation to each other,
//and in relation to handler objects. That is, all event_context objects should be
//created as automatic/stack variables.
//
//handler objects need not be created as automatic variables (e.g. you could put
//them in a vector) however you must guarantee that handler objects are destroyed
//before their context is destroyed
struct event_context
{
	event_context();
	~event_context();
};

//causes events to be dispatched to all handler objects.
void pump();

void raise_process_event();
void raise_draw_event();
}

#endif
