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

#ifndef GUI_WIDGETS_CONTROL_HPP_INCLUDED
#define GUI_WIDGETS_CONTROL_HPP_INCLUDED

#include "gui/widgets/canvas.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/widget.hpp"
#include "../../text.hpp"
#include "tstring.hpp"

#include <cassert>

namespace gui2 {

/** Base class for all visible items. */
class tcontrol : public virtual twidget
{
public:

	tcontrol(const unsigned canvas_count);

	virtual ~tcontrol() {}

	/**
	 * Sets the members of the control.
	 *
	 * The map contains named members it can set, controls inheriting from us
	 * can add additional members to set by this function. The following
	 * members can by the following key:
	 *  * label_                  label
	 *  * tooltip_                tooltip
	 *  * help_message_           help
	 *
	 *
	 * @param data                Map with the key value pairs to set the members.
	 */
	virtual void set_members(const std::map<
		std::string /* member id */, t_string /* member value */>& data);

	/***** ***** ***** ***** State handling ***** ***** ***** *****/

	/**
	 * Sets the control's state.
	 *
	 *  Sets the control in the active state, when inactive a control can't be
	 *  used and doesn't react to events. (Note read-only for a ttext_ is a
	 *  different state.)
	 */
	virtual void set_active(const bool active) = 0;

	/** Gets the active state of the control. */
	virtual bool get_active() const = 0;

protected:
	/** Returns the id of the state.
	 *
	 * The current state is also the index canvas_.
	 */
	virtual unsigned get_state() const = 0;

public:

	/***** ***** ***** ***** Inherited ***** ***** ***** *****/

	/** Inherted from tevent_executor. */
	void mouse_hover(tevent_handler& event);

	/** Inherted from tevent_executor. */
	void help_key(tevent_handler& event);

	/**
	 * Inherited from twidget.
	 *
	 * This function shouldn't be called directly it's called by set_definition().
	 * All classes which use this class as base should call this function in
	 * their constructor. Abstract classes shouldn't call this routine. The 
	 *
	 * classes which call this routine should also define get_control_type().
	 */
	void load_config();

	/** Inherited from twidget. */
	tpoint get_minimum_size() const;

	/** Inherited from twidget. */
	tpoint get_best_size() const;

	/** Import overloaded versions. */
	using twidget::get_best_size;

	/** Inherited from twidget. */
	tpoint get_maximum_size() const;

	/** Inherited from twidget. */
	void draw(surface& surface,  const bool force = false,
	        const bool invalidate_background = false);

	/** Inherited from twidget. */
	twidget* find_widget(const tpoint& coordinate, const bool must_be_active) 
	{
		return (twidget::find_widget(coordinate, must_be_active) 
			&& (!must_be_active || get_active())) ? this : 0;
	}

	/** Inherited from twidget. */
	const twidget* find_widget(const tpoint& coordinate, 
			const bool must_be_active) const
	{
		return (twidget::find_widget(coordinate, must_be_active) 
			&& (!must_be_active || get_active())) ? this : 0;
	}

	/** Inherited from twidget.*/
	twidget* find_widget(const std::string& id, const bool must_be_active)
	{
		return (twidget::find_widget(id, must_be_active) 
			&& (!must_be_active || get_active())) ? this : 0;
	}

	/** Inherited from twidget.*/
	const twidget* find_widget(const std::string& id, 
			const bool must_be_active) const
	{
		return (twidget::find_widget(id, must_be_active) 
			&& (!must_be_active || get_active())) ? this : 0;
	}

	/** 
	 * Inherited from twidget. 
	 * 
	 * This function sets the defintion of a control and should be called soon
	 * after creating the object since a lot of internal functions depend on the
	 * definition.
	 *
	 * This function should be called one time only!!!
	 */
	void set_definition(const std::string& definition);

	/** Inherited from twidget. */
	void set_size(const SDL_Rect& rect);

	/***** ***** ***** setters / getters for members ***** ****** *****/

	bool get_visible() const { return visible_; }
	void set_visible(const bool visible = true) 
		{ if(visible_ != visible) { visible_ = visible; set_dirty();} }

	bool get_multiline_label() const { return multiline_label_; }
	void set_multiline_label(const bool multiline = true) 
		{ if(multiline != multiline_label_) { multiline_label_ = multiline; set_dirty(); } }

	bool get_use_tooltip_on_label_overflow() const { return use_tooltip_on_label_overflow_; }
	void set_use_tooltip_on_label_overflow(const bool use_tooltip = true) 
		{ use_tooltip_on_label_overflow_ = use_tooltip; }

	const t_string& label() const { return label_; }
	void set_label(const t_string& label);

	const t_string& tooltip() const { return tooltip_; }
	// Note setting the tooltip_ doesn't dirty an object.
	void set_tooltip(const t_string& tooltip) 
		{ tooltip_ = tooltip; set_wants_mouse_hover(!tooltip_.empty()); }

	const t_string& help_message() const { return help_message_; }
	// Note setting the help_message_ doesn't dirty an object.
	void set_help_message(const t_string& help_message) { help_message_ = help_message; }

	// const versions will be added when needed
	std::vector<tcanvas>& canvas() { return canvas_; }
	tcanvas& canvas(const unsigned index) 
		{ assert(index < canvas_.size()); return canvas_[index]; }

protected:
	tresolution_definition_ptr config() { return config_; }
	tresolution_definition_const_ptr config() const { return config_; }

	void set_config(tresolution_definition_ptr config) { config_ = config; }

	/***** ***** ***** ***** miscellaneous ***** ***** ***** *****/

	/** Does the widget need to restore the surface before (re)painting? */
	virtual bool needs_full_redraw() const;

	/** Sets the text variable for the canvases. */
	virtual void set_canvas_text();

private:

	/** Visible state of the control, invisible controls aren't drawn. */
	bool visible_;

	/** Contain the non-editable text associated with control. */
	t_string label_;

	/** 
	 * Can the label contain multiple lines.
	 *
	 * This is needed in order to get the sizing, when the control can contain
	 * multiple lines of text we need to find the best width/height combination.
	 */
	bool multiline_label_;

	/**
	 * If the text doesn't fit on the label should the text be used as tooltip?
	 *
	 * This only happens if the tooltip is empty.
	 */
	bool use_tooltip_on_label_overflow_;

	/**
	 * Tooltip text.
	 *
	 * The hovering event can cause a small tooltip to be shown, this is the
	 * text to be shown. At the moment the tooltip is a single line of text.
	 */
	t_string tooltip_;

	/**
	 * Tooltip text.
	 *
	 * The help event can cause a tooltip to be shown, this is the text to be
	 * shown. At the moment the tooltip is a single line of text.
	 */
	t_string help_message_;

	/**
	 * Holds all canvas objects for a control. 
	 *
	 * A control can have multiple states, which are defined in the classes
	 * inheriting from us. For every state there is a separate canvas, which is
	 * stored here. When drawing the state is determined and that canvas is
	 * drawn.
	 */
	std::vector<tcanvas> canvas_;

	/**
	 * Holds a copy of the original background.
	 *
	 * This background can be used before redrawing. This is needed for
	 * semi-tranparent items, the user defines whether it's required or not.
	 */
	surface restorer_;

	/***** ***** ***** ***** private functions ***** ***** ***** *****/

	/**
	 * Saves the portion of the background.
	 * 
	 * We expect an empty restorer and copy the part in get_rect() to the new
	 * surface. We copy the data since we want to put it back 1:1 and not a
	 * blit so can't use get_surface_portion.
	 * 
	 * @param src          background to save.
	 */
	void save_background(const surface& src);

	/**
	 * Restores a portion of the background.
	 *
	 * See save_background for more info.
	 * 
	 * @param dst          Background to restore.
	 */
	void restore_background(surface& dst);

	/**
	 * Contains the pointer to the configuration.
	 *
	 * Every control has a definition of how it should look, this contains a
	 * pointer to the definition. The definition is resolution dependant, where
	 * the resolution is the size of the Wesnoth application window. Depending
	 * on the resolution widgets can look different, use different fonts.
	 * Windows can use extra scrollbars use abbreviations as text etc.
	 */
 	tresolution_definition_ptr config_;

	/**
	 * Load class dependant config settings.
	 *
	 * load_config will call this method after loading the config, by default it
	 * does nothing but classes can override it to implement custom behaviour.
	 */
	virtual void load_config_extra() {}

	/**
	 * Returns the control_type of the control.
	 *
	 * The control_type parameter for tgui_definition::get_control() To keep the
	 * code more generic this type is required so the controls need to return
	 * the proper string here.  Might be used at other parts as well the get the
	 * type of 
	 * control involved.
	 */
	virtual const std::string& get_control_type() const = 0;

	/**
	 * Gets the best size for a text.
	 *
	 * @param minimum_size        The minimum size of the text.
	 *
	 * @returns                   The best size.
	 */
	tpoint get_best_text_size(const tpoint& minimum_size) const;


	/** 
	 * Contains a helper cache for the rendering.
	 *
	 * Creating a ttext object is quite expensive and is done on various
	 * occasions so it's cached here.
	 * 
	 * @todo Maybe if still too slow we might also copy this cache to the
	 * canvas so it can reuse our results, but for now it seems fast enough.
	 * Unfortunately that would make the dependency between the classes bigger
	 * as wanted.
	 */
	mutable font::ttext renderer_;
};

} // namespace gui2

#endif

