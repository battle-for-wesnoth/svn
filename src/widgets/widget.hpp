#ifndef WIDGET_HPP_INCLUDED
#define WIDGET_HPP_INCLUDED

#include "../events.hpp"
#include "../sdl_utils.hpp"

#include "SDL.h"

#include <vector>

class display;

namespace gui {

class widget : public events::handler
{
public:
	SDL_Rect const &location() const;
	void set_location(SDL_Rect const &rect);
	void set_location(int x, int y);
	void set_width(unsigned w);
	void set_height(unsigned h);
	void set_measurements(unsigned w, unsigned h);

	unsigned width() const;
	unsigned height() const;

	virtual bool focus() const;
	void set_focus(bool focus);

	virtual void hide(bool value = true);
	bool hidden() const;

	void set_clip_rect(const SDL_Rect& rect);
	
	//Function to set the widget to draw in 'volatile' mode.
	//When in 'volatile' mode, instead of using the normal
	//save-background-redraw-when-dirty procedure, redrawing is done
	//every frame, and then after every frame the area under the widget
	//is restored to the state it was in before the frame. This is useful
	//for drawing widgets with alpha components in volatile settings where
	//the background may change at any time.
	//(e.g. for putting widgets on top of the game map)
	void set_volatile(bool val=true);

	void set_dirty(bool dirty=true);
	bool dirty() const;

	void set_help_string(const std::string& str);

	virtual void process_help_string(int mousex, int mousey);

protected:
	widget(widget const &o);
	widget(display& disp);
	virtual ~widget();

	// During each relocation, this function should be called to register
	// the rectangles the widget needs to refresh automatically
	void bg_register(SDL_Rect const &rect);
	void bg_restore() const;
	void bg_restore(SDL_Rect const &rect) const;

	display& disp() const { return *disp_; }

	virtual void handle_event(SDL_Event const &event) {}

	virtual void draw();
	virtual void draw_contents() {};
	virtual void update_location(SDL_Rect const &rect);

private:
	void volatile_draw();
	void volatile_undraw();

	void bg_update();
	void bg_cancel();

	display* disp_;
	std::vector< surface_restorer > restorer_;
	SDL_Rect rect_;
	bool focus_;		// Should user input be ignored?
	mutable bool needs_restore_; // Have we drawn ourselves, so that if moved, we need to restore the background?

	enum { UNINIT, HIDDEN, DIRTY, DRAWN } state_;
	SDL_Rect clip_rect_;

	bool volatile_;

	std::string help_text_;
	int help_string_;
};

}

#endif
