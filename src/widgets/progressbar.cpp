#include "progressbar.hpp"

#include "../display.hpp"
#include "../font.hpp"
#include "../util.hpp"

namespace gui {

progress_bar::progress_bar(display& disp) : widget(disp), progress_(0)
{}

void progress_bar::set_progress_percent(int progress)
{
	progress_ = progress;
	set_dirty(true);
}

void progress_bar::draw_contents()
{
	surface const surf = disp().video().getSurface();
	SDL_Rect area = location();

	if(area.w >= 2 && area.h >= 2) {
		SDL_Rect inner_area = {area.x+1,area.y+1,area.w-2,area.h-2};
		SDL_FillRect(surf,&area,SDL_MapRGB(surf->format,0,0,0));
		SDL_FillRect(surf,&inner_area,SDL_MapRGB(surf->format,255,255,255));

		inner_area.w = (inner_area.w*progress_)/100;
		SDL_FillRect(surf,&inner_area,SDL_MapRGB(surf->format,150,0,0));

		const std::string text = str_cast(progress_) + "%";
		SDL_Rect text_area = font::text_area(text,font::SIZE_NORMAL);

		text_area.x = area.x + area.w/2 - text_area.w/2;
		text_area.y = area.y + area.h/2 - text_area.h/2;

		font::draw_text(&disp(),location(),font::SIZE_NORMAL,font::BLACK_COLOUR,text,text_area.x,text_area.y);
	}

	update_rect(location());
}

}
