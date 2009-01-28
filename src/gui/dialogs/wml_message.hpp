/* $Id$ */
/*
   copyright (c) 2008 - 2009 by mark de wever <koraq@xs4all.nl>
   part of the battle for wesnoth project http://www.wesnoth.org/

   this program is free software; you can redistribute it and/or modify
   it under the terms of the gnu general public license version 2
   or at your option any later version.
   this program is distributed in the hope that it will be useful,
   but without any warranty.

   see the copying file for more details.
*/

#ifndef GUI_DIALOGS_WML_MESSAGE_HPP_INCLUDED
#define GUI_DIALOGS_WML_MESSAGE_HPP_INCLUDED

#include "gui/dialogs/message.hpp"

namespace gui2 {

/**
 * Base class for the wml generated messages.
 *
 * We have a separate sub class for left and right images.
 */
class twml_message_ : public tmessage
{
public:
	twml_message_(const std::string& title, const std::string& message,
			const std::string portrait, const bool mirror)
		: tmessage(title, message, true)
		, portrait_(portrait)
		, mirror_(mirror)
		, input_caption_("")
		, input_text_(NULL)
	    , input_maximum_lenght_(0)
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

private:

	/** Filename of the portrait. */
	std::string portrait_;

	/** Mirror the portrait? */
	bool mirror_;

	/** The caption to show for the input text. */
	std::string input_caption_;

	/** The text input. */
	std::string* input_text_;

	/** The maximum length of the input text. */
	unsigned input_maximum_lenght_;

	/** Does the dialog have an input text? */
	bool has_input() { return !input_caption_.empty(); }

	/**
	 * Inherited from tmessage.
	 *
	 * The subclasses need to implement the left or right definition.
	 */
	twindow* build_window(CVideo& /*video*/) = 0;

	/** Inherited from tmessage. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);
};

/** Shows a dialog with the portrait on the left side. */
class twml_message_left : public twml_message_
{
public:
	twml_message_left(const std::string& title, const std::string& message,
			const std::string portrait, const bool mirror)
		: twml_message_(title, message, portrait, mirror)
	{
	}
private:
	/** Inherited from twml_message_. */
	twindow* build_window(CVideo& video);
};

/** Shows a dialog with the portrait on the right side. */
class twml_message_right : public twml_message_
{
public:
	twml_message_right(const std::string& title, const std::string& message,
			const std::string portrait, const bool mirror)
		: twml_message_(title, message, portrait, mirror)
	{
	}
private:
	/** Inherited from twml_message_. */
	twindow* build_window(CVideo& video);
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
 *  @param input_caption          The caption for the optional input text
 *                                box. If this value != "" there is an input
 *                                and the input text parameter is mandatory.
 *  @param input_text             Pointer to the initial text value will be
 *                                set to the result.
 *  @param maximum_length         The maximum length of the text.
 */
int show_wml_message(const bool left_side
		, CVideo& video
		, const std::string& title
		, const std::string& message
		, const std::string& portrait
		, const bool mirror
		, const std::string& input_caption
		, std::string* input_text
	    , const unsigned maximum_length);


} // namespace gui2

#endif

