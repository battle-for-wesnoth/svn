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

#ifndef __GUI_WIDGETS_TEXT_BOX_HPP_INCLUDED__
#define __GUI_WIDGETS_TEXT_BOX_HPP_INCLUDED__

#include "gui/widgets/text.hpp"


namespace gui2 {

//! Class for text input history
class ttext_history
{
public:
	//! Gets history that matches id and enables or disables it.
	static ttext_history get_history(const std::string& id, const bool enabled);

	// Defaults to a disabled history
	ttext_history() : 
		history_(0), 
		pos_(0), 
		enabled_(false) 
	{}
	
	//! Push string into the history if it is non-empty and is not the same as the last item
	//! updates position to end of history.
	void push(const std::string& text);
	
	//! One step up/down in history. Pushes text to the history if at the end.
	std::string up(const std::string& text = "");
	std::string down(const std::string& text = "");
	
	//! Returns the value at the current history position. Returns "" if not enabled
	//! or if at the end of the history.
	std::string get_value() const;
	
	void set_enabled(bool enabled = true) { enabled_ = enabled; }
	bool get_enabled() const { return enabled_; }
	
private:
	ttext_history(std::vector<std::string>* history, const bool enabled) : 
		history_(history), 
		pos_(history->size()), 
		enabled_(enabled) 
	{}
	
	std::vector<std::string>* history_;
	unsigned pos_;
	
	bool enabled_;
};

//! Class for a single line text area.
class ttext_box : public ttext_
{

public:

	ttext_box() :
		ttext_(),
		character_offset_(),
		history_()
	{}

	void set_history(const std::string& id) 
		{ history_ = ttext_history::get_history(id, true); }
	
	void save_to_history() { history_.push(text()); }
	void save_to_history(const std::string& text) { history_.push(text); }

protected:
	
	// Gets the character position at the wanted place
	// as if the mouse would click there.
//	virtual unsigned get_position_at(tpoint& coordinate);

//	void delete_selection();
//	void copy();
//	void paste();
//	
	//! Inherited from ttext_.
	void insert_char(Uint16 unicode);

	//! Inherited from ttext_.
	void delete_char(const bool before_cursor);

	//! Inherited from ttext_.
	void delete_selection();

	//! Inherited from tcontrol.
	void set_canvas_text();

	void goto_end_of_line(const bool select = false) { goto_end_of_data(select); }
	void goto_start_of_line(const bool select = false) { goto_start_of_data(select); }

private:

	void handle_key_up_arrow(SDLMod modifier, bool& handled);
	void handle_key_down_arrow(SDLMod modifier, bool& handled);

	// Clears the current line
	void handle_key_clear_line(SDLMod modifier, bool& handled);

	//! Contains the end offset of each character in the text area.
	std::vector<unsigned> character_offset_;

	//! Inherited from ttext_.
	void calculate_char_offset();

	ttext_history history_;

	//! Inherited from tcontrol.
	const std::string& get_control_type() const 
		{ static const std::string type = "text_box"; return type; }
};



} //namespace gui2

#endif

