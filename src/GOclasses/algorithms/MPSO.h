/*****************************************************************************
 *   Copyright (C) 2008, 2009 Advanced Concepts Team (European Space Agency) *
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

// 16/05/2008: Initial version by Dario Izzo.

#ifndef PAGMO_MPSO_H
#define PAGMO_MPSO_H

#include <iostream>

#include "../../config.h"
#include "../basic/population.h"
#include "go_algorithm.h"

/// Multiple Particle Swarm Optimisation Solver (MPSO)
/**
 * The Multiple Particle Swarm Optimisation algorithm. A simple modification to PSOalgorithm that divides the swarm
 * into subswarms and exchange particles
 */

class __PAGMO_VISIBLE MPSOalgorithm: public go_algorithm {
	public:
                /// Constructor
                /**
                 * It instantiate a MPSO algorithm.
                 * \param[in] gen Generation to be evolved
                 * \param[in] inertia The particle inertia
                 * \param[in] cognitive The particle cognitive component
                 * \param[in] social The particle social component
                 * \param[in] vcoeff Defines the initial particle velocity. Must be in [0,1]. When 0 the particle initial velocity is
                 * zero, when one it is a random vector between the lower and upper bounds
                 * \param[in] nswarms Defines the number of swarms

                */
                MPSOalgorithm(int gen, const double &inertia, const double &cognitive, const double &social, const double &vcoeff, int nswarms);

                /// Algorithm
                /**
                 * It performs a call to the MPSO algorithm evolving the population for gen generations
                 * \param[in] popin Starting population
                 * \return Evolved population
                */
                virtual Population evolve(const Population &popin) const;
		virtual MPSOalgorithm *clone() const {return new MPSOalgorithm(*this);}
		virtual std::string id_object() const {return id_name(); }
	private:
		virtual void log(std::ostream &) const;
		size_t generations;
		double omega;
		double eta1;
		double eta2;
		double vcoeff;
		size_t nswarms;
};

#endif
