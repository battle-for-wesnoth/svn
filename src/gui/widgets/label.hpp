/* $Id$ */
/*
   copyright (c) 2008 by mark de wever <koraq@xs4all.nl>
   part of the battle for wesnoth project http://www.wesnoth.org/

   this program is free software; you can redistribute it and/or modify
   it under the terms of the gnu general public license version 2
   or at your option any later version.
   this program is distributed in the hope that it will be useful,
   but without any warranty.

   see the copying file for more details.
*/

#ifndef __GUI_WIDGETS_LABEL_HPP_INCLUDED__
#define __GUI_WIDGETS_LABEL_HPP_INCLUDED__

#include "gui/widgets/widget.hpp"

#include "gui/widgets/settings.hpp"

namespace gui2 {

class tlabel : public tcontrol
{
public:
	
	tlabel() :
		tcontrol(),
		canvas_()
	{}

	void set_width(const unsigned width);

	void set_height(const unsigned height);

	void set_label(const t_string& label);

	void mouse_hover(tevent_handler&);

	void draw(surface& canvas);

	// note we should check whether the label fits in the label
	tpoint get_best_size() const;

	void set_best_size(const tpoint& origin);
private:

	tcanvas canvas_;

	std::vector<tlabel_definition::tresolution>::const_iterator definition_;

	void resolve_definition();
};

} // namespace gui2

#endif

