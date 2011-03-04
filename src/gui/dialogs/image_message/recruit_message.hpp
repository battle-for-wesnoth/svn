/* $Id$ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_RECRUIT_MESSAGE_HPP_INCLUDED
#define GUI_DIALOGS_RECRUIT_MESSAGE_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
//TODO get rid of this here
#include "unit.hpp"

namespace gui2 {

/**
 * Base class for the wml generated messages.
 *
 * We have a separate sub class for left and right images.
 */
class trecruit_message_
	: public tdialog
{
public:
	trecruit_message_(const std::string& title, const std::string& message,
			const std::string& portrait, const bool mirror)
		: title_(title)
		, image_("")
		, message_(message)
		, portrait_(portrait)
		, mirror_(mirror)
		, has_input_(false)
		, input_caption_("")
		, input_text_(NULL)
		, input_maximum_lenght_(0)
		, option_list_()
		, chosen_option_(NULL)
	{
	}

	/**
	 * Sets the input text variables.
	 *
	 * @param caption             The caption for the label.
	 * @param text                The initial text, after showing the final
	 *                            text.
	 * @param maximum_length      The maximum length of the text.
	 */
	void set_input(const std::string& caption,
			std::string* text, const unsigned maximum_length);

	/**
	 * Sets the option list.
	 *
	 * @param option_list			Vector of options to choice from.
	 * @param chosen_option			Number of the option that was chosen.
	 */
	void set_option_list(
			const std::vector<std::string>& option_list, int* chosen_option);

	/**
	 * Sets the unit list.
	 *
	 * @param unit_list          TODO
	 * @param chosen_unit        TODO
	 */
	void set_unit_list(
			const std::vector<unit>& unit_list, std::string* chosen_unit);

	/**
	 * Sets the type list
	 *
	 * @param type_list 		TODO
	 * @param type_id			TODO
	 * @param side_num			TODO
	 * @param gold				TODO
	 */
	void set_type_list(
				std::vector<const unit_type*> type_list, std::string* type_id, int side_num, int gold);
private:


	/** Handler for changed unit selection. */
	void update_unit_list(twindow& window);

	/** Handler for the profile button event. */
	void profile_pressed();

	/** Handler for the help button event */
	void help_pressed();

	/** The title for the dialog. */
	std::string title_;

	/**
	 * The image which is shown in the dialog.
	 *
	 * This image can be an icon or portrait or any other image.
	 */
	std::string image_;

	/** The message to show to the user. */
	std::string message_;
	/** Filename of the portrait. */
	std::string portrait_;

	/** Mirror the portrait? */
	bool mirror_;

	/** Do we need to show an input box? */
	bool has_input_;

	/** The caption to show for the input text. */
	std::string input_caption_;

	/** The text input. */
	std::string* input_text_;

	/** The maximum length of the input text. */
	unsigned input_maximum_lenght_;

	/** The unit id. */
	std::string* unit_id_;

	/** The list of units the player can choose. */
	std::vector<unit> unit_list_;

	/** The list of unit_types the player can choose. */
	//std::vector<const unit_type*> type_list_;

	/** The list of options the player can choose. */
	std::vector<std::string> option_list_;

	/** The chosen option. */
	int *chosen_option_;

	/** The chosen unit. */
	unit *chosen_unit_;

	/** Gold */
	int gold_;

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);
};

/** Shows a dialog with the portrait on the left side. */
class trecruit_message_left : public trecruit_message_
{
public:
	trecruit_message_left(const std::string& title, const std::string& message,
			const std::string& portrait, const bool mirror)
		: trecruit_message_(title, message, portrait, mirror)
	{
	}

private:

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;
};

/** Shows a dialog with the portrait on the right side. */
class trecruit_message_right : public trecruit_message_
{
public:
	trecruit_message_right(const std::string& title, const std::string& message,
			const std::string& portrait, const bool mirror)
		: trecruit_message_(title, message, portrait, mirror)
	{
	}

private:

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;
};


/**
 *  Helper function to show a portrait.
 *
 *  @param left_side              If true the portrait is shown on the left,
 *                                on the right side otherwise.
 *  @param video                  The display variable.
 *  @param title                  The title of the dialog.
 *  @param message                The message to show.
 *  @param portrait               Filename of the portrait.
 *  @param mirror                 Does the portrait need to be mirrored?
 *
 *  @param has_input              Do we need to show the input box.
 *  @param input_caption          The caption for the optional input text
 *                                box. If this value != "" there is an input
 *                                and the input text parameter is mandatory.
 *  @param input_text             Pointer to the initial text value will be
 *                                set to the result.
 *  @param maximum_length         The maximum length of the text.
 *
 *  @param option_list            A list of options to select in the dialog.
 *  @param chosen_option          Pointer to the initially chosen option.
 *                                Will be set to the chosen_option when the
 *                                dialog closes.
 */
int show_recruit_message(const bool left_side
		, CVideo& video
	    , std::vector<const unit_type*> type_list
	    , std::string* type_id
	    , int side_num
	    , int gold);

//int show_wml_message(const bool left_side
//		, CVideo& video
//		, const std::string& title
//		, const std::string& message
//		, const std::string& portrait
//		, const bool mirror
//		, const bool has_input
//		, const std::string& input_caption
//		, std::string* input_text
//	    , const unsigned maximum_length
//	    , const bool has_unit
//	    , std::string* unit_id
//	    , const std::vector<unit>& unit_list
//		, const std::vector<std::string>& option_list
//		, int* chosen_option);


} // namespace gui2

#endif

