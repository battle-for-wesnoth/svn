#ifndef THEME_HPP_INCLUDED
#define THEME_HPP_INCLUDED

#include <map>
#include <vector>

#include "config.hpp"

#include "SDL.h"

class theme
{

	class object
	{
	public:
		object();
		object(const config& cfg);

		SDL_Rect& location(const SDL_Rect& screen) const;

		//all on-screen objects have 'anchoring' in the x and y dimensions
		//'fixed' means that they have fixed co-ordinates and don't move
		//'top anchored' means they are anchored to the top (or left) side of
		//the screen - the top (or left) edge stays a constant distance from
		//the top of the screen
		//'bottom anchored' is the inverse of top anchored
		//'proportional' means the location and dimensions change proportionally
		//to the screen size
		enum ANCHORING { FIXED, TOP_ANCHORED, PROPORTIONAL, BOTTOM_ANCHORED };

	private:
		SDL_Rect loc_;
		mutable SDL_Rect relative_loc_;
		mutable SDL_Rect last_screen_;

		ANCHORING xanchor_, yanchor_;

		static ANCHORING read_anchor(const std::string& str);
	};

public:

	class label : private object
	{
	public:
		label();
		explicit label(const config& cfg);

		using object::location;

		const std::string& text() const;
		const std::string& icon() const;

		bool empty() const;

		size_t font_size() const;

	private:
		std::string text_, icon_;
		size_t font_;
	};

	class status_item : private object
	{
	public:

		explicit status_item(const config& cfg);

		using object::location;
		
		const std::string& prefix() const;
		const std::string& postfix() const;

		//function returns true if the colour of the label should depend
		//upon whether it's good/bad or not. E.g. for hitpoints, whether
		//the hitpoints should be displayed in different colours depending
		//on whether they are high or low.
		bool context_colouring() const;

		//if the item has a label associated with it, show where the label is
		const label* get_label() const;

		size_t font_size() const;

	private:
		std::string prefix_, postfix_;
		bool context_;
		label label_;
		size_t font_;
	};

	class panel : private object
	{
	public:
		explicit panel(const config& cfg);

		using object::location;

		const std::string& image() const;

	private:
		std::string image_;
	};

	explicit theme(const config& cfg, const SDL_Rect& screen);
	bool set_resolution(const SDL_Rect& screen);

	const std::vector<panel>& panels() const;
	const std::vector<label>& labels() const;

	const status_item* get_status_item(const std::string& item) const;

	const SDL_Rect& main_map_location(const SDL_Rect& screen) const;
	const SDL_Rect& mini_map_location(const SDL_Rect& screen) const;

private:
	const config& cfg_;
	std::vector<panel> panels_;
	std::vector<label> labels_;

	std::map<std::string,status_item> status_;

	object main_map_, mini_map_;
};

#endif
