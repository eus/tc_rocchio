/*****************************************************************************
 * Copyright (C) 2011  Tadeus Prastowo (eus@member.fsf.org)                  *
 *                                                                           *
 * This program is free software: you can redistribute it and/or modify      *
 * it under the terms of the GNU General Public License as published by      *
 * the Free Software Foundation, either version 3 of the License, or         *
 * (at your option) any later version.                                       *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU General Public License for more details.                              *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
 *****************************************************************************/

#ifndef UTILITY_CLASSIFIER_HPP
#define UTILITY_CLASSIFIER_HPP

#include "utility_vector.hpp"

class class_W_property {
public:
  double P_avg; /* valid after all ESes have been processed and to estimate
		 * final threshold
		 */
  double P_max; /* valid in each ES */
  double BEP_max; /* valid in each ES to help determine P_max */
  double threshold; /* valid for final output */
  double BEP; /* valid for final output; BEP associated with threshold */
  inline void reset(void)
  {
    P_avg = 0;
    P_max = 0;
    BEP_max = 0;
    threshold = 0;
    BEP = 0;
  }
  inline void ES_reset(void)
  {
    P_max = 0;
    BEP_max = 0;
  }
  inline void update_BEP_max(double BEP, double P)
  {
    if (BEP > BEP_max) {
      BEP_max = BEP;
      P_max = P;
    }
  }
  inline class_W_property(void)
  {
    reset();
  }
  inline ~class_W_property()
  {
    reset();
  }
};

typedef pair<class_W_property, class_sparse_vector /* W */> class_classifier;

#endif /* UTILITY_CLASSIFIER_HPP */
