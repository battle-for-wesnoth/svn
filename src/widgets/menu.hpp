#ifndef WIDGET_MENU_HPP_INCLUDED
#define WIDGET_MENU_HPP_INCLUDED

#include <string>
#include <vector>

#include "../display.hpp"
#include "../events.hpp"
#include "../sdl_utils.hpp"

#include "scrollbar.hpp"
#include "button.hpp"

#include "SDL.h"

namespace gui {

class menu : public events::handler
{
public:
	menu(display& disp, const std::vector<std::string>& items,
	     bool click_selects=false, int max_height=-1);

	int height() const;
	int width() const;

	int selection() const;

	void set_loc(int x, int y);
	void set_width(int w);

	void redraw(); //forced redraw
	
	// allows user to change_item while running (dangerous)
	void change_item(int pos1,int pos2,std::string str);
	
	int process(int x, int y, bool button,bool up_arrow,bool down_arrow,
	            bool page_up, bool page_down, int select_item=-1);

	bool double_clicked() const;

private:
	size_t max_items_onscreen() const;

	int max_height_;
	mutable int max_items_, item_height_;

	void calculate_position();
	void key_press(SDLKey key);

	void handle_event(const SDL_Event& event);

	bool show_scrollbar() const;

	display* display_;
	int x_, y_;
	std::vector<std::vector<std::string> > items_;
	mutable std::vector<int> column_widths_;

	scoped_sdl_surface buffer_;
	int selected_;
	bool click_selects_;
	bool previous_button_;
	bool drawn_;

	bool show_result_;

	mutable int height_;
	mutable int width_;

	mutable int first_item_on_screen_;
	gui::button uparrow_, downarrow_;

	bool double_clicked_;

	const std::vector<int>& column_widths() const;
	void draw_item(int item);
	void draw();
	int hit(int x, int y) const;

	mutable std::map<int,SDL_Rect> itemRects_;

	SDL_Rect get_item_rect(int item) const;
	size_t get_item_height_internal(int item) const;
	size_t get_item_height(int item) const;
	int items_start() const;

	int items_end() const;
	int items_height() const;

	/// Set the vertical size of the scroll bar grip. The size should
	/// vary inversely with the ratio of the number of items to the 
	/// size of the viewing rect
	void set_scrollbar_height();
	gui::scrollbar scrollbar_;
	int scrollbar_width_;
	int scrollbar_height_;
};

}

#endif
