//=====================================================
// File   :  action_matrix_vector_product.hh
// Author :  L. Plagne <laurent.plagne@edf.fr)>
// Copyright (C) EDF R&D,  lun sep 30 14:23:19 CEST 2002
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
#ifndef ACTION_MATRIX_VECTOR_PRODUCT
#define ACTION_MATRIX_VECTOR_PRODUCT
#include "../generic_bench/utils/utilities.h"
#include "../libs/STL/STL_interface.hh"
#include <string>
#include "../generic_bench/init/init_function.hh"
#include "../generic_bench/init/init_vector.hh"
#include "../generic_bench/init/init_matrix.hh"

using namespace std;

template<class Interface>
class Action_matrix_vector_product {

public :

  // Ctor

  BTL_DONT_INLINE Action_matrix_vector_product( int size ):_size(size)
  {
    MESSAGE("Action_matrix_vector_product Ctor");

    // STL matrix and vector initialization

    init_matrix<pseudo_random>(A_stl,_size);
    init_vector<pseudo_random>(B_stl,_size);
    init_vector<null_function>(X_stl,_size);
    init_vector<null_function>(resu_stl,_size);

    // generic matrix and vector initialization

    Interface::matrix_from_stl(A_ref,A_stl);
    Interface::matrix_from_stl(A,A_stl);
    Interface::vector_from_stl(B_ref,B_stl);
    Interface::vector_from_stl(B,B_stl);
    Interface::vector_from_stl(X_ref,X_stl);
    Interface::vector_from_stl(X,X_stl);

  }

  // invalidate copy ctor

  Action_matrix_vector_product( const  Action_matrix_vector_product & )
  {
    INFOS("illegal call to Action_matrix_vector_product Copy Ctor");
    exit(1);
  }

  // Dtor

  BTL_DONT_INLINE ~Action_matrix_vector_product( void ){

    MESSAGE("Action_matrix_vector_product Dtor");

    // deallocation

    Interface::free_matrix(A,_size);
    Interface::free_vector(B);
    Interface::free_vector(X);

    Interface::free_matrix(A_ref,_size);
    Interface::free_vector(B_ref);
    Interface::free_vector(X_ref);

  }

  // action name

  static inline std::string name( void )
  {
    return "matrix_vector_" + Interface::name();
  }

  double nb_op_base( void ){
    return 2.0*_size*_size;
  }

  BTL_DONT_INLINE  void initialize( void ){

    Interface::copy_matrix(A_ref,A,_size);
    Interface::copy_vector(B_ref,B,_size);
    Interface::copy_vector(X_ref,X,_size);

  }

  BTL_DONT_INLINE void calculate( void ) {
      BTL_ASM_COMMENT("#begin matrix_vector_product");
      Interface::matrix_vector_product(A,B,X,_size);
      BTL_ASM_COMMENT("end matrix_vector_product");
  }

  BTL_DONT_INLINE void check_result( void ){

    // calculation check

    Interface::vector_to_stl(X,resu_stl);

    STL_interface<typename Interface::real_type>::matrix_vector_product(A_stl,B_stl,X_stl,_size);

    typename Interface::real_type error=
      STL_interface<typename Interface::real_type>::norm_diff(X_stl,resu_stl);

    if (error>1.e-5){
      INFOS("WRONG CALCULATION...residual=" << error);
      exit(0);
    }

  }

private :

  typename Interface::stl_matrix A_stl;
  typename Interface::stl_vector B_stl;
  typename Interface::stl_vector X_stl;
  typename Interface::stl_vector resu_stl;

  typename Interface::gene_matrix A_ref;
  typename Interface::gene_vector B_ref;
  typename Interface::gene_vector X_ref;

  typename Interface::gene_matrix A;
  typename Interface::gene_vector B;
  typename Interface::gene_vector X;


  int _size;

};


#endif



