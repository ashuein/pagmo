/*****************************************************************************
 *   Copyright (C) 2004-2009 The PaGMO development team,                     *
 *   Advanced Concepts Team (ACT), European Space Agency (ESA)               *
 *   http://apps.sourceforge.net/mediawiki/pagmo                             *
 *   http://apps.sourceforge.net/mediawiki/pagmo/index.php?title=Developers  *
 *   http://apps.sourceforge.net/mediawiki/pagmo/index.php?title=Credits     *
 *   act@esa.int                                                             *
 *                                                                           *
 *   This program is free software; you can redistribute it and/or modify    *
 *   it under the terms of the GNU General Public License as published by    *
 *   the Free Software Foundation; either version 2 of the License, or       *
 *   (at your option) any later version.                                     *
 *                                                                           *
 *   This program is distributed in the hope that it will be useful,         *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 *   GNU General Public License for more details.                            *
 *                                                                           *
 *   You should have received a copy of the GNU General Public License       *
 *   along with this program; if not, write to the                           *
 *   Free Software Foundation, Inc.,                                         *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.               *
 *****************************************************************************/

// 30/01/10 Created by Francesco Biscani.

#ifndef PAGMO_PROBLEM_BASE_H
#define PAGMO_PROBLEM_BASE_H

#include <algorithm>
#include <boost/circular_buffer.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/shared_ptr.hpp>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <string>

#include "../atomic_counters/atomic_counters.h"
#include "../config.h"
#include "../exceptions.h"
#include "../types.h"

namespace pagmo
{

/// Problem namespace.
/**
 * This namespace contains all the problems implemented in PaGMO.
 */
namespace problem {

/// Base problem class.
class __PAGMO_VISIBLE base;

/// Alias for shared pointer to base problem.
typedef boost::shared_ptr<base> base_ptr;

/// Base problem class.
/**
 * This class represents a multiobjective mixed-integer optimisation problem defined by:
 * - a global dimension, i.e., the dimension of the global search space,
 * - the dimension of the integral (or combinatorial) part of the problem,
 * - lower and upper bounds of the global search space,
 * - an objective function that take as input a mixed-integer decision vector and returns a vector of fitnesses,
 * - a fitness dimension, i.e., the length of the fitness vector returned by the objective function.
 *
 * All dimensions are supposed to be invariant in the life cycle of a problem object.
 *
 * The bounds of the problem are allowed to vary over the whole range of double-precision values for continuous optimisation,
 * while for combinatorial optimisation the bounds must be in the [-32767,32767] range (corresponding to the INT_MIN and INT_MAX
 * constants defined in the C++ standard "climits" header file). All bounds setting functions will make sure that the following conditions are
 * respected:
 * - lower bounds are not greater than upper bounds,
 * - the bounds of the integer part of the problem are integer and they are in the allowed range.
 *
 * If the first condition is not met, an error will be raised. If the second condition is not met, the bounds will be set to the extremes
 * of the allowed range and rounded to the nearest integer as necessary.
 *
 * All problems implemented in PaGMO must derive from this base class and implement the following pure virtual methods:
 * - the clone() method, i.e., the polymorphic copy constructor,
 * - the objfun_impl() method, i.e., the implementation of the objective function, which computes the fitness vector associated to a decision vector.
 *
 * Additionally, the following virtual protected methods can be reimplemented in derived classes:
 * - human_readable_extra(), for providing extra output when printing the problem to stream,
 * - equality_operator_extra(), for providing additional criterions when testing for equality between two problems,
 * - compare_f_impl(), to reimplement the function that compares two fitness vectors (reurning true if the first vector is strictly better
 *   than the second one, false otherwise).
 *
 * @author Francesco Biscani (bluescarni@gmail.com)
 */
class __PAGMO_VISIBLE base
{
		friend std::size_t __PAGMO_VISIBLE_FUNC objfun_calls();
		friend void __PAGMO_VISIBLE_FUNC reset_objfun_calls();
		// Underlying containers used for caching decision and fitness vectors.
		typedef boost::circular_buffer<decision_vector> decision_vector_cache_type;
		typedef boost::circular_buffer<fitness_vector> fitness_vector_cache_type;
	public:
		/// Capacity of the internal caches.
		static const std::size_t cache_capacity = 5;
		/// Problem's size type: the same as pagmo::decision_vector's size type.
		typedef decision_vector::size_type size_type;
		/// Fitness' size type: the same as pagmo::fitness_vector's size type.
		typedef fitness_vector::size_type f_size_type;
		/// Constraints' size type: the same as pagmo::constraint_vector's size type.
		typedef constraint_vector::size_type c_size_type;
		base(int, int ni = 0, int nf = 1, int nc = 0, int nic = 0);
		base(const decision_vector &, const decision_vector &, int ni = 0, int nf = 1, int nc = 0, int nic = 0);
		/// Constructor from raw arrays, integer dimension, fitness dimension, global constraints dimension and inequality constraints dimension.
		/**
		 * Lower and upper bounds are initialised with the content of two arrays of size N.
		 * Construction will fail if at least one lower bound is greater than the corresponding upper bound,
		 * if integer dimension is either negative or greater than the global dimension, if fitness dimension is not positive,
		 * if constraints dimensions are negative or if inequality constraints dimension is greater than global constraints dimension.
		 */
		template <std::size_t N>
		base(const double (&v1)[N], const double (&v2)[N], int ni = 0, int nf = 1, int nc = 0, int nic = 0):
			m_i_dimension(boost::numeric_cast<size_type>(ni)),m_f_dimension(boost::numeric_cast<f_size_type>(nf)),
			m_c_dimension(boost::numeric_cast<c_size_type>(nc)),m_ic_dimension(boost::numeric_cast<c_size_type>(nic)),
			m_decision_vector_cache(boost::numeric_cast<decision_vector_cache_type::size_type>(cache_capacity)),
			m_fitness_vector_cache(boost::numeric_cast<fitness_vector_cache_type::size_type>(cache_capacity))
		{
			if (!m_f_dimension) {
				pagmo_throw(value_error,"fitness dimension must be strictly positive");
			}
			if (m_ic_dimension > m_c_dimension) {
				pagmo_throw(value_error,"inequality constraints dimension must not be greater than global constraints dimension");
			}
			construct_from_iterators(v1,v1 + N,v2,v2 + N);
			if (m_i_dimension > m_lb.size()) {
				pagmo_throw(value_error,"integer dimension must not be greater than global dimension");
			}
			// Resize properly temporary fitness storage.
			m_tmp_f1.resize(m_f_dimension);
			m_tmp_f2.resize(m_f_dimension);
			// Normalise bounds.
			normalise_bounds();
		}
		/// Constructor from iterators, integer dimension, fitness dimension, global constraints dimension and inequality constraints dimension.
		/**
		 * Lower bounds are initialised with the content in the range [start1,end1[, upper bounds with the content in the range [start2,end2[.
		 * Construction will fail if the ranges have different or null sizes, if at least one lower bound is greater than the corresponding upper bound,
		 * if integer dimension is either negative or greater than the global dimension, if fitness dimension is not positive,
		 * if constraints dimensions are negative or if inequality constraints dimension is greater than global constraints dimension.
		 */
		template <class Iterator1, class Iterator2>
		base(Iterator1 start1, Iterator1 end1, Iterator2 start2, Iterator2 end2, int ni = 0, int nf = 1, int nc = 0, int nic = 0):
			m_i_dimension(boost::numeric_cast<size_type>(ni)),m_f_dimension(boost::numeric_cast<f_size_type>(nf)),
			m_c_dimension(boost::numeric_cast<c_size_type>(nc)),m_ic_dimension(boost::numeric_cast<c_size_type>(nic)),
			m_decision_vector_cache(boost::numeric_cast<decision_vector_cache_type::size_type>(cache_capacity)),
			m_fitness_vector_cache(boost::numeric_cast<fitness_vector_cache_type::size_type>(cache_capacity))
		{
			if (!m_f_dimension) {
				pagmo_throw(value_error,"fitness dimension must be strictly positive");
			}
			if (m_ic_dimension > m_c_dimension) {
				pagmo_throw(value_error,"inequality constraints dimension must not be greater than global constraints dimension");
			}
			construct_from_iterators(start1,end1,start2,end2);
			if (m_i_dimension > m_lb.size()) {
				pagmo_throw(value_error,"integer dimension must not be greater than global dimension");
			}
			// Properly resize temporary fitness storage.
			m_tmp_f1.resize(m_f_dimension);
			m_tmp_f2.resize(m_f_dimension);
			// Normalise bounds.
			normalise_bounds();
		}
		virtual ~base();
		// Getters and setters.
		const decision_vector &get_lb() const;
		const decision_vector &get_ub() const;
		void set_bounds(const decision_vector &, const decision_vector &);
		/// Bounds setter from iterators.
		/**
		* Set lower and upper bounds to the content of the ranges [start1,end1[ and [start2,end2[. Will fail if ranges sizes do not match, if ranges sizes are different
		* from the global size of the problem or if at least one lower bound is greater than the corresponding upper bound.
		*/
		template <class Iterator1, class Iterator2>
		void set_bounds(Iterator1 start1, Iterator1 end1, Iterator2 start2, Iterator2 end2)
		{
			typedef typename std::iterator_traits<Iterator1>::difference_type d_type1;
			typedef typename std::iterator_traits<Iterator2>::difference_type d_type2;
			const d_type1 d1 = std::distance(start1,end1);
			const d_type2 d2 = std::distance(start2,end2);
			if (d1 != d2 || d1 != std::distance(m_lb.begin(),m_lb.end())) {
				pagmo_throw(value_error,"invalid or inconsistent bounds dimensions in set_bounds()");
			}
			verify_bounds(start1,end1,start2,end2);
			std::copy(start1,end1,m_lb.begin());
			std::copy(start2,end2,m_ub.begin());
			// Normalise bounds.
			normalise_bounds();
		}
		/// Bounds setter from raw arrays.
		/**
		* Set lower and upper bounds to the content of the raw arrays v1 and v2. Will fail if N is different
		* from the global size of the problem or if at least one lower bound is greater than the corresponding upper bound.
		*/
		template <std::size_t N>
		void set_bounds(const double (&v1)[N], const double (&v2)[N])
		{
			if (m_lb.size() != N) {
				pagmo_throw(value_error,"invalid bounds dimensions in set_bounds()");
			}
			verify_bounds(v1,v1 + N,v2,v2 + N);
			std::copy(v1,v1 + N,m_lb.begin());
			std::copy(v2,v2 + N,m_ub.begin());
			// Normalise bounds.
			normalise_bounds();
		}
		void set_lb(const decision_vector &);
		void set_lb(int, const double &);
		void set_lb(const double &);
		/// Lower bounds setter from iterators.
		/**
		 * Will fail if the iterator distance is different from global problem dimension or if at least one lower bound is greater than the
		 * corresponding upper bound.
		 */
		template <class Iterator>
		void set_lb(Iterator start, Iterator end)
		{
			if (std::distance(start,end) != std::distance(m_lb.begin(),m_lb.end())) {
				pagmo_throw(value_error,"invalid bounds dimension in set_lb()");
			}
			verify_bounds(start,end,m_ub.begin(),m_ub.end());
			std::copy(start,end,m_lb.begin());
			// Normalise bounds.
			normalise_bounds();
		}
		/// Lower bounds setter from raw array.
		/**
		 * Will fail if N is different from global problem dimension or if at least one lower bound is greater than the
		 * corresponding upper bound.
		 */
		template <std::size_t N>
		void set_lb(const double (&v)[N])
		{
			if (N != m_lb.size()) {
				pagmo_throw(value_error,"invalid bounds dimension in set_lb()");
			}
			verify_bounds(v,v + N,m_ub.begin(),m_ub.end());
			std::copy(v,v + N,m_lb.begin());
			// Normalise bounds.
			normalise_bounds();
		}
		void set_ub(const decision_vector &);
		void set_ub(int, const double &);
		void set_ub(const double &);
		/// Upper bounds setter from iterators.
		/**
		 * Will fail if the iterator distance is different from global problem dimension or if at least one upper bound is less than the
		 * corresponding lower bound.
		 */
		template <class Iterator>
		void set_ub(Iterator start, Iterator end)
		{
			if (std::distance(start,end) != std::distance(m_lb.begin(),m_lb.end())) {
				pagmo_throw(value_error,"invalid bounds dimension in set_ub()");
			}
			verify_bounds(m_lb.begin(),m_lb.end(),start,end);
			std::copy(start,end,m_ub.begin());
			// Normalise bounds.
			normalise_bounds();
		}
		/// Upper bounds setter from raw array.
		/**
		 * Will fail if N is different from global problem dimension or if at least one upper bound is less than the
		 * corresponding lower bound.
		 */
		template <std::size_t N>
		void set_ub(const double (&v)[N])
		{
			if (N != m_lb.size()) {
				pagmo_throw(value_error,"invalid bounds dimension in set_ub()");
			}
			verify_bounds(m_lb.begin(),m_lb.end(),v,v + N);
			std::copy(v,v + N,m_ub.begin());
			// Normalise bounds.
			normalise_bounds();
		}
		size_type get_dimension() const;
		size_type get_i_dimension() const;
		f_size_type get_f_dimension() const;
		fitness_vector objfun(const decision_vector &) const;
		void objfun(fitness_vector &, const decision_vector &) const;
		/// Clone method.
		virtual base_ptr clone() const = 0;
		std::string human_readable() const;
		bool operator==(const base &) const;
		bool operator!=(const base &) const;
		bool compare_x(const decision_vector &, const decision_vector &) const;
		bool compare_f(const fitness_vector &, const fitness_vector &) const;
		bool verify_x(const decision_vector &) const;
	protected:
		//virtual void pre_evolution(island &) const;
		//virtual void post_evolution(island &) const;
		virtual std::string human_readable_extra() const;
		virtual bool equality_operator_extra(const base &) const;
		virtual bool compare_f_impl(const fitness_vector &, const fitness_vector &) const;
		/// Objective function implementation.
		/**
		 * Takes a pagmo::decision_vector x as input and writes its pagmo::fitness_vector to f. This function is not to be called directly,
		 * it is invoked by objfun() after a series of safety checks is performed on x and f.
		 */
		virtual void objfun_impl(fitness_vector &f, const decision_vector &x) const = 0;
	private:
		void normalise_bounds();
		// Construct from iterators.
		template <class Iterator1, class Iterator2>
		void construct_from_iterators(Iterator1 start1, Iterator1 end1, Iterator2 start2, Iterator2 end2)
		{
			m_lb.insert(m_lb.end(),start1,end1);
			m_ub.insert(m_ub.end(),start2,end2);
			if (m_lb.size() != m_ub.size() || m_lb.size() == 0) {
				pagmo_throw(value_error,"null or inconsistent dimension(s) for upper/lower bounds while constructing problem");
			}
			verify_bounds(m_lb.begin(),m_lb.end(),m_ub.begin(),m_ub.end());
		}
		// Verify upper/lower bounds. This must be called only after having made sure that the iterator distances
		// are consistent.
		template <class Iterator1, class Iterator2>
		static void verify_bounds(Iterator1 start1, Iterator1 end1, Iterator2 start2, Iterator2 end2)
		{
			for (; start1 != end1 && start2 != end2; ++start1, ++start2) {
				if (*start1 > *start2) {
					pagmo_throw(value_error,"lower bound is greater than upper bound");
				}
			}
		}
	private:
		// Data members.
		// Size of the integer part of the problem.
		const size_type				m_i_dimension;
		// Size of the fitness vector.
		const f_size_type			m_f_dimension;
		// Global constraints dimension.
		const c_size_type			m_c_dimension;
		// Inequality constraints dimension
		const c_size_type			m_ic_dimension;
		// Lower bounds.
		decision_vector				m_lb;
		// Upper bounds.
		decision_vector				m_ub;
		// Decision vector cache.
		mutable decision_vector_cache_type	m_decision_vector_cache;
		// Fitness vector cache.
		mutable fitness_vector_cache_type	m_fitness_vector_cache;
		// Temporary storage used during decision_vector comparisons.
		mutable fitness_vector			m_tmp_f1;
		mutable fitness_vector			m_tmp_f2;
		// Objective function calls counter.
		static atomic_counter_size_t		m_objfun_counter;
};

std::ostream __PAGMO_VISIBLE_FUNC &operator<<(std::ostream &, const base &);

std::size_t __PAGMO_VISIBLE_FUNC objfun_calls();
void __PAGMO_VISIBLE_FUNC reset_objfun_calls();

}
}

#endif