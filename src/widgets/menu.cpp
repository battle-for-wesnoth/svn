#include "menu.hpp"

#include "../font.hpp"
#include "../show_dialog.hpp"

#include <numeric>

namespace {
const size_t max_menu_items = 18;
const size_t menu_font_size = 16;
const size_t menu_cell_padding = 10;
}

namespace gui {

menu::menu(display& disp, const std::vector<std::string>& items,
           bool click_selects)
        : display_(&disp), x_(0), y_(0), buffer_(NULL),
          selected_(-1), click_selects_(click_selects),
          previous_button_(true), drawn_(false), height_(-1), width_(-1),
		  first_item_on_screen_(0),
		  uparrow_(disp,"",gui::button::TYPE_PRESS,"uparrow"),
          downarrow_(disp,"",gui::button::TYPE_PRESS,"downarrow")
{
	for(std::vector<std::string>::const_iterator item = items.begin();
	    item != items.end(); ++item) {
		items_.push_back(config::split(*item));

		//make sure there is always at least one item
		if(items_.back().empty())
			items_.back().push_back(" ");
	}
}

int menu::height() const
{
	if(height_ == -1) {
		height_ = 0;
		for(size_t i = 0; i != items_.size() && i != max_menu_items; ++i) {
			height_ += get_item_rect(i).h;
		}

		if(items_.size() > max_menu_items) {
			height_ += uparrow_.height() + downarrow_.height();
		}
	}

	return height_;
}

int menu::width() const
{
	if(width_ == -1) {
		const std::vector<int>& widths = column_widths();
		width_ = std::accumulate(widths.begin(),widths.end(),0);
	}

	return width_;
}

int menu::selection() const { return selected_; }

void menu::set_loc(int x, int y)
{
	x_ = x;
	y_ = y;

	const int w = width();

	SDL_Rect portion = {x_,y_,w,height()};
	SDL_Surface* const screen = display_->video().getSurface();
	buffer_.assign(get_surface_portion(screen, portion));

	if(items_.size() > max_menu_items) {
		uparrow_.set_x(x_);
		uparrow_.set_y(y_);
		downarrow_.set_x(x_);
		downarrow_.set_y(y_+items_end());
	}
}

int menu::process(int x, int y, bool button,bool up_arrow,bool down_arrow,
                  bool page_up, bool page_down, int select_item)
{
	if(items_.size() > size_t(max_menu_items)) {
		const bool up = uparrow_.process(x,y,button);
		if(up && first_item_on_screen_ > 0) {
			itemRects_.clear();
			--first_item_on_screen_;

			draw();
		}

		const bool down = downarrow_.process(x,y,button);
		if(down &&
		   size_t(first_item_on_screen_+max_menu_items) < items_.size()) {
			itemRects_.clear();
			++first_item_on_screen_;
			draw();
		}
	}

	if(select_item >= 0 && size_t(select_item) < items_.size()) {
		selected_ = select_item;
		if(selected_ < first_item_on_screen_) {
			itemRects_.clear();
			first_item_on_screen_ = selected_;
		}

		if(size_t(selected_ - first_item_on_screen_) >= max_menu_items) {
			itemRects_.clear();
			first_item_on_screen_ = selected_ - max_menu_items - 1;
		}

		draw();
	}

	if(up_arrow && !click_selects_ && selected_ > 0) {
		--selected_;
		if(selected_ < first_item_on_screen_) {
			itemRects_.clear();
			first_item_on_screen_ = selected_;
		}

		draw();
	}

	if(down_arrow && !click_selects_ && selected_ < int(items_.size()-1)) {
		++selected_;
		if(size_t(selected_ - first_item_on_screen_) == max_menu_items) {
			itemRects_.clear();
			++first_item_on_screen_;
		}

		draw();
	}

	if(page_up && !click_selects_) {
		selected_ -= max_menu_items;
		if(selected_ < 0)
			selected_ = 0;

		itemRects_.clear();
		first_item_on_screen_ = selected_;

		draw();
	}

	if(page_down && !click_selects_) {
		selected_ += max_menu_items;
		if(size_t(selected_) >= items_.size())
			selected_ = items_.size()-1;

		first_item_on_screen_ = selected_ - (max_menu_items-1);
		if(first_item_on_screen_ < 0)
			first_item_on_screen_ = 0;

		itemRects_.clear();

		draw();
	}

	const int starting_selected = selected_;

	const int hit_item = hit(x,y);

	if(click_selects_) {
		selected_ = hit_item;
		if(button && !previous_button_)
			return selected_;
		else {
			if(!drawn_ || selected_ != starting_selected)
				draw();
			previous_button_ = button;
			return -1;
		}
	}

	if(button && hit_item != -1){
		selected_ = hit_item;
	}

	if(selected_ == -1)
		selected_ = 0;

	if(selected_ != starting_selected)
		draw();

	return -1;
}

const std::vector<int>& menu::column_widths() const
{
	if(column_widths_.empty()) {
		for(size_t row = 0; row != items_.size(); ++row) {
			for(size_t col = 0; col != items_[row].size(); ++col) {
				static const SDL_Rect area =
				                {0,0,display_->x(),display_->y()};

				const SDL_Rect res =
					font::draw_text(NULL,area,menu_font_size,
					          font::NORMAL_COLOUR,items_[row][col],x_,y_);

				if(col == column_widths_.size()) {
					column_widths_.push_back(res.w + menu_cell_padding);
				} else if(res.w > column_widths_[col] - menu_cell_padding) {
					column_widths_[col] = res.w + menu_cell_padding;
				}
			}
		}
	}

	return column_widths_;
}

void menu::draw_item(int item)
{
	SDL_Rect rect = get_item_rect(item);
	if(rect.w == 0)
		return;

	if(buffer_.get() != NULL) {
		const int ypos = items_start()+(item-first_item_on_screen_)*rect.h;
		SDL_Rect srcrect = {0,ypos,rect.w,rect.h};
		SDL_BlitSurface(buffer_,&srcrect,
		                display_->video().getSurface(),&rect);
	}

	gui::draw_solid_tinted_rectangle(x_,rect.y,width(),rect.h,
	                                 item == selected_ ? 150:0,0,0,
	                                 item == selected_ ? 0.6 : 0.2,
	                                 display_->video().getSurface());

	SDL_Rect area = {0,0,display_->x(),display_->y()};

	const std::vector<int>& widths = column_widths();

	int xpos = rect.x;
	for(size_t i = 0; i != items_[item].size(); ++i) {
		font::draw_text(display_,area,menu_font_size,font::NORMAL_COLOUR,
		                items_[item][i],xpos,rect.y);
		xpos += widths[i];
	}
}

void menu::draw()
{
	drawn_ = true;

	for(size_t i = 0; i != items_.size(); ++i)
		draw_item(i);

	display_->video().flip();
}

int menu::hit(int x, int y) const
{
	if(x > x_ && x < x_ + width() && y > y_ && y < y_ + height()){
		for(size_t i = 0; i != items_.size(); ++i) {
			const SDL_Rect& rect = get_item_rect(i);
			if(y > rect.y && y < rect.y + rect.h)
				return i;
		}
	}

	return -1;
}

SDL_Rect menu::get_item_rect(int item) const
{
	const SDL_Rect empty_rect = {0,0,0,0};
	if(item < first_item_on_screen_ ||
	   size_t(item) >= first_item_on_screen_ + max_menu_items) {
		return empty_rect;
	}

	const std::map<int,SDL_Rect>::const_iterator i = itemRects_.find(item);
	if(i != itemRects_.end())
		return i->second;

	int y = y_ + items_start();
	if(item != first_item_on_screen_) {
		const SDL_Rect& prev = get_item_rect(item-1);
		y = prev.y + prev.h;
	}

	static const SDL_Rect area = {0,0,display_->x(),display_->y()};

	SDL_Rect res = font::draw_text(NULL,area,menu_font_size,
		                   font::NORMAL_COLOUR,items_[item][0],x_,y);

	res.w = width();

	//only insert into the cache if the menu's co-ordinates have
	//been initialized
	if(x_ > 0 && y_ > 0)
		itemRects_.insert(std::pair<int,SDL_Rect>(item,res));

	return res;
}

int menu::items_start() const
{
	if(items_.size() > max_menu_items)
		return uparrow_.height();
	else
		return 0;
}

int menu::items_end() const
{
	if(items_.size() > max_menu_items)
		return height() - downarrow_.height();
	else
		return height();
}

int menu::items_height() const
{
	return items_end() - items_start();
}

}
