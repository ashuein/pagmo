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

#include <boost/random/uniform_int.hpp>
#include <boost/random/uniform_real.hpp>
#include <string>
#include <vector>

#include "../exceptions.h"
#include "../population.h"
#include "../problem/base.h"
#include "../types.h"
#include "base.h"
#include "mbh.h"

namespace pagmo { namespace algorithm {

/// Constructor.
/**
 * Allows to specify in detail all the parameters of the algorithm.
 *
 * @param[in] local pagmo::algorithm to use as 'local' optimization method
 * @param[in] stop number of consecutive step allowed without any improvement
 * @param[in] perturb At the end of one iteration of mbh, each chromosome of each population individual
 * will be perturbed by +-perturb*(ub - lb), the same for the velocity. The integer part is treated the same way.
 * @throws value_error if stop is negative or perturb is not in ]0,1]
 */
mbh::mbh(const algorithm::base & local, int stop, double perturb):base(),m_stop(stop),m_perturb(perturb)
{
	m_local = local.clone();
	if (stop < 0) {
		pagmo_throw(value_error,"number of consecutive step allowed without any improvement needs to be positive");
	}
	if (perturb <= 0 || perturb > 1) {
		pagmo_throw(value_error,"perturb must be in ]0,1]");
	}
}

/// Clone method.
base_ptr mbh::clone() const
{
	return base_ptr(new mbh(*this));
}

/// Evolve implementation.
/**
 * Run the MBH algorithm
 *
 * @param[in,out] pop input/output pagmo::population to be evolved.
 */

void mbh::evolve(population &pop) const
{
	// Let's store some useful variables.
	const problem::base &prob = pop.problem();
	const problem::base::size_type D = prob.get_dimension(), prob_i_dimension = prob.get_i_dimension();
	const decision_vector &lb = prob.get_lb(), &ub = prob.get_ub();
	const population::size_type NP = pop.size();
	const problem::base::size_type Dc = D - prob_i_dimension;

	// Get out if there is nothing to do.
	if (m_stop == 0 || NP == 0) {
		return;
	}

	// Some dummies and temporary variables
	decision_vector tmp_x(D), tmp_v(D);
	double dummy, width;

	// Init the best fitness and constraint vector
	fitness_vector best_f = pop.get_individual(pop.get_best_idx()).cur_f;
	constraint_vector best_c = pop.get_individual(pop.get_best_idx()).cur_c;
	population best_pop(pop);

	int i = 0;

	//mbh main loop

	while (i<m_stop){
		//1. Evolve population with selected algorithm
		m_local->evolve(pop); i++;

		//2. Reset counter if improved
		if (pop.problem().compare_fc(pop.get_individual(pop.get_best_idx()).cur_f,pop.get_individual(pop.get_best_idx()).cur_c,best_f,best_c) )
		{
			if (m_screen_out)
			{
				std::cout << "Improved after: " << i << "\tBest-so-far: " << pop.get_individual(pop.get_best_idx()).cur_f <<std::endl;
			}
			i = 0;
			best_f = pop.get_individual(pop.get_best_idx()).cur_f;
			best_c = pop.get_individual(pop.get_best_idx()).cur_c;
			//update best population
			for (population::size_type j=0; j<pop.size();++j)
			{
				best_pop.set_x(j,pop.get_individual(j).cur_x);
				best_pop.set_v(j,pop.get_individual(j).cur_v);
			}
		}

		//3. Perturb the current population (this could be moved in a pagmo::population method should other algorithm use it....
		for (population::size_type j =0; j < NP; ++j)
		{
			for (decision_vector::size_type k=0; k < Dc; ++k)
			{
				dummy = best_pop.get_individual(j).best_x[k];
				width = (ub[k]-lb[k]) * m_perturb / 2;
				tmp_x[k] = boost::uniform_real<double>(std::max(dummy-width,lb[k]),std::min(dummy+width,ub[k]))(m_drng);
				dummy = best_pop.get_individual(j).cur_v[k];
				tmp_v[k] = boost::uniform_real<double>(dummy-width,dummy+width)(m_drng);
			}

			for (decision_vector::size_type k=Dc; k < D; ++k)
			{
				dummy = best_pop.get_individual(j).best_x[k];
				width = (ub[k]-lb[k]) * m_perturb / 2;
				tmp_x[k] = boost::uniform_int<int>(std::max(dummy-width,lb[k]),std::min(dummy+width,ub[k]))(m_urng);
				dummy = best_pop.get_individual(j).cur_v[k];
				tmp_v[k] = boost::uniform_int<int>(std::max(dummy-width,lb[k]),std::min(dummy+width,ub[k]))(m_urng);
			}
			pop.set_x(j,tmp_x);
			pop.set_v(j,tmp_v);
		}
	}
	//on exit set the population to the best one (discard perturbations)
	for (population::size_type j=0; j<pop.size();++j)
	{
		pop.set_x(j,best_pop.get_individual(j).cur_x);
		pop.set_v(j,best_pop.get_individual(j).cur_v);
	}
}

/// Activate screen output
/**
 * Activate screen output. Everytime a new champion is found the following information is printed
 * on the screen: Number of consecutive non improving iterations, best fitness
 *
 * @param[in] p true or false
 */
void mbh::screen_output(const bool p) {m_screen_out = p;}


/// Algorithm name
std::string mbh::get_name() const
{
	return "Generalized Monotonic Basin Hopping";
}


/// Extra human readable algorithm info.
/**
 * @return a formatted string displaying the parameters of the algorithm.
 */
std::string mbh::human_readable_extra() const
{
	std::ostringstream s;
	s << "\tSelected sub-algorithm:\t\t\t" << m_local->get_name() << '\n';
	s << "\tAllowed not improving iterations:\t" << m_stop << '\n';
	s << "\tPerturbation width:\t\t\t" << m_perturb << '\n';
	return s.str();
}

}} //namespaces
