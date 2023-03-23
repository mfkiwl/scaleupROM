// Copyright (c) 2010-2022, Lawrence Livermore National Security, LLC. Produced
// at the Lawrence Livermore National Laboratory. All Rights reserved. See files
// LICENSE and NOTICE for details. LLNL-CODE-806117.
//
// This file is part of the MFEM library. For more information and source code
// availability visit https://mfem.org.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the BSD-3 license. We welcome feedback and contributions, see file
// CONTRIBUTING.md for details.

// Implementation of Bilinear Form Integrators

#include "parameterized_problem.hpp"
#include "input_parser.hpp"

using namespace mfem;
using namespace std;

namespace function_factory
{

int index;  // parameter space index

namespace poisson0
{

double k, offset;

double rhs(const Vector &x)
{
   double tmp = 0.0;
   for (int d = 0; d < x.Size(); d++)
      tmp += x(d);
   return sin(poisson0::k * tmp + poisson0::offset);
}

}  // namespace poisson0

namespace poisson_component
{

Vector k(3), bdr_k(3);
double offset, bdr_offset;

double bdr(const Vector &x)
{
   assert(bdr_k.Size() >= x.Size());
   double tmp = 0.0;
   for (int d = 0; d < x.Size(); d++)
      tmp += poisson_component::bdr_k(d) * x(d);
   return sin(tmp + poisson_component::bdr_offset);
}

double rhs(const Vector &x)
{
   assert(k.Size() >= x.Size());
   double tmp = 0.0;
   for (int d = 0; d < x.Size(); d++)
      tmp += poisson_component::k(d) * x(d);
   return sin(tmp + poisson_component::offset);
}

}  // namespace poisson_component

}  // namespace function_factory

void ParameterizedProblem::SetParams(const std::string &key, const double &value)
{
   if (!param_map.count(key))
   {
      mfem_error("Poisson0: unknown parameter name!\n");
   }

   (*param_ptr[param_map[key]]) = value;
}

void ParameterizedProblem::SetParams(const Array<int> &indexes, const Vector &values)
{
   assert(indexes.Size() <= param_num);
   assert(indexes.Size() == values.Size());

   for (int idx = 0; idx < indexes.Size(); idx++)
      (*param_ptr[indexes[idx]]) = values(idx);
}

ParameterizedProblem* InitParameterizedProblem()
{
   ParameterizedProblem *problem = NULL;
   std::string problem_name = config.GetRequiredOption<std::string>("parameterized_problem/name");

   if (problem_name == "poisson0")
   {
      problem = new Poisson0();
   }
   else if (problem_name == "poisson_component")
   {
      problem = new PoissonComponent();
   }
   else
   {
      mfem_error("Unknown parameterized problem name!\n");
   }

   return problem;
}

/*
   Poisson0
*/

Poisson0::Poisson0()
   : ParameterizedProblem()
{
   param_num = 2;

   // pointer to static function.
   scalar_rhs_ptr = &(function_factory::poisson0::rhs);

   // Default values.
   function_factory::poisson0::k = 1.0;
   function_factory::poisson0::offset = 0.0;

   param_map["k"] = 0;
   param_map["offset"] = 1;

   param_ptr.SetSize(2);
   param_ptr[0] = &(function_factory::poisson0::k);
   param_ptr[1] = &(function_factory::poisson0::offset);
}

void Poisson0::SetParameterizedProblem(MultiBlockSolver *solver)
{
   // clean up rhs for parametrized problem.
   if (solver->rhs_coeffs.Size() > 0)
   {
      for (int k = 0; k < solver->rhs_coeffs.Size(); k++) delete solver->rhs_coeffs[k];
      solver->rhs_coeffs.SetSize(0);
   }
   // clean up boundary functions for parametrized problem.
   solver->bdr_coeffs = NULL;

   // std::string problem_name = GetProblemName();

   // This problem is set on homogenous Dirichlet BC.
   solver->AddBCFunction(0.0);

   // parameter values are set in the namespace function_factory::poisson0.
   solver->AddRHSFunction(*scalar_rhs_ptr);
}

/*
   PoissonComponent
*/

PoissonComponent::PoissonComponent()
   : ParameterizedProblem()
{
   // k (max 3) + offset (1) + bdr_k (max 3) + bdr_offset(1)
   param_num = 8;

   // pointer to static function.
   scalar_rhs_ptr = &(function_factory::poisson_component::rhs);
   scalar_bdr_ptr = &(function_factory::poisson_component::bdr);

   // Default values: a constant right-hand side with homogeneous Dirichlet BC.
   function_factory::poisson_component::k = 0.0;
   function_factory::poisson_component::offset = 1.0;
   function_factory::poisson_component::bdr_k = 0.0;
   function_factory::poisson_component::bdr_offset = 0.0;

   for (int d = 0; d < 3; d++)
   {
      param_map["k" + std::to_string(d)] = d;
      param_map["bdr_k" + std::to_string(d)] = d + 4;
   }
   param_map["offset"] = 3;
   param_map["bdr_offset"] = 7;

   param_ptr.SetSize(param_num);
   for (int d = 0; d < 3; d++)
   {
      param_ptr[d] = &(function_factory::poisson_component::k[d]);
      param_ptr[d + 4] = &(function_factory::poisson_component::bdr_k[d]);
   }
   param_ptr[3] = &(function_factory::poisson_component::offset);
   param_ptr[7] = &(function_factory::poisson_component::bdr_offset);
}

void PoissonComponent::SetParameterizedProblem(MultiBlockSolver *solver)
{
   // clean up rhs for parametrized problem.
   if (solver->rhs_coeffs.Size() > 0)
   {
      for (int k = 0; k < solver->rhs_coeffs.Size(); k++) delete solver->rhs_coeffs[k];
      solver->rhs_coeffs.SetSize(0);
   }
   // clean up boundary functions for parametrized problem.
   solver->bdr_coeffs = NULL;

   // parameter values are set in the namespace function_factory::poisson_component.
   solver->AddBCFunction(*scalar_bdr_ptr);

   // parameter values are set in the namespace function_factory::poisson_component.
   solver->AddRHSFunction(*scalar_rhs_ptr);
}