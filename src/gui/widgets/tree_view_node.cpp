/* $Id$ */
/*
   Copyright (C) 2010 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/widgets/tree_view_node.hpp"

#include "gui/auxiliary/log.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/tree_view.hpp"
#include "gui/widgets/window.hpp"

#include <boost/bind.hpp>

#define LOG_SCOPE_HEADER \
		get_control_type() + " [" + parent_widget_->id() + "] " + __func__
#define LOG_HEADER LOG_SCOPE_HEADER + ':'

namespace gui2 {

ttree_view_node::ttree_view_node(const std::string& id
		, const std::vector<tnode_definition>& node_definitions
		, ttree_view_node* parent
		, ttree_view* parent_widget
		, const std::map<std::string /* widget id */, string_map>& data
		)
	: parent_(parent)
	, parent_widget_(parent_widget) // need to store? also used in set_parent
	, grid_()
	, children_()
	, node_definitions_(node_definitions)
	, icon_(NULL)
	, label_(NULL)
{
	set_parent(parent_widget);
	grid_.set_parent(this);
	if(id != "root") {
		foreach(const tnode_definition& node_definition, node_definitions_) {
			if(node_definition.id == id) {
				node_definition.builder->build(&grid_);
				init_grid(&grid_, data);

				icon_ = find_widget<ttoggle_button>(
						  &grid_
						, "tree_view_node_icon"
						, false
						, false);

				if(icon_) {
					icon_->set_visible(twidget::HIDDEN);
					icon_->connect_signal<event::LEFT_BUTTON_CLICK>(
							boost::bind(&ttree_view_node::
								signal_handler_left_button_click
								, this, _2));

				}

				if(parent_ && parent_->icon_) {
					parent_->icon_->set_visible(twidget::VISIBLE);
				}

				twidget& widget = find_widget<twidget>(
						  &grid_
						, "tree_view_node_label"
						, false);

				label_ = dynamic_cast<tselectable_*>(&widget);
				if(label_) {
					widget.connect_signal<event::LEFT_BUTTON_CLICK>(
							  boost::bind(&ttree_view_node::
								signal_handler_label_left_button_click
								, this, _2, _3, _4)
							, event::tdispatcher::front_child);
					widget.connect_signal<event::LEFT_BUTTON_CLICK>(
							  boost::bind(&ttree_view_node::
								signal_handler_label_left_button_click
								, this, _2, _3, _4)
							, event::tdispatcher::front_pre_child);

					if(!parent_widget_->selected_item_) {
						parent_widget_->selected_item_ = this;
						label_->set_value(true);
					}
				}

				return;
			}
		}

		/** @todo enable after 1.9. */
#if 0
		// FIXME add node id
//		VALIDATE(false, _("Unknown builder id for tree view node."));
#else
		assert(false);
#endif

	}
}

ttree_view_node& ttree_view_node::add_child(
		  const std::string& id
		, const std::map<std::string /* widget id */, string_map>& data
		, const int index)
{
	assert(parent_widget_ && parent_widget_->content_grid());

	boost::ptr_vector<ttree_view_node>::iterator itor = children_.end();

	if(static_cast<size_t>(index) < children_.size()) {
		itor = children_.begin() + index;
	}

	itor = children_.insert(itor, new ttree_view_node(
			  id
			, node_definitions_
			, this
			, parent_widget_
			, data));

	if(is_folded() || is_root_node()) {
		return *itor;
	}

	if(parent_widget_->get_size() == tpoint(0, 0)) {
		return *itor;
	}

	const int current_width = parent_widget_->content_grid()->get_width();

	// Calculate width modification.
	tpoint best_size = itor->get_best_size();
	best_size.x += get_indention_level() * parent_widget_->indention_step_size_;
	const unsigned width_modification = best_size.x > current_width
			? current_width - best_size.x
			: 0;

	// Calculate height modification.
	const int height_modification = best_size.y;
	assert(height_modification > 0);

	// Request new size.
	parent_widget_->resize_content(width_modification, height_modification);

	return *itor;
}

unsigned ttree_view_node::get_indention_level() const
{
	unsigned level = 0;

	const ttree_view_node* node = this;
	while(node->is_root_node()) {
		node = &node->parent();
		++level;
	}

	return level;
}

ttree_view_node& ttree_view_node::parent()
{
	assert(!is_root_node());

	return *parent_;
}

const ttree_view_node& ttree_view_node::parent() const
{
	assert(!is_root_node());

	return *parent_;
}

bool ttree_view_node::is_folded() const
{
	return icon_ && icon_->get_value();
}
#if 0
void ttree_view_node::fold(const bool /*recursive*/)
{
	// FIXME set state

}

void ttree_view_node::unfold(const texpand_mode /*mode*/)
{
	// FIXME set state

}
#endif

void ttree_view_node::clear()
{
	/** @todo Also try to find the optimal width. */
	unsigned height_reduction = 0;

	if(!is_folded()) {
		foreach(const ttree_view_node& node, children_) {
			height_reduction += node.get_current_size().y;
		}
	}

	children_.clear();

	if(height_reduction == 0) {
		return;
	}

	parent_widget_->resize_content(0, -height_reduction);
}

struct ttree_view_node_implementation
{
	template<class W>
	static W* find_at(
			  typename tconst_duplicator<W, ttree_view_node>::type&
				tree_view_node
			, const tpoint& coordinate, const bool must_be_active)
	{
		if(W* widget =
				tree_view_node.grid_.find_at(coordinate, must_be_active)) {

			return widget;
		}

		if(tree_view_node.is_folded()) {
			return NULL;
		}

		typedef typename tconst_duplicator<W, ttree_view_node>::type thack;
		foreach(thack& node, tree_view_node.children_) {
			if(W* widget = node.find_at(coordinate, must_be_active)) {
				return widget;
			}
		}

		return NULL;
	}
};

twidget* ttree_view_node::find_at(
		  const tpoint& coordinate
		, const bool must_be_active)
{
	return ttree_view_node_implementation::find_at<twidget>(
			*this, coordinate, must_be_active);
}

const twidget* ttree_view_node::find_at(
		  const tpoint& coordinate
		, const bool must_be_active) const
{
	return ttree_view_node_implementation::find_at<const twidget>(
			*this, coordinate, must_be_active);
}

void ttree_view_node::impl_populate_dirty_list(twindow& caller
		, const std::vector<twidget*>& call_stack)
{
	std::vector<twidget*> child_call_stack = call_stack;
	grid_.populate_dirty_list(caller, child_call_stack);

	if(is_folded()) {
		return;
	}

	foreach(ttree_view_node& node, children_) {
		std::vector<twidget*> child_call_stack = call_stack;
		node.impl_populate_dirty_list(caller, child_call_stack);
	}
}

tpoint ttree_view_node::calculate_best_size() const
{
	assert(parent_widget_);
	return calculate_best_size(-1, parent_widget_->indention_step_size_);
}

tpoint ttree_view_node::get_current_size() const
{
	if(parent_ && parent_->is_folded()) {
		return tpoint(0, 0);
	}

	tpoint size = grid_.get_size();
	if(is_folded()) {
		return size;
	}

	foreach(const ttree_view_node& node, children_) {

		if(node.grid_.get_visible() == twidget::INVISIBLE) {
			continue;
		}

		const tpoint node_size = node.get_unfolded_size();

		size.y += node_size.y;
		size.x = std::max(size.x, node_size.x);
	}

	return size;
}

tpoint ttree_view_node::get_unfolded_size() const
{
	tpoint size = grid_.get_best_size();
	size.x += get_indention_level() * parent_widget_->indention_step_size_;

	foreach(const ttree_view_node& node, children_) {

		if(node.grid_.get_visible() == twidget::INVISIBLE) {
			continue;
		}

		const tpoint node_size = node.get_unfolded_size();

		size.y += node_size.y;
		size.x = std::max(size.x, node_size.x);
	}

	return size;
}

tpoint ttree_view_node::calculate_best_size(const int indention_level
		, const unsigned indention_step_size) const
{
	log_scope2(log_gui_layout, LOG_SCOPE_HEADER);

	tpoint best_size = grid_.get_best_size();
	if(indention_level > 0) {
		best_size.x += indention_level * indention_step_size;
	}

	if(is_folded()) {

		DBG_GUI_L << LOG_HEADER
				<< " Folded grid return own best size " << best_size << ".\n";
		return best_size;
	}

	DBG_GUI_L << LOG_HEADER << " own grid best size " << best_size << ".\n";

	foreach(const ttree_view_node& node, children_) {

		if(node.grid_.get_visible() == twidget::INVISIBLE) {
			continue;
		}

		const tpoint node_size = node.calculate_best_size(indention_level + 1,
				indention_step_size);

		best_size.y += node_size.y;
		best_size.x = std::max(best_size.x, node_size.x);
	}

	DBG_GUI_L << LOG_HEADER << " result " << best_size << ".\n";
	return best_size;
}

void ttree_view_node::set_origin(const tpoint& origin)
{
	// Inherited.
	twidget::set_origin(origin);

	assert(parent_widget_);
	// Using layout_children seems to fail.
	place(parent_widget_->indention_step_size_, origin, get_size().x);
}

void ttree_view_node::place(const tpoint& origin, const tpoint& size)
{
	// Inherited.
	twidget::place(origin, size);

	assert(parent_widget_);
	parent_widget_->layout_children(true);
}

unsigned ttree_view_node::place(
	  const unsigned indention_step_size
	, tpoint origin
	, unsigned width)
{
	log_scope2(log_gui_layout, LOG_SCOPE_HEADER);
	DBG_GUI_L << LOG_HEADER << " origin " << origin << ".\n";

	const unsigned offset = origin.y;
	tpoint best_size = grid_.get_best_size();
	best_size.x = width;
	grid_.place(origin, best_size);

	if(!is_root_node()) {
		origin.x += indention_step_size;
		width -= indention_step_size;
	}
	origin.y += best_size.y;

	if(is_folded()) {
		DBG_GUI_L << LOG_HEADER << " folded node done.\n";
		return origin.y - offset;
	}

	DBG_GUI_L << LOG_HEADER << " set children.\n";
	foreach(ttree_view_node& node, children_) {
		origin.y += node.place(indention_step_size, origin, width);
	}

	// Inherited.
	twidget::set_size(tpoint(width, origin.y - offset));

	DBG_GUI_L << LOG_HEADER << " result " << ( origin.y - offset) << ".\n";
	return origin.y - offset;
}

void ttree_view_node::set_visible_area(const SDL_Rect& area)
{
	log_scope2(log_gui_layout, LOG_SCOPE_HEADER);
	DBG_GUI_L << LOG_HEADER << " area " << area << ".\n";
	grid_.set_visible_area(area);

	if(is_folded()) {
		DBG_GUI_L << LOG_HEADER << " folded node done.\n";
		return;
	}

	foreach(ttree_view_node& node, children_) {
		node.set_visible_area(area);
	}
}

void ttree_view_node::impl_draw_children(surface& frame_buffer)
{
	grid_.draw_children(frame_buffer);

	if(is_folded()) {
		return;
	}

	foreach(ttree_view_node& node, children_) {
		node.impl_draw_children(frame_buffer);
	}
}

void ttree_view_node::signal_handler_left_button_click(
		const event::tevent event)
{
	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";

	assert(parent_widget_);

	// is_folded() returns the new state.
	if(is_folded()) {

		const tpoint current_size = get_folded_size();
		const tpoint new_size = get_unfolded_size();

		parent_widget_->resize_content(
				  current_size.x - new_size.x
				, current_size.y - new_size.y);
	} else {

		const tpoint current_size = get_unfolded_size();
		const tpoint new_size = get_folded_size();

		parent_widget_->resize_content(
				  current_size.x - new_size.x
				, current_size.y - new_size.y);
	}
}

void ttree_view_node::signal_handler_label_left_button_click(
		  const event::tevent event
		, bool& handled
		, bool& halt)
{
	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";

	assert(label_);

	// We only snoop on the event so normally don't touch the handled, else if
	// we snoop in preexcept when halting.

	if(label_->get_value()) {
		// Forbid deselecting
		halt = handled = true;
	} else {
		// Deselect current item
		assert(parent_widget_);
		if(parent_widget_->selected_item_ && parent_widget_->selected_item_->label_) {
			parent_widget_->selected_item_->label_->set_value(false);
		}

		parent_widget_->selected_item_ = this;

		if(parent_widget_->selection_change_callback_) {
			parent_widget_->selection_change_callback_();
		}
	}
}

void ttree_view_node::init_grid(tgrid* grid
		, const std::map<std::string /* widget id */, string_map>& data)
{
	assert(grid);

	for(unsigned row = 0; row < grid->get_rows(); ++row) {
		for(unsigned col = 0; col < grid->get_cols(); ++col) {
			twidget* widget = grid->widget(row, col);
			assert(widget);

			tgrid* child_grid = dynamic_cast<tgrid*>(widget);
//			ttoggle_button* btn = dynamic_cast<ttoggle_button*>(widget);
			ttoggle_panel* panel = dynamic_cast<ttoggle_panel*>(widget);
			tcontrol* ctrl = dynamic_cast<tcontrol*>(widget);

			if(panel) {
				panel->set_child_members(data);
			} else if(child_grid) {
				init_grid(child_grid, data);
			} else if(ctrl) {
				std::map<std::string, string_map>::const_iterator itor =
						data.find(ctrl->id());

				if(itor == data.end()) {
					itor = data.find("");
				}
				if(itor != data.end()) {
					ctrl->set_members(itor->second);
				}
//				ctrl->set_members(data);
			} else {

//				ERROR_LOG("Widget type '"
//						<< typeid(*widget).name() << "'.");
			}
		}
	}

}

const std::string& ttree_view_node::get_control_type() const
{
	static const std::string type = "tree_view_node";
	return type;
}

} // namespace gui2

