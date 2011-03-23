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

#ifndef ROCCHIO_HPP
#define ROCCHIO_HPP

#include <list>
#include <vector>
#include "utility_doc_cat_list.hpp"
#include "utility_vector.hpp"

using namespace std;

typedef pair<class_sparse_vector /* w */,
	     class_set_of_cats* /* gold standard */> class_w_cats;
typedef list<class_w_cats> class_w_cats_list;

typedef class_w_cats * class_w_cats_ptr;
typedef vector<class_w_cats_ptr> class_unique_docs_for_estimating_Th;

#ifdef BE_VERBOSE
typedef unordered_map<class_sparse_vector *, string> class_w_to_doc_name;
static class_w_to_doc_name w_to_doc_name;
typedef unordered_set<class_sparse_vector *> class_docs;
static inline void class_docs_insert(class_sparse_vector *w, class_docs &docs)
{
  docs.insert(w);
}
#else
typedef vector<class_sparse_vector *> class_docs;
static inline void class_docs_insert(class_sparse_vector *w, class_docs &docs)
{
  docs.push_back(w);
}
#endif

#endif /* ROCCHIO_HPP */
