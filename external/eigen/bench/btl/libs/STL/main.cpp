//=====================================================
// File   :  main.cpp
// Author :  L. Plagne <laurent.plagne@edf.fr)>
// Copyright (C) EDF R&D,  lun sep 30 14:23:23 CEST 2002
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
#include "../../generic_bench/utils/utilities.h"
#include "STL_interface.hh"
#include "../../generic_bench/bench.hh"
#include "../../actions/basic_actions.hh"

BTL_MAIN;

int main()
{
  bench<Action_axpy<STL_interface<REAL_TYPE> > >(MIN_AXPY,MAX_AXPY,NB_POINT);
  bench<Action_axpby<STL_interface<REAL_TYPE> > >(MIN_AXPY,MAX_AXPY,NB_POINT);
  bench<Action_matrix_vector_product<STL_interface<REAL_TYPE> > >(MIN_MV,MAX_MV,NB_POINT);
  bench<Action_atv_product<STL_interface<REAL_TYPE> > >(MIN_MV,MAX_MV,NB_POINT);
  bench<Action_symv<STL_interface<REAL_TYPE> > >(MIN_MV,MAX_MV,NB_POINT);
  bench<Action_syr2<STL_interface<REAL_TYPE> > >(MIN_MV,MAX_MV,NB_POINT);
  bench<Action_matrix_matrix_product<STL_interface<REAL_TYPE> > >(MIN_MM,MAX_MM,NB_POINT);
  bench<Action_ata_product<STL_interface<REAL_TYPE> > >(MIN_MM,MAX_MM,NB_POINT);
  bench<Action_aat_product<STL_interface<REAL_TYPE> > >(MIN_MM,MAX_MM,NB_POINT);

  return 0;
}


