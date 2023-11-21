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

#include "hyperreduction_integ.hpp"
#include "linalg_utils.hpp"

using namespace std;

namespace mfem
{

void HyperReductionIntegrator::AssembleQuadratureVector(
   const FiniteElement &el, ElementTransformation &T, const IntegrationPoint &ip,
   const double &iw, const Vector &eltest, Vector &elquad)
{
   mfem_error ("HyperReductionIntegrator::AssembleQuadratureVector(...)\n"
               "for element is not implemented for this class.");
}

void HyperReductionIntegrator::AssembleQuadratureVector(
   const FiniteElement &el1, const FiniteElement &el2, FaceElementTransformations &T,
   const IntegrationPoint &ip, const double &iw, const Vector &eltest, Vector &elquad)
{
   mfem_error ("HyperReductionIntegrator::AssembleQuadratureVector(...)\n"
               "for face is not implemented for this class.");
}

void HyperReductionIntegrator::AssembleQuadratureGrad(
   const FiniteElement &el, ElementTransformation &T, const IntegrationPoint &ip,
   const double &iw, const Vector &eltest, DenseMatrix &quadmat)
{
   mfem_error ("HyperReductionIntegrator::AssembleQuadratureGrad(...)\n"
               "for element is not implemented for this class.");
}

void HyperReductionIntegrator::AssembleQuadratureGrad(
   const FiniteElement &el1, const FiniteElement &el2, FaceElementTransformations &T,
   const IntegrationPoint &ip, const double &iw, const Vector &eltest, DenseMatrix &quadmat)
{
   mfem_error ("HyperReductionIntegrator::AssembleQuadratureGrad(...)\n"
               "for face is not implemented for this class.");
}

void HyperReductionIntegrator::AppendPrecomputeCoefficients(
   const FiniteElementSpace *fes, DenseMatrix &basis, const SampleInfo &sample)
{
   mfem_error ("HyperReductionIntegrator::AppendPrecomputeCoefficients(...)\n"
               "is not implemented for this class,\n"
               "even though this class is set to be precomputable!\n");
}

void HyperReductionIntegrator::AddAssembleVector_Fast(
   const int s, const double qw, ElementTransformation &T, const IntegrationPoint &ip, const Vector &x, Vector &y) const
{
   mfem_error ("HyperReductionIntegrator::AddAssembleVector_Fast(...)\n"
               "is not implemented for this class,\n"
               "even though this class is set to be precomputable!\n");
}

void HyperReductionIntegrator::AddAssembleVector_Fast(
   const int s, const double qw, FaceElementTransformations &T, const IntegrationPoint &ip, const Vector &x, Vector &y) const
{
   mfem_error ("HyperReductionIntegrator::AddAssembleVector_Fast(...)\n"
               "is not implemented for this class,\n"
               "even though this class is set to be precomputable!\n");
}

void HyperReductionIntegrator::AddAssembleGrad_Fast(
   const int s, const double qw, ElementTransformation &T, const IntegrationPoint &ip, const Vector &x, DenseMatrix &jac) const
{
   mfem_error ("HyperReductionIntegrator::AddAssembleGrad_Fast(...)\n"
               "is not implemented for this class,\n"
               "even though this class is set to be precomputable!\n");
}

void HyperReductionIntegrator::AddAssembleGrad_Fast(
   const int s, const double qw, FaceElementTransformations &T, const IntegrationPoint &ip, const Vector &x, DenseMatrix &jac) const
{
   mfem_error ("HyperReductionIntegrator::AddAssembleGrad_Fast(...)\n"
               "is not implemented for this class,\n"
               "even though this class is set to be precomputable!\n");
}

void HyperReductionIntegrator::GetBasisElement(
   DenseMatrix &basis, const int col, const Array<int> vdofs, Vector &basis_el, DofTransformation *dof_trans)
{
   Vector tmp;
   basis.GetColumnReference(col, tmp);
   tmp.GetSubVector(vdofs, basis_el);   // this involves a copy.
   if (dof_trans) {dof_trans->InvTransformPrimal(basis_el); }
}

const IntegrationRule&
VectorConvectionTrilinearFormIntegrator::GetRule(const FiniteElement &fe,
                                                ElementTransformation &T)
{
   const int order = 2 * fe.GetOrder() + T.OrderGrad(&fe);
   return IntRules.Get(fe.GetGeomType(), order);
}

void VectorConvectionTrilinearFormIntegrator::AssembleElementVector(
   const FiniteElement &el,
   ElementTransformation &T,
   const Vector &elfun,
   Vector &elvect)
{
   const int nd = el.GetDof();
   dim = el.GetDim();

   shape.SetSize(nd);
   dshape.SetSize(nd, dim);
   elvect.SetSize(nd * dim);
   gradEF.SetSize(dim);

   EF.UseExternalData(elfun.GetData(), nd, dim);
   ELV.UseExternalData(elvect.GetData(), nd, dim);

   Vector vec1(dim), vec2(dim);
   const IntegrationRule *ir = IntRule ? IntRule : &GetRule(el, T);
   ELV = 0.0;
   for (int i = 0; i < ir->GetNPoints(); i++)
   {
      const IntegrationPoint &ip = ir->IntPoint(i);

      // // NOTE: this is for a test of AssembleQuadratureVector,
      // //       which should return the equivalent answer.
      // Vector tmp(nd * dim);
      // AssembleQuadratureVector(el, T, ip, ip.weight, elfun, tmp);
      // elvect += tmp;
      // continue;

      T.SetIntPoint(&ip);
      el.CalcShape(ip, shape);
      el.CalcPhysDShape(T, dshape);
      double w = ip.weight * T.Weight();
      if (Q) { w *= Q->Eval(T, ip); }

      MultAtB(EF, dshape, gradEF);
      if (vQ)
         vQ->Eval(vec1, T, ip);
      else
         EF.MultTranspose(shape, vec1);
      gradEF.Mult(vec1, vec2);
      vec2 *= w;
      AddMultVWt(shape, vec2, ELV);
   }
}

// void VectorConvectionTrilinearFormIntegrator::AssembleElementQuadrature(
//    const FiniteElement &el,
//    ElementTransformation &T,
//    const Vector &eltest,
//    DenseMatrix &elquad)
// {
//    const int nd = el.GetDof();
//    const IntegrationRule *ir = IntRule ? IntRule : &GetRule(el, T);
//    const int nq = ir->GetNPoints();
//    dim = el.GetDim();

//    shape.SetSize(nd);
//    dshape.SetSize(nd, dim);
//    elquad.SetSize(nd * dim, nq);
//    gradEF.SetSize(dim);

//    EF.UseExternalData(eltest.GetData(), nd, dim);

//    Vector vec1(dim), vec2(dim), vec_tr(dim);

//    for (int i = 0; i < ir->GetNPoints(); i++)
//    {
//       ELV.UseExternalData(elquad.GetColumn(i), nd, dim);

//       const IntegrationPoint &ip = ir->IntPoint(i);
//       T.SetIntPoint(&ip);
//       el.CalcShape(ip, shape);
//       el.CalcPhysDShape(T, dshape);
//       double w = ip.weight * T.Weight();
//       if (Q) { w *= Q->Eval(T, ip); }

//       MultAtB(EF, dshape, gradEF);
//       if (vQ)
//          vQ->Eval(vec1, T, ip);
//       else
//          EF.MultTranspose(shape, vec1);
//       gradEF.Mult(vec1, vec2);
//       vec2 *= w;

//       MultVWt(shape, vec2, ELV);
//    }
// }

void VectorConvectionTrilinearFormIntegrator::AssembleQuadratureVector(
   const FiniteElement &el,
   ElementTransformation &T,
   const IntegrationPoint &ip,
   const double &iw,
   const Vector &eltest,
   Vector &elquad)
{
   const int nd = el.GetDof();
   dim = el.GetDim();

   shape.SetSize(nd);
   dshape.SetSize(nd, dim);
   elquad.SetSize(nd * dim);
   gradEF.SetSize(dim);

   EF.UseExternalData(eltest.GetData(), nd, dim);
   ELV.UseExternalData(elquad.GetData(), nd, dim);

   Vector vec1(dim), vec2(dim), vec_tr(dim);

   // const IntegrationPoint &ip = ir->IntPoint(i);
   T.SetIntPoint(&ip);
   el.CalcShape(ip, shape);
   el.CalcPhysDShape(T, dshape);
   // double w = ip.weight * T.Weight();
   double w = iw * T.Weight();
   if (Q) { w *= Q->Eval(T, ip); }

   MultAtB(EF, dshape, gradEF);
   if (vQ)
      vQ->Eval(vec1, T, ip);
   else
      EF.MultTranspose(shape, vec1);
   gradEF.Mult(vec1, vec2);
   vec2 *= w;

   MultVWt(shape, vec2, ELV);
}

void VectorConvectionTrilinearFormIntegrator::AssembleElementGrad(
   const FiniteElement &el,
   ElementTransformation &trans,
   const Vector &elfun,
   DenseMatrix &elmat)
{
   const int nd = el.GetDof();
   dim = el.GetDim();

   shape.SetSize(nd);
   dshape.SetSize(nd, dim);
   dshapex.SetSize(nd, dim);
   elmat.SetSize(nd * dim);
   elmat_comp.SetSize(nd);
   gradEF.SetSize(dim);

   EF.UseExternalData(elfun.GetData(), nd, dim);

   double w;
   Vector vec1(dim), vec2(dim), vec3(nd);

   const IntegrationRule *ir = IntRule ? IntRule : &GetRule(el, trans);

   elmat = 0.0;
   for (int i = 0; i < ir->GetNPoints(); i++)
   {
      const IntegrationPoint &ip = ir->IntPoint(i);
      trans.SetIntPoint(&ip);

      el.CalcShape(ip, shape);
      el.CalcDShape(ip, dshape);

      Mult(dshape, trans.InverseJacobian(), dshapex);

      w = ip.weight;

      if (Q)
      {
         w *= Q->Eval(trans, ip);
      }

      MultAtB(EF, dshapex, gradEF);
      EF.MultTranspose(shape, vec1);

      trans.AdjugateJacobian().Mult(vec1, vec2);

      vec2 *= w;
      dshape.Mult(vec2, vec3);
      MultVWt(shape, vec3, elmat_comp);

      for (int ii = 0; ii < dim; ii++)
      {
         elmat.AddMatrix(elmat_comp, ii * nd, ii * nd);
      }

      MultVVt(shape, elmat_comp);
      w = ip.weight * trans.Weight();
      if (Q)
      {
         w *= Q->Eval(trans, ip);
      }
      for (int ii = 0; ii < dim; ii++)
      {
         for (int jj = 0; jj < dim; jj++)
         {
            elmat.AddMatrix(w * gradEF(ii, jj), elmat_comp, ii * nd, jj * nd);
         }
      }
   }
}

void VectorConvectionTrilinearFormIntegrator::AssembleQuadratureGrad(
   const FiniteElement &el,
   ElementTransformation &trans,
   const IntegrationPoint &ip,
   const double &iw,
   const Vector &elfun,
   DenseMatrix &elmat)
{
   const int nd = el.GetDof();
   dim = el.GetDim();

   shape.SetSize(nd);
   dshape.SetSize(nd, dim);
   dshapex.SetSize(nd, dim);
   elmat.SetSize(nd * dim);
   elmat_comp.SetSize(nd);
   gradEF.SetSize(dim);

   EF.UseExternalData(elfun.GetData(), nd, dim);

   double w;
   Vector vec1(dim), vec2(dim), vec3(nd);

   elmat = 0.0;
   trans.SetIntPoint(&ip);

   el.CalcShape(ip, shape);
   el.CalcDShape(ip, dshape);

   Mult(dshape, trans.InverseJacobian(), dshapex);

   // w = ip.weight;
   w = iw;

   if (Q)
   {
      w *= Q->Eval(trans, ip);
   }

   MultAtB(EF, dshapex, gradEF);
   EF.MultTranspose(shape, vec1);

   trans.AdjugateJacobian().Mult(vec1, vec2);

   vec2 *= w;
   dshape.Mult(vec2, vec3);
   MultVWt(shape, vec3, elmat_comp);

   for (int ii = 0; ii < dim; ii++)
   {
      elmat.AddMatrix(elmat_comp, ii * nd, ii * nd);
   }

   MultVVt(shape, elmat_comp);
   // w = ip.weight * trans.Weight();
   w = iw * trans.Weight();
   if (Q)
   {
      w *= Q->Eval(trans, ip);
   }
   for (int ii = 0; ii < dim; ii++)
   {
      for (int jj = 0; jj < dim; jj++)
      {
         elmat.AddMatrix(w * gradEF(ii, jj), elmat_comp, ii * nd, jj * nd);
      }
   }
}

void VectorConvectionTrilinearFormIntegrator::AppendPrecomputeCoefficients(
   const FiniteElementSpace *fes, DenseMatrix &basis, const SampleInfo &sample)
{
   const int nbasis = basis.NumCols();
   // Not all nonlinear form integrators can have tensors as coefficients.
   // This is the special case of polynominally nonlinear operator.
   // For more general nonlinear operators, probably shape/dshape have to be stored.
   DenseTensor *elten = new DenseTensor(nbasis, nbasis, nbasis);

   const int el = sample.el;
   const FiniteElement *fe = fes->GetFE(el);
   Array<int> vdofs;
   // TODO(kevin): not exactly sure what doftrans impacts.
   DofTransformation *doftrans = fes->GetElementVDofs(el, vdofs);
   ElementTransformation *T = fes->GetElementTransformation(el);
   const IntegrationRule *ir = IntRule ? IntRule : &GetRule(*fe, *T);
   const IntegrationPoint &ip = ir->IntPoint(sample.qp);

   // double w = ip.weight * T.Weight();
   double w = T->Weight();  // quadrature weight will be multipled in Mult step.
   // CoefficientFunction will be multiplied at Mult step.
   // if (Q) { w *= Q->Eval(*T, ip); }

   const int nd = fe->GetDof();
   dim = fe->GetDim();

   shape.SetSize(nd);
   dshape.SetSize(nd, dim);
   gradEF.SetSize(dim);

   T->SetIntPoint(&ip);
   fe->CalcShape(ip, shape);
   fe->CalcPhysDShape(*T, dshape);

   Vector vec1(dim), vec2(dim);
   Vector vec3(nd * dim);
   elmat_comp.UseExternalData(vec3.GetData(), nd, dim);
   Vector basis_i, basis_j, basis_k;

   for (int i = 0; i < nbasis; i++)
   {
      GetBasisElement(basis, i, vdofs, basis_i, doftrans);
      EF.UseExternalData(basis_i.GetData(), nd, dim);
      EF.MultTranspose(shape, vec1);

      for (int j = 0; j < nbasis; j++)
      {
         GetBasisElement(basis, j, vdofs, basis_j, doftrans);
         ELV.UseExternalData(basis_j.GetData(), nd, dim);
         MultAtB(ELV, dshape, gradEF);
         gradEF.Mult(vec1, vec2);
         vec2 *= w;
         MultVWt(shape, vec2, elmat_comp);
         if (doftrans) { doftrans->TransformDual(vec3); }

         for (int k = 0; k < nbasis; k++)
         {
            GetBasisElement(basis, k, vdofs, basis_k);   // doftrans is already applied above for test function.

            (*elten)(i, j, k) = (basis_k * vec3);
         }  // for (int k = 0; k < nbasis; k++)
      }  // for (int j = 0; j < nbasis; j++)
   }  // for (int i = 0; i < nbasis; i++)

   coeffs.Append(elten);
}

void VectorConvectionTrilinearFormIntegrator::AddAssembleVector_Fast(
   const int s, const double qw, ElementTransformation &T, const IntegrationPoint &ip, const Vector &x, Vector &y) const
{
   const DenseTensor *tensor = coeffs[s];
   Vector tmp(tensor->SizeK());
   y.SetSize(tensor->SizeK());

   double w = qw;
   if (Q) 
   {
      T.SetIntPoint(&ip);
      w *= Q->Eval(T, ip);
   }
   TensorAddScaledContract(*tensor, w, x, x, y);
}

void VectorConvectionTrilinearFormIntegrator::AddAssembleGrad_Fast(
   const int s, const double qw, ElementTransformation &T, const IntegrationPoint &ip, const Vector &x, DenseMatrix &jac) const
{
   const DenseTensor *tensor = coeffs[s];
   double w = qw;
   if (Q) 
   {
      T.SetIntPoint(&ip);
      w *= Q->Eval(T, ip);
   }
   TensorAddScaledMultTranspose(*tensor, w, x, 0, jac);
   TensorAddScaledMultTranspose(*tensor, w, x, 1, jac);
}

}
