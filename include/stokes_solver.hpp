// Copyright (c) 2010-2022, Lawrence Livermore National Security, LLC. Produced
// at the Lawrence Livermore National Laboratory. All Rights reserved. See files
// LICENSE and NOTICE for details. LLNL-CODE-806117.
//
// This file is part of the scaleupROM library. For more information and source code
// availability visit https://lc.llnl.gov/gitlab/chung28/scaleupROM.git.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the BSD-3 license. We welcome feedback and contributions, see file
// CONTRIBUTING.md for details.

#ifndef SCALEUPROM_STOKES_SOLVER_HPP
#define SCALEUPROM_STOKES_SOLVER_HPP

#include "multiblock_solver.hpp"
#include "interfaceinteg.hpp"
#include "dg_mixed_bilin.hpp"
#include "mfem.hpp"

// By convention we only use mfem namespace as default, not CAROM.
using namespace mfem;

class SchurOperator : public Operator
{
protected:
   Operator *A, *B;//, *Bt;
   CGSolver *solver = NULL;

   bool use_amg = false;
   HypreBoomerAMG *amg_prec = NULL;

   int maxIter = -1;
   double rtol = -1.0;
   double atol = -1.0;

public:
   SchurOperator(Operator* const A_, Operator* const B_, bool use_amg = false,
                  const int &max_iter_ = 10000, const double rtol_ = 1.0e-15, const double atol_ = 1.0e-15)
      : Operator(B_->Height()), A(A_), B(B_),
        maxIter(max_iter_), rtol(rtol_), atol(atol_)
   {
      solver = new CGSolver();
      solver->SetRelTol(rtol);
      solver->SetAbsTol(atol);
      solver->SetMaxIter(maxIter);
      solver->SetPrintLevel(0);

      if (use_amg)
      {
         HypreParMatrix *A_amg = dynamic_cast<HypreParMatrix *>(A_);
         A = A_amg;
         amg_prec = new HypreBoomerAMG(*A_amg);
         amg_prec->SetPrintLevel(0);
         solver->SetPreconditioner(*amg_prec);
      }

      solver->SetOperator(*A);
   }

   virtual ~SchurOperator()
   {
      delete solver;
      delete amg_prec;
   }
   
   virtual void Mult(const Vector &x, Vector &y) const
   {
      Vector x1(A->NumCols());
      B->MultTranspose(x, x1);

      Vector y1(x1.Size());
      y1 = 0.0;
      solver->Mult(x1, y1);
      if (!solver->GetConverged())
         mfem_error("SchurOperator: A^{-1} fails to converge!\n");

      B->Mult(y1, y);
   }
};

class StokesSolver : public MultiBlockSolver
{

friend class ParameterizedProblem;

protected:
   double nu;
   Coefficient *nu_coeff = NULL;
   ConstantCoefficient minus_one;

   int porder, uorder;

   // // Finite element collection for all fe spaces.
   // View array for Finite element spaces
   Array<FiniteElementSpace *> ufes, pfes;

   // View Array for GridFunctions. Size(numSub);
   Array<GridFunction *> vels, ps;

   // interface integrator
   InterfaceNonlinearFormIntegrator *vec_diff = NULL, *norm_flux = NULL;

   // System matrix for Bilinear case.
   Array<int> u_offsets, p_offsets;
   Array2D<SparseMatrix *> m_mats, b_mats;
   BlockMatrix *mMat = NULL, *bMat = NULL;
   SparseMatrix *M = NULL, *B = NULL;

   // operators
   Array<LinearForm *> fs, gs;
   Array<BilinearForm *> ms;
   Array<MixedBilinearFormDGExtension *> bs;

   // rhs coefficients
   // The solution dimension is 1 by default, for which using VectorCoefficient is not allowed. (in LinearForm Assemble.)
   // For a derived class for vector solution, this is the first one needs to be changed to Array<VectorCoefficient*>.
   Array<VectorCoefficient *> f_coeffs;
   // Velocity Dirichlet condition
   Array<VectorCoefficient *> ud_coeffs;
   // Stress Neumann condition
   Array<VectorCoefficient *> sn_coeffs;
   bool pres_dbc = false;

   // DG parameters for interior penalty method.
   double sigma = -1.0;
   double kappa = -1.0;

   // // Used for bottom-up building, only with ComponentTopologyHandler.
   // Array<DenseMatrix *> comp_mats;
   // // boundary condition is enforced via forcing term.
   // Array<Array<DenseMatrix *> *> bdr_mats;
   // Array<Array2D<DenseMatrix *> *> port_mats;   // reference ports.

public:
   StokesSolver();

   virtual ~StokesSolver();

   GridFunction* GetVelGridFunction(const int k) const { return vels[k]; }
   GridFunction* GetPresGridFunction(const int k) const { return ps[k]; }
   const int GetVelFEOrder() const { return uorder; }
   const int GetPresFEOrder() const { return porder; }

   virtual void SetupBCVariables() override;
   virtual void AddBCFunction(std::function<void(const Vector &, Vector &)> F, const int battr = -1);
   virtual void AddBCFunction(const Vector &F, const int battr = -1);
   virtual void InitVariables();

   void DeterminePressureDirichlet();

   virtual void BuildOperators();
   virtual void BuildRHSOperators();
   virtual void BuildDomainOperators();
   
   virtual bool BCExistsOnBdr(const int &global_battr_idx);
   virtual void SetupBCOperators() override;
   virtual void SetupRHSBCOperators();
   virtual void SetupDomainBCOperators();

   virtual void AddRHSFunction(std::function<void(const Vector &, Vector &)> F)
   { f_coeffs.Append(new VectorFunctionCoefficient(vdim[0], F)); }
   virtual void AddRHSFunction(const Vector &F)
   { f_coeffs.Append(new VectorConstantCoefficient(F)); }

   virtual void Assemble();
   virtual void AssembleRHS();
   virtual void AssembleOperator();
   // For bilinear case.
   // system-specific.
   virtual void AssembleInterfaceMatrixes();

   // Component-wise assembly
   virtual void BuildCompROMElement(Array<FiniteElementSpace *> &fes_comp);
   virtual void BuildBdrROMElement(Array<FiniteElementSpace *> &fes_comp);
   virtual void BuildInterfaceROMElement(Array<FiniteElementSpace *> &fes_comp);

   virtual void Solve();

   virtual void ProjectOperatorOnReducedBasis();

   void SanityCheckOnCoeffs();

   virtual void SetParameterizedProblem(ParameterizedProblem *problem) override;

   // to ensure incompressibility for the problems with all velocity dirichlet bc.
   void SetComplementaryFlux(const Array<bool> nz_dbcs);

private:
   // NOTE: Block Matrix does not own the offsets,
   // and will access to invalid memory if the offsets variable is destroyed.
   BlockMatrix* FormBlockMatrix(SparseMatrix* const m, SparseMatrix* const b, SparseMatrix* const bt,
                                Array<int> &row_offsets, Array<int> &col_offsets);

   double ComputeBEFlux(const FiniteElement &el, ElementTransformation &Tr, VectorCoefficient &ud);
   double ComputeBEIntegral(const FiniteElement &el, ElementTransformation &Tr, Coefficient &Q);
   void ComputeBEIntegral(const FiniteElement &el, ElementTransformation &Tr,
                           VectorCoefficient &Q, Vector &result);
};

#endif