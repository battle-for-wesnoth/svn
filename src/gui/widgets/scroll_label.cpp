/* $Id$ */
/*
   copyright (C) 2008 by mark de wever <koraq@xs4all.nl>
   part of the battle for wesnoth project http://www.wesnoth.org/

   this program is free software; you can redistribute it and/or modify
   it under the terms of the gnu general public license version 2
   or at your option any later version.
   this program is distributed in the hope that it will be useful,
   but without any warranty.

   see the copying file for more details.
*/

#include "gui/widgets/scroll_label.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/scrollbar.hpp"
#include "gui/widgets/spacer.hpp"

namespace gui2 {

tscroll_label::tscroll_label() 
	: tvertical_scrollbar_container_(COUNT)
	, state_(ENABLED)
	, label_(NULL)
{
}

tscroll_label::~tscroll_label() 
{
	delete label_;
}

void tscroll_label::set_label(const t_string& label)
{
	// Inherit.
	tcontrol::set_label(label);

	tlabel* widget = find_label(false);
	if(widget) {
		widget->set_label(label);
	}
}

tlabel* tscroll_label::find_label(const bool must_exist)
{
	if(label_) {
		return label_;
	} else {
		return find_widget<tlabel>("_label", false, must_exist); 
	}
}

const tlabel* tscroll_label::find_label(const bool must_exist) const
{ 
	if(label_) {
		return label_;
	} else {
		return find_widget<const tlabel>("_label", false, must_exist); 
	}
}

tspacer* tscroll_label::find_spacer(const bool must_exist)
{
	assert(label_ || !must_exist);
	return find_widget<tspacer>("_label", false, must_exist); 
}	

const tspacer* tscroll_label::find_spacer(const bool must_exist) const
{
	assert(label_ || !must_exist);
	return find_widget<const tspacer>("_label", false, must_exist); 
}

void tscroll_label::finalize()
{
	tspacer* spacer = new tspacer();
	spacer->set_id("_label");
	spacer->set_definition("default");

	label_ = dynamic_cast<tlabel*>(grid().swap_child("_label", spacer, true));
	assert(label_);

	label_->set_label(label());
	label_->set_can_wrap(true);
}

// REMOVE when wrapping is reimplemented.	
#if 0
bool tscroll_label::content_set_width_constrain(const unsigned width)
{
	bool result = !label_ ? true : label_->set_width_constrain(width);

	DBG_G_L << "tscroll_label " << __func__ << ":"
		<< " no label " << !label_
		<< " result " << result
		<< ".\n";
	return result;
}

void tscroll_label::content_clear_width_constrain()
{
	if(label_) {
		label_->clear_width_constrain();
	}
}

#endif
tpoint tscroll_label::content_calculate_best_size() const
{
	assert(label_);

	log_scope2(gui_layout, std::string("tscroll_label ") + __func__);
	tpoint result = label_->get_best_size();

	DBG_G_L << " result " << result << ".\n";
	return result;
}

void tscroll_label::
	content_use_vertical_scrollbar(const unsigned maximum_height)
{
	int width = content_get_best_size().x;

	set_content_layout_size(tpoint(width, maximum_height));
}

void tscroll_label::content_set_size(const SDL_Rect& rect)
{
	assert(label_);

	tpoint label_best_size = label_->get_best_size();

	// Set the dummy spacer.
	find_spacer()->set_size(tpoint(rect.x, rect.y), tpoint(rect.w, rect.h));

	//maybe add a get best height for a label with a given width...
	label_->set_size(
			tpoint(0, 0),
			tpoint(label_best_size.x, label_best_size.y));

	tscrollbar_* scrollbar = find_scrollbar(false);
	if(scrollbar) {
		scrollbar->set_item_count(label_best_size.y);
		scrollbar->set_visible_items(rect.h);
	}
}

void tscroll_label::draw_content(surface& surf, const bool force,
		const bool invalidate_background)
{
	assert(label_);
	if(label_ == NULL) {
		content_find_grid()->draw(surf, force, invalidate_background);
		return;
	}

	// For now redraw the label every cycle
	surface label_surf(
		create_neutral_surface(label_->get_width(), label_->get_height()));

	label_->draw(label_surf, true, true);

	SDL_Rect src_rect = find_spacer()->get_rect();
	src_rect.x = 0;
	tscrollbar_* scrollbar = find_scrollbar(false);
	src_rect.y = scrollbar ? scrollbar->get_item_position() : 0;

	const SDL_Rect dst_rect = find_spacer()->get_rect();

	blit_surface(label_surf, &src_rect , surf, &dst_rect);
}

twidget* tscroll_label::content_find_widget(
		const tpoint& /*coordinate*/, const bool /*must_be_active*/)
{
	return label_;
}

const twidget* tscroll_label::content_find_widget(const tpoint& /*coordinate*/, 
		const bool /*must_be_active*/) const
{
	return label_;
}

} // namespace gui2

