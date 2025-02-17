//=====================================================
// File   :  action_lu_decomp.hh
// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
//=====================================================
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
#ifndef ACTION_LU_DECOMP
#define ACTION_LU_DECOMP
#include "../generic_bench/utils/utilities.h"
#include "../libs/STL/STL_interface.hh"
#include <string>
#include "../generic_bench/init/init_function.hh"
#include "../generic_bench/init/init_vector.hh"
#include "../generic_bench/init/init_matrix.hh"

using namespace std;

template<class Interface>
class Action_lu_decomp {

public :

  // Ctor

  Action_lu_decomp( int size ):_size(size)
  {
    MESSAGE("Action_lu_decomp Ctor");

    // STL vector initialization
    init_matrix<pseudo_random>(X_stl,_size);

    init_matrix<null_function>(C_stl,_size);
    init_matrix<null_function>(resu_stl,_size);

    // generic matrix and vector initialization
    Interface::matrix_from_stl(X_ref,X_stl);
    Interface::matrix_from_stl(X,X_stl);
    Interface::matrix_from_stl(C,C_stl);

    _cost = 2.0*size*size*size/3.0 + size*size;
  }

  // invalidate copy ctor

  Action_lu_decomp( const  Action_lu_decomp & )
  {
    INFOS("illegal call to Action_lu_decomp Copy Ctor");
    exit(1);
  }

  // Dtor

  ~Action_lu_decomp( void ){

    MESSAGE("Action_lu_decomp Dtor");

    // deallocation
    Interface::free_matrix(X_ref,_size);
    Interface::free_matrix(X,_size);
    Interface::free_matrix(C,_size);
  }

  // action name

  static inline std::string name( void )
  {
    return "complete_lu_decomp_"+Interface::name();
  }

  double nb_op_base( void ){
    return _cost;
  }

  inline void initialize( void ){
    Interface::copy_matrix(X_ref,X,_size);
  }

  inline void calculate( void ) {
      Interface::lu_decomp(X,C,_size);
  }

  void check_result( void ){
    // calculation check
    Interface::matrix_to_stl(C,resu_stl);

//     STL_interface<typename Interface::real_type>::lu_decomp(X_stl,C_stl,_size);
//
//     typename Interface::real_type error=
//       STL_interface<typename Interface::real_type>::norm_diff(C_stl,resu_stl);
//
//     if (error>1.e-6){
//       INFOS("WRONG CALCULATION...residual=" << error);
//       exit(0);
//     }

  }

private :

  typename Interface::stl_matrix X_stl;
  typename Interface::stl_matrix C_stl;
  typename Interface::stl_matrix resu_stl;

  typename Interface::gene_matrix X_ref;
  typename Interface::gene_matrix X;
  typename Interface::gene_matrix C;

  int _size;
  double _cost;
};

#endif
