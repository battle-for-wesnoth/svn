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

#include "gui/widgets/debug.hpp"

#include "foreach.hpp"
#include "formatter.hpp"
#include "gui/widgets/vertical_scrollbar_container.hpp"
#include "gui/widgets/window.hpp"
#include "serialization/string_utils.hpp"

#include <fstream>

namespace gui2 {

namespace {

/**
 * Gets the id of a grid child cell.
 *
 * @param parent_id               The id of the parent grid.
 * @param row                     Row number in the grid.
 * @param col                     Column number in the grid.
 *
 * @returns                       The id of the child cell.
 */
std::string get_child_id(
		const std::string& parent_id, const unsigned row, const unsigned col)
{
	return (formatter() << parent_id << "_C_" << row << '_' << col).c_str();
}

/**
 * Gets the id of a widget in a grid child cell.
 *
 * @param parent_id               The id of the parent grid.
 * @param row                     Row number in the grid.
 * @param col                     Column number in the grid.
 *
 * @returns                       The id of the widget.
 */
std::string get_child_widget_id(
		const std::string& parent_id, const unsigned row, const unsigned col)
{
	return get_child_id(parent_id, row, col) + "_W";
}

/** Gets the prefix of the filename. */
std::string get_base_filename()
{
	char buf[17] = {0};
	time_t t = time(NULL);
	tm* lt = localtime(&t);
	if(lt) {
		strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", lt);
	}
	static unsigned counter = 0;
	++counter;

	static const std::string 
		result((formatter() << buf << '_' << counter << '_').c_str());

	return result;
}

	/***** ***** ***** ***** FLAGS ***** ***** ***** *****/

	const unsigned ALL = UINT_MAX;       /**< All levels/domains */

	// level flags
	const unsigned CHILD      = 1 << 0; /**< 
										  * Shows the child records of a cell.
										  */
	const unsigned SIZE_INFO  = 1 << 1; /**< 
										  * Shows the size info of 
										  * children/widgets.
										  */
	const unsigned STATE_INFO = 1 << 2; /**< Shows the state info of widgets. */

	// domain flags
	const unsigned SHOW        = 1 << 0; /**< 
										   * Shows the info when the dialog is
										   * shown.
										   */
	const unsigned LAYOUT      = 1 << 1; /**< 
										   * Shows the info in all layout
										   * phases.
										   */

	unsigned level_ = ALL; /** @todo Should default to 0. */
	unsigned domain_ = ALL; /** @todo Should default to 0. */
} //namespace

tdebug_layout_graph::tdebug_layout_graph(const twindow* window)
	: window_(window)
	, sequence_number_(0)
	, filename_base_(get_base_filename())
{
}

void tdebug_layout_graph::set_level(const std::string& level)
{
	if(level.empty()) {
		level_ = ALL; /** @todo Should default to 0. */
		return;
	}

	std::vector<std::string> params = utils::split(level);

	foreach(const std::string& param, params) {
		if(param == "all") {
			level_ = ALL;
			// No need to look further eventhought invalid items are now
			// ignored.
			return; 
		} else if(param == "child") {
			level_ |= CHILD;
		} else if(param == "size") {
			level_ |= SIZE_INFO;
		} else if(param == "state") {
			level_ |= STATE_INFO;
		} else {
			// loging might not be up yet.
			std::cerr << "Unknown level '" << param << "' is ignored.\n";
		}
	}
}

void tdebug_layout_graph::set_domain(const std::string& domain)
{
	if(domain.empty()) {
		// return error and die
		domain_ = ALL; /** @todo Should default to 0. */
		return;
	}

	std::vector<std::string> params = utils::split(domain);

	foreach(const std::string& param, params) {
		if(param == "all") {
			domain_ = ALL;
			// No need to look further eventhought invalid items are now
			// ignored.
			return; 
		} else if(param == "show") {
			domain_ |= SHOW;
		} else if(param == "layout") {
			domain_ |= LAYOUT;
		} else {
			// loging might not be up yet.
			std::cerr << "Unknown domain '" << param << "' is ignored.\n";
		}
	}
}

void tdebug_layout_graph::generate_dot_file(const std::string& generator)
{
	const std::string filename = filename_base_ + 
		lexical_cast<std::string>(++sequence_number_) + "-" + generator + ".dot";
	std::ofstream file(filename.c_str());

	file << "//Basic layout graph for window id '" << window_->id() 
		<< "' using definition '" <<  window_->definition() << "'.\n"
		<< "digraph window {\n"
		<< "\tnode [shape=record, style=filled, fillcolor=\"bisque\"];\n"
		<< "\trankdir=LR;\n"
		;

	widget_generate_info(file, window_, "root");

	file << "}\n";
}

void tdebug_layout_graph::widget_generate_info(std::ostream& out, 
		const twidget* widget, const std::string& id, const bool embedded) const
{

	out << "\t" << id 
		<< " [label=<<table border=\"0\" cellborder=\"1\" cellspacing=\"0\">";

	widget_generate_basic_info(out, widget);
	if(level_ & STATE_INFO) widget_generate_state_info(out, widget);
	if(level_ & SIZE_INFO) widget_generate_size_info(out, widget);

	out << "</table>>";
	if(embedded) {
		out << ", fillcolor=\"palegoldenrod\"";
	}	
	out << "];\n";

	const tgrid* grid = dynamic_cast<const tgrid*>(widget);
	if(!grid) {
		const tcontainer_* container = dynamic_cast<const tcontainer_*>(widget);

		if(container && level_ & CHILD) { // The extra constrain needed???

			widget_generate_info(out, &container->grid(), id + "_G", true);
			out << "\t" << id << " -> " 
				<< id << "_G"
				<< " [label=\"(grid)\"];\n";
		}
	}
	if(grid) {
		grid_generate_info(out, grid, id);
	}
}

void tdebug_layout_graph::widget_generate_basic_info(
		std::ostream& out, const twidget* widget) const
{
	std::string header_background = level_ & (SIZE_INFO|STATE_INFO) 
	   ? " bgcolor=\"gray\"" : "";	
	const tcontrol* control = dynamic_cast<const tcontrol*>(widget);

	out << "<tr><td" << header_background << ">" << '\n'
		<< "type=" << get_type(widget) << '\n'
		<< "</td></tr>" << '\n'
		<< "<tr><td" << header_background << ">" << '\n'
		<< "id=" << widget->id() << '\n'
		<< "</td></tr>" << '\n'
		<< "<tr><td" << header_background << ">" << '\n'
		<< "definition=" << widget->definition() << '\n'
		<< "</td></tr>" << '\n';
		if(control) {
			std::string label = control->label();
			if(label.size() > 50) {
				label = label.substr(0, 50) + "...";
			}
			out << "<tr><td" << header_background << ">" << '\n'
				<< "label=" << label << '\n'
				<< "</td></tr>\n";
		}
}

void tdebug_layout_graph::widget_generate_state_info(
		std::ostream& out, const twidget* widget) const
{
	const tcontrol* control = dynamic_cast<const tcontrol*>(widget);
	if(!control) {
		return;
	}

	out << "<tr><td>\n"
		<< "tooltip=" << control->tooltip() << '\n'
		<< "</td></tr>\n"
		<< "<tr><td>\n"
		<< "help message" << control->help_message() << '\n'
		// FIXME add value and other specific items
		<< "</td></tr>\n"
		<< "<tr><td>\n"
		<< "active=" << control->get_active() << '\n'
		<< "</td></tr>\n"
		<< "<tr><td>\n"
		<< "visible=" << control->get_visible() << '\n'
		<< "</td></tr>\n"
		<< "<tr><td>\n"
		<< "use tooltip on label overflow="
			<< control->get_use_tooltip_on_label_overflow() << '\n'
		<< "</td></tr>\n"
		<< "<tr><td>\n"
		<< "does block easy close=" << control->does_block_easy_close() << '\n'
		<< "</td></tr>\n";

	const tvertical_scrollbar_container_* vertical_scrollbar_container = 
		dynamic_cast<const tvertical_scrollbar_container_*>(widget);

	if(vertical_scrollbar_container) {

		out << "<tr><td>\n"
			<< "scrollbar_mode_=" 
				<< vertical_scrollbar_container->scrollbar_mode_ << '\n'
			<< "</td></tr>\n";
	}

}

void tdebug_layout_graph::widget_generate_size_info(
		std::ostream& out, const twidget* widget) const
{
	out << "<tr><td>\n"
		<< "can wrap=" << widget->can_wrap() << '\n'
		<< "</td></tr>\n"
		<< "<tr><td>\n"
		<< "has vertical scrollbar=" 
			<< widget->has_vertical_scrollbar() << '\n'
		<< "</td></tr>\n"
		<< "<tr><td>\n"
		<< "has horizontal scrollbar=" 
			<< widget->has_horizontal_scrollbar() << '\n'
		<< "</td></tr>\n"
		<< "<tr><td>\n"
		<< "size=" << widget->get_rect() << '\n'
		<< "</td></tr>\n"
		<< "<tr><td>\n"
		<< "last_best_size_=" << widget->last_best_size_ << '\n'
		<< "</td></tr>\n"
		<< "<tr><td>\n"
		<< "layout_size_=" << widget->layout_size_ << '\n'
		<< "</td></tr>\n";


	const tcontrol* control = dynamic_cast<const tcontrol*>(widget);

	if(control) {
		out << "<tr><td>\n"
			<< "minimum config size=" << control->get_config_minimum_size() << '\n'
			<< "</td></tr>\n"
			<< "<tr><td>\n"
			<< "default config size=" << control->get_config_default_size() << '\n'
			<< "</td></tr>\n"
			<< "<tr><td>\n"
			<< "maximum config size=" << control->get_config_maximum_size() << '\n'
			<< "</td></tr>\n"
			<< "<tr><td>\n"
			<< "shrunken_=" << control->shrunken_ << '\n'
			<< "</td></tr>\n";
	}

	const tcontainer_* container = dynamic_cast<const tcontainer_*>(widget);

	if(container) {
		out << "<tr><td>\n"
			<< "border_space=" << container->border_space() << '\n'
			<< "</td></tr>\n";
	}

	const tvertical_scrollbar_container_* vertical_scrollbar_container = 
		dynamic_cast<const tvertical_scrollbar_container_*>(widget);

	if(vertical_scrollbar_container) {

		out << "<tr><td>\n"
			<< "content_last_best_size_=" 
				<< vertical_scrollbar_container->content_last_best_size_ << '\n'
			<< "</td></tr>\n"
			<< "<tr><td>\n"
			<< "content_layout_size_=" 
				<< vertical_scrollbar_container->content_layout_size_ << '\n'
			<< "</td></tr>\n";
	}
}

void tdebug_layout_graph::grid_generate_info(std::ostream& out, 
		const tgrid* grid, const std::string& parent_id) const
{
	const bool show_child = level_ & CHILD; 

	// maybe change the order to links, child, widgets so the output of the 
	// dot file might look better.

	out << "\n\n\t// The children of " << parent_id << ".\n";

	for(unsigned row = 0; row < grid->get_rows(); ++row) {
		for(unsigned col = 0; col < grid->get_cols(); ++col) {

			const twidget* widget = grid->child(row, col).widget();
			assert(widget);

			widget_generate_info(
					out, widget, get_child_widget_id(parent_id, row, col));
		}
	}

	if(show_child) {
		out << "\n\t// The grid child data of " << parent_id << ".\n";

		for(unsigned row = 0; row < grid->get_rows(); ++row) {
			for(unsigned col = 0; col < grid->get_cols(); ++col) {

				child_generate_info(out, grid->child(row, col), 
					get_child_id(parent_id, row, col));
			}
		}

	}

	out << "\n\t// The links of " << parent_id << ".\n";

	for(unsigned row = 0; row < grid->get_rows(); ++row) {
		for(unsigned col = 0; col < grid->get_cols(); ++col) {

			if(show_child) {

				// grid -> child
				out << "\t" << parent_id << " -> " 
					<< get_child_id(parent_id, row, col)  
					<< " [label=\"(" << row << ',' << col 
					<< ")\"];\n";

				// child -> widget
				out << "\t" << get_child_id(parent_id, row, col) << " -> " 
					<< get_child_widget_id(parent_id, row, col) << ";\n";

			} else {

				// grid -> widget
				out << "\t" << parent_id << " -> " 
					<< get_child_widget_id(parent_id, row, col)
					<< " [label=\"(" << row << ',' << col 
					<< ")\"];\n";
			}
		}
	}
}

void tdebug_layout_graph::child_generate_info(std::ostream& out, 
		const tgrid::tchild& child, const std::string& id) const
{

	unsigned flags = child.get_flags();

	out << "\t" << id 
		<< " [style=\"\", label=<<table border=\"0\" cellborder=\"1\" cellspacing=\"0\">";
	out << "<tr><td>"
		<< "vertical flag=";

	switch(flags & tgrid::VERTICAL_MASK) {
		case tgrid::VERTICAL_GROW_SEND_TO_CLIENT : out << "send to client"; break;
		case tgrid::VERTICAL_ALIGN_TOP           : out << "align to top"; break;
		case tgrid::VERTICAL_ALIGN_CENTER        : out << "center"; break;
		case tgrid::VERTICAL_ALIGN_BOTTOM        : out << "align to bottom"; break;
		default                                  : 
			out << "unknown value(" 
				<< ((flags & tgrid::VERTICAL_MASK) >> tgrid::VERTICAL_SHIFT)
				<< ")";
	}

	out	<< "</td></tr>"
		<< "<tr><td>"
		<< "horizontal flag=";

	switch(flags & tgrid::HORIZONTAL_MASK) {
		case tgrid::HORIZONTAL_GROW_SEND_TO_CLIENT : out << "send to client"; break;
		case tgrid::HORIZONTAL_ALIGN_LEFT          : out << "align to left"; break;
		case tgrid::HORIZONTAL_ALIGN_CENTER        : out << "center"; break;
		case tgrid::HORIZONTAL_ALIGN_RIGHT         : out << "align to right"; break;
		default                                    : 
			out << "unknown value(" 
				<< ((flags & tgrid::HORIZONTAL_MASK) >> tgrid::HORIZONTAL_SHIFT)
				<< ")";
	}

	out	<< "</td></tr>"
		<< "<tr><td>"
		<< "border location=";

	if((flags & tgrid::BORDER_ALL) == 0) {
		out << "none";
	} else if((flags & tgrid::BORDER_ALL) == tgrid::BORDER_ALL) {
		out << "all";
	} else {
		std::string result;
		if(flags & tgrid::BORDER_TOP)    result += "top, ";
		if(flags & tgrid::BORDER_BOTTOM) result += "bottom, ";
		if(flags & tgrid::BORDER_LEFT)   result += "left, ";
		if(flags & tgrid::BORDER_RIGHT)  result += "right, ";

		if(!result.empty()) {
			result.resize(result.size() - 2);
		}

		out << result;
	}

	out	<< "</td></tr>"
		<< "<tr><td>"
		<< "border_size="<< child.get_border_size()
		<< "</td></tr>";

	out << "</table>>];\n";
}

std::string tdebug_layout_graph::get_type(const twidget* widget) const
{
	const tcontrol* control = dynamic_cast<const tcontrol*>(widget);
	if(control) {
		return control->get_control_type();
	} else {
		const tgrid* grid = dynamic_cast<const tgrid*>(widget);
		if(grid) {
			return "grid";
		} else {
			return "unknown";
		}
	}
}

} // namespace gui2

