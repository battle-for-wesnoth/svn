/* $Id$ */
/*
   Copyright (C) 2006 - 2009 by Rusty Russell <rusty@rustcorp.com.au>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/** @file unit_map.hpp */

#ifndef UNIT_MAP_H_INCLUDED
#define UNIT_MAP_H_INCLUDED

class unit;
#include <cassert>
#include <cstring>
#include <boost/static_assert.hpp>
#include "map_location.hpp"

// unit_map is used primarily as a map<location, unit>, but we need to be able to move a unit
// without copying it as the ai moves units a lot.
// Also, we want iterators to remain valid as much as reasonable as the underlying map is changed
// and we want a way to test an iterator for validity.

class unit_map
{
private:

	/**
	 * Used so unit_map can keep a count of iterators and clean invalid pointers
	 * only when no iterators exist. Every iterator and accessor has a counter
	 * instance.
	 */
	struct iterator_counter {
		iterator_counter() : map_(NULL) {}
		iterator_counter(const unit_map* map) : map_(map)
			{ if (map_) map_->add_iter(); }

		iterator_counter(const iterator_counter& i) : map_(i.map_) 
			{ if (map_) map_->add_iter(); }

		iterator_counter& operator=(const iterator_counter &that) {
			if (map_) map_->remove_iter();

			map_ = that.map_;
			if (map_) map_->add_iter();

			return *this;
		}

		~iterator_counter() 
			{if (map_) map_->remove_iter(); }
	private:
		const unit_map* map_;
	};

	struct node {
		bool valid_;
		std::pair<map_location, unit>* ptr_;

		node(bool v, std::pair<map_location, unit>* p) : valid_(v), ptr_(p) { }
		node() : valid_(0), ptr_(NULL) { }

		bool is_valid() const { return valid_; }
		bool& get_valid() { return valid_; }
		unit& get_unit() const;
		map_location& get_location() const;
	};

public:

	/** unit.underlying_id() type */
	typedef size_t unit_id_type;

	/** unit.underlying_id() -> unit_map::node . This requires that underlying_id() be unique (which is enforced in unit_map::add).*/
	typedef std::map<unit_id_type,node> umap;

	/** location -> unit.underlying_id() of the unit at that location */
	typedef std::map<map_location, unit_id_type> lmap;

	unit_map() : map_(), lmap_(), num_iters_(0), num_invalid_(0) { };
	unit_map(const unit_map &that);
	unit_map& operator=(const unit_map &that);

	/** A unit map with a copy of a single unit in it. */
	unit_map(const map_location &loc, const unit &u);
	~unit_map();


// ~~~ Begin iterator code ~~~

	template <typename X, typename Y>
	struct convertible;

	template <template <typename> class iter_policy, typename iter_types>
	struct iterator_base {
		typedef typename iter_types::map_type map_type;
		typedef typename iter_types::iterator_type iterator_type;
		typedef typename iter_types::value_type value_type;
		typedef typename iter_types::reference_type reference_type;
		typedef typename iter_types::pointer_type pointer_type;

		iterator_base() : policy_(), counter_(), map_(NULL), i_() { }
		iterator_base(iterator_type i, map_type* m) : policy_(i, m), counter_(m), map_(m), i_(i) { }

		pointer_type operator->() const { assert(policy_.valid(i_, map_)); return i_->second.ptr_; }
		reference_type operator*() const { assert(policy_.valid(i_, map_)); return *i_->second.ptr_; }

		iterator_base& operator++();
		iterator_base operator++(int);

		iterator_base& operator--();
		iterator_base operator--(int);

		bool valid() const { return policy_.valid(i_, map_); }

		bool operator==(const iterator_base& rhs) const { return i_ == rhs.i_; }
		bool operator!=(const iterator_base& rhs) const { return !operator==(rhs); }		

		template <template <typename> class that_policy, typename that_types>
		iterator_base(const iterator_base<that_policy, that_types>& that) :
			policy_(that.i_, that.map_),
			counter_(that.counter_),
			map_(that.map_),
			i_(that.i_)
		{ 
			BOOST_STATIC_ASSERT(sizeof(convertible<that_policy<that_types>, iter_policy<iter_types> >) != 0);
			BOOST_STATIC_ASSERT(sizeof(convertible<that_types, iter_types>) != 0);
		}

		map_type* get_map() const { return map_; }

		template <template <typename> class X, typename Y> friend class iterator_base;
	
	private:	 
		iter_policy<iter_types> policy_;
		iterator_counter counter_;
		map_type* map_;
		iterator_type i_;
	};

	struct standard_iter_types {
		typedef unit_map map_type;
		typedef unit_map::umap::iterator iterator_type;
		typedef std::pair<map_location, unit> value_type;
		typedef value_type* pointer_type;
		typedef value_type& reference_type;
	};

	struct const_iter_types {
		typedef const unit_map map_type;
		typedef unit_map::umap::const_iterator iterator_type;
		typedef const std::pair<map_location, unit> value_type;
		typedef value_type* pointer_type;
		typedef value_type& reference_type;
	};

	template <typename iter_types>
	struct unit_policy {
		typedef typename iter_types::iterator_type iterator_type;

		unit_policy() { }
		unit_policy(const iterator_type& /*i*/, const unit_map* /*map*/) { }
		
		void update(const iterator_type& /*i*/, const unit_map* /*map*/) { }

		bool valid(const iterator_type& i, const unit_map* map) const { return i != map->map_.end() && i->second.is_valid(); }
	};

	template <typename iter_types>
	struct unit_xy_policy {		
		typedef typename iter_types::iterator_type iterator_type;

		unit_xy_policy() { }
		unit_xy_policy(const iterator_type& i, const unit_map* map) : loc_(i->second.get_location()) { }

		void update(iterator_type& iter, const unit_map* map) {
			loc_ = iter->second.get_location();
		}

		bool valid(const iterator_type& iter, const unit_map* map) const {
			return iter != map->map_.end() && iter->second.is_valid() && iter->second.get_location() == loc_;
		} 		
	private:
		map_location loc_;
	};

	template <typename iter_types>
	struct xy_accessor_base {		
		typedef typename iter_types::map_type map_type;
		typedef typename iter_types::iterator_type iterator_type;
		typedef typename iter_types::reference_type reference_type;

		xy_accessor_base() : counter_(), loc_(), map_(NULL) { }
		xy_accessor_base(const iterator_type& iter, const unit_map* map) : counter_(), loc_(iter->second.get_location()), map_(map) { }

		template <typename that_types>
		xy_accessor_base(const xy_accessor_base<that_types>& that) :
			counter_(that.counter_),
			loc_(that.loc_),
			map_(that.map_) 
		{
			BOOST_STATIC_ASSERT(sizeof(convertible<that_types, iter_types>) != 0);
		}

		template <typename that_types>
		xy_accessor_base(const iterator_base<unit_policy, that_types>& that) :
			counter_(that.get_map()),
			loc_(),
			map_(that.get_map())
		{
			BOOST_STATIC_ASSERT(sizeof(convertible<that_types, iter_types>) != 0);
			if (that.valid()) loc_ = that->first;
		}

		iterator_base<unit_policy, iter_types> operator->() const { assert(valid()); return map_->find(loc_); }
		reference_type operator*() const { assert(valid()); return *map_->find(loc_); }

		bool operator==(const xy_accessor_base& rhs) { return loc_ == rhs.loc_; }
		bool operator!=(const xy_accessor_base& rhs) { return !operator==(rhs); }

		bool valid() const { return map_->find(loc_) != map_->end(); }
	private:
		iterator_counter counter_;
		map_location loc_;
		map_type* map_;
	};

// ~~~ End iterator code ~~~



	/**
	 * All iterators are invalidated on unit_map::clear(), unit_map::swap(), unit_map::delete_all(), etc.
	 * Several implicit conversion are supplied, you can convert from more restrictive to less restrictive.
	 * That is:
	 * non-const -> const
	 * unit_iter -> xy_unit_iter
	 * unit_iter -> xy_accessor
	 */
	
	/** 
	 * unit_iterators iterate over all units in the unit_map. A unit_iterator is invalidated if 
	 * unit_map::erase, unit_map::extract, or unit_map::replace are called with the location of the
	 * unit that the unit_iterator points to. It will become valid again if unit_map::add is called with
	 * a pair containing the unit that the iterator points to. Basically, as long as the unit is on the
	 * gamemap somewhere, the iterator will be valid.
	 */
	typedef iterator_base<unit_policy, standard_iter_types> unit_iterator;
	typedef iterator_base<unit_policy, const_iter_types> const_unit_iterator;

	/**
	 * unit_xy_iterators have all the constraints of unit_iterators and will also be invalidated if the unit
	 * is not at the same location as when the iterator first pointed at it. That is, as long as the unit stays
	 * in the same spot, the iterator is valid.
	 */
	typedef iterator_base<unit_xy_policy, standard_iter_types> unit__xy_iterator;
	typedef iterator_base<unit_xy_policy, const_iter_types> const_unit_xy_iterator;

	/**
	 * xy_accessors cannot be incremented or decremented. An xy_accessor is valid as long as there is a unit at
	 * the map_location that it is created at, and it allows access to the unit at that location.
	 */
	typedef xy_accessor_base<standard_iter_types> xy_accessor;
	typedef xy_accessor_base<const_iter_types> const_xy_accessor;

	/** provided as a convenience as unit_map used to be an std::map */
	typedef unit_iterator iterator;
	typedef const_unit_iterator const_iterator;


	/**
	 * Lookups can be done by map_location, by unit::underlying_id(), or by unit::id(). 
	 * Looking up with unit::id() is slow. 
	 * Returned iterators can be implicitly converted to the other types.
	 */
	unit_iterator find(const map_location& loc) ;
	unit_iterator find(const unit_id_type& id);
	unit_iterator find(const std::string& id);

	const_unit_iterator find(const map_location& loc) const;
	const_unit_iterator find(const unit_id_type& id) const;
	const_unit_iterator find(const std::string& id) const;

	size_t count(const map_location& loc) const { return lmap_.count(loc); }

	/**
	 * Return iterators are implicitly converted to other types as needed.
	 */
	unit_iterator begin();
	const_unit_iterator begin() const;

	unit_iterator end() { return iterator(map_.end(), this); }
	const_unit_iterator end() const { return const_iterator(map_.end(), this); }
	
	size_t size() const { return lmap_.size(); }

	void clear();

	/**
	 * Map owns pointer after this.  Loc must be currently empty. unit's
	 * underlying_id should not be present in the map already.
	 */
	void add(std::pair<map_location,unit> *p);

	/** Like add, but loc must be occupied (implicitly erased). */
	void replace(std::pair<map_location,unit> *p);

	/** Erases the pair<location, unit> of this location. */
	void erase(xy_accessor pos);
	size_t erase(const map_location &loc);

	/** Extract (like erase, but don't delete). */
	std::pair<map_location,unit>* extract(const map_location& loc);

	/** Invalidates all iterators on both maps */
	void swap(unit_map& o) {
		std::swap(map_, o.map_);
		std::swap(lmap_, o.lmap_);
		std::swap(num_invalid_, o.num_invalid_);
	}

private:
	/** Removes invalid entries in map_ if safe and needed. */
	void clean_invalid();

	void invalidate(umap::iterator& i)
		{ if(i == map_.end()) return; i->second.get_valid() = false; ++num_invalid_; }
	void validate(umap::iterator& i)
		{ if(i == map_.end()) return; i->second.get_valid() = true; --num_invalid_; }

	void delete_all();

	void add_iter() const { ++num_iters_; }
	void remove_iter() const { --num_iters_; }

	bool is_valid(const umap::const_iterator& i) const { return i != map_.end() && i->second.is_valid(); }

	/**
	 * unit.underlying_id() -> unit_map::node
	 */
	umap map_;

	/** location -> unit.underlying_id(). Unit_map is usually used as though it
	 * is a map<location, unit> and so we need this map for efficient
	 * access/modification.
	 */
	lmap lmap_;

	mutable size_t num_iters_;
	size_t num_invalid_;
};

// define allowed conversions.
template <>
struct unit_map::convertible<unit_map::standard_iter_types, unit_map::const_iter_types> { };

template <typename T>
struct unit_map::convertible<T, T> { };

template <typename X, typename Y>
struct unit_map::convertible<unit_map::unit_policy<X>, unit_map::unit_policy<Y> > { };

template <typename X, typename Y>
struct unit_map::convertible<unit_map::unit_policy<X>, unit_map::unit_xy_policy<Y> > { };

template <typename X, typename Y>
struct unit_map::convertible<unit_map::unit_xy_policy<X>, unit_map::unit_xy_policy<Y> > { };



template <template <typename> class iter_policy, typename iter_types>
unit_map::iterator_base<iter_policy, iter_types>& unit_map::iterator_base<iter_policy, iter_types>::operator++() {
	assert(i_ != map_->map_.end());
	++i_;
	while (i_ != map_->map_.end() && !i_->second.is_valid()) ++i_;

	if (i_ != map_->map_.end()) policy_.update(i_, map_);

	return *this;	
}

template <template <typename> class iter_policy, typename iter_types>
unit_map::iterator_base<iter_policy, iter_types> unit_map::iterator_base<iter_policy, iter_types>::operator++(int) {
	unit_map::iterator_base<iter_policy, iter_types> temp(*this);
	operator++();
	return temp;
}

template <template <typename> class iter_policy, typename iter_types>
unit_map::iterator_base<iter_policy, iter_types>& unit_map::iterator_base<iter_policy, iter_types>::operator--() {
	assert(i_ != map_->map_.begin());
	
	--i_;
	while (i_ != map_->map_.begin() && !i_->second.is_valid()) --i_;

	if (i_->second.is_valid()) policy_.update(i_, map_);

	return *this;	
}

template <template <typename> class iter_policy, typename iter_types>
unit_map::iterator_base<iter_policy, iter_types> unit_map::iterator_base<iter_policy, iter_types>::operator--(int) {
	unit_map::iterator_base<iter_policy, iter_types> temp(*this);
	operator--();
	return temp;
}

#endif	// UNIT_MAP_H_INCLUDED
