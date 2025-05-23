// Copyright 2023 Lawrence Livermore National Security, LLC. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: MIT

#include<gtest/gtest.h>
#include "hyperreduction_integ.hpp"
#include "interfaceinteg.hpp"
#include "rom_nonlinearform.hpp"
#include "etc.hpp"

using namespace std;
using namespace mfem;

static const double threshold = 1.0e-12;
static const double grad_thre = 1.0e-7;

/**
 * Simple smoke test to make sure Google Test is properly linked
 */
TEST(GoogleTestFramework, GoogleTestFrameworkFound) {
   SUCCEED();
}

TEST(ROMNonlinearForm, VectorConvectionTrilinearFormIntegrator)
{
   Mesh *mesh = new Mesh("meshes/test.4x4.mesh");
   const int dim = mesh->Dimension();
   const int order = UniformRandom(1, 3);

   FiniteElementCollection *h1_coll(new H1_FECollection(order, dim));
   FiniteElementSpace *fes(new FiniteElementSpace(mesh, h1_coll, dim));
   const int ndofs = fes->GetTrueVSize();

   const int num_basis = 10;
   // a fictitious basis.
   DenseMatrix basis(ndofs, num_basis);
   for (int i = 0; i < ndofs; i++)
      for (int j = 0; j < num_basis; j++)
         basis(i, j) = UniformRandom();

   IntegrationRule ir = IntRules.Get(fes->GetFE(0)->GetGeomType(),
                                    (int)(ceil(1.5 * (2 * fes->GetMaxElementOrder() - 1))));
   ConstantCoefficient pi(3.141592);
   auto *integ1 = new VectorConvectionTrilinearFormIntegrator(pi);
   integ1->SetIntRule(&ir);
   auto *integ2 = new VectorConvectionTrilinearFormIntegrator(pi);
   integ2->SetIntRule(&ir);

   NonlinearForm *nform(new NonlinearForm(fes));
   nform->AddDomainIntegrator(integ1);

   ROMNonlinearForm *rform(new ROMNonlinearForm(num_basis, fes));
   rform->AddDomainIntegrator(integ2);
   rform->SetBasis(basis);

   // we set the full elements/quadrature points,
   // so that the resulting vector is equilvalent to FOM.
   const int nqe = ir.GetNPoints();
   const int ne = fes->GetNE();
   Array<double> const& w_el = ir.GetWeights();
   Array<SampleInfo> samples(ne * nqe);
   for (int e = 0, idx = 0; e < ne; e++)
      for (int q = 0; q < nqe; q++, idx++)
      {
         samples[idx].el = e;
         samples[idx].qp = q;
         samples[idx].qw = w_el[q];
      }
   rform->UpdateDomainIntegratorSampling(0, samples);

   Vector rom_u(num_basis), u(fes->GetTrueVSize());
   for (int k = 0; k < rom_u.Size(); k++)
      rom_u(k) = UniformRandom();

   basis.Mult(rom_u, u);

   Vector rom_y(num_basis), y(fes->GetTrueVSize()), Pty(num_basis);
   nform->Mult(u, y);
   basis.MultTranspose(y, Pty);
   rform->Mult(rom_u, rom_y);

   for (int k = 0; k < rom_y.Size(); k++)
      EXPECT_NEAR(rom_y(k), Pty(k), threshold);

   delete mesh;
   delete h1_coll;
   delete fes;
   delete nform;
   delete rform;
   return;
}

TEST(ROMNonlinearForm, IncompressibleInviscidFluxNLFIntegrator)
{
   Mesh *mesh = new Mesh("meshes/test.4x4.mesh");
   const int dim = mesh->Dimension();
   const int order = UniformRandom(1, 3);

   FiniteElementCollection *dg_coll(new DG_FECollection(order, dim));
   FiniteElementSpace *fes(new FiniteElementSpace(mesh, dg_coll, dim));
   const int ndofs = fes->GetTrueVSize();

   const int num_basis = 10;
   // a fictitious basis.
   DenseMatrix basis(ndofs, num_basis);
   for (int i = 0; i < ndofs; i++)
      for (int j = 0; j < num_basis; j++)
         basis(i, j) = UniformRandom();

   IntegrationRule ir = IntRules.Get(fes->GetFE(0)->GetGeomType(),
                                    (int)(ceil(1.5 * (2 * fes->GetMaxElementOrder() - 1))));
   ConstantCoefficient pi(3.141592);
   auto *integ1 = new IncompressibleInviscidFluxNLFIntegrator(pi);
   integ1->SetIntRule(&ir);
   auto *integ2 = new IncompressibleInviscidFluxNLFIntegrator(pi);
   integ2->SetIntRule(&ir);

   NonlinearForm *nform(new NonlinearForm(fes));
   nform->AddDomainIntegrator(integ1);

   ROMNonlinearForm *rform(new ROMNonlinearForm(num_basis, fes));
   rform->AddDomainIntegrator(integ2);
   rform->SetBasis(basis);

   // we set the full elements/quadrature points,
   // so that the resulting vector is equilvalent to FOM.
   const int nqe = ir.GetNPoints();
   const int ne = fes->GetNE();
   Array<double> const& w_el = ir.GetWeights();
   Array<SampleInfo> samples(ne * nqe);
   for (int e = 0, idx = 0; e < ne; e++)
      for (int q = 0; q < nqe; q++, idx++)
      {
         samples[idx].el = e;
         samples[idx].qp = q;
         samples[idx].qw = w_el[q];
      }
   rform->UpdateDomainIntegratorSampling(0, samples);

   Vector rom_u(num_basis), u(fes->GetTrueVSize());
   for (int k = 0; k < rom_u.Size(); k++)
      rom_u(k) = UniformRandom();

   basis.Mult(rom_u, u);

   Vector rom_y(num_basis), y(fes->GetTrueVSize()), Pty(num_basis);
   nform->Mult(u, y);
   basis.MultTranspose(y, Pty);
   rform->Mult(rom_u, rom_y);

   for (int k = 0; k < rom_y.Size(); k++)
      EXPECT_NEAR(rom_y(k), Pty(k), threshold);

   delete mesh;
   delete dg_coll;
   delete fes;
   delete nform;
   delete rform;
   return;
}

TEST(ROMNonlinearForm, DGLaxFriedrichsFluxIntegrator)
{
   Mesh *mesh = new Mesh("meshes/test.4x4.mesh");
   const int dim = mesh->Dimension();
   const int order = UniformRandom(1, 3);

   FiniteElementCollection *dg_coll(new DG_FECollection(order, dim));
   FiniteElementSpace *fes(new FiniteElementSpace(mesh, dg_coll, dim));
   const int ndofs = fes->GetTrueVSize();

   const int num_basis = 10;
   // a fictitious basis.
   DenseMatrix basis(ndofs, num_basis);
   for (int i = 0; i < ndofs; i++)
      for (int j = 0; j < num_basis; j++)
         basis(i, j) = UniformRandom();

   IntegrationRule ir = IntRules.Get(fes->GetFE(0)->GetGeomType(),
                                    (int)(ceil(1.5 * (2 * fes->GetMaxElementOrder() - 1))));
   ConstantCoefficient pi(3.141592);
   auto *integ1 = new DGLaxFriedrichsFluxIntegrator(pi);
   integ1->SetIntRule(&ir);
   auto *integ2 = new DGLaxFriedrichsFluxIntegrator(pi);
   integ2->SetIntRule(&ir);

   NonlinearForm *nform(new NonlinearForm(fes));
   nform->AddInteriorFaceIntegrator(integ1);

   ROMNonlinearForm *rform(new ROMNonlinearForm(num_basis, fes));
   rform->AddInteriorFaceIntegrator(integ2);
   rform->SetBasis(basis);

   // we set the full elements/quadrature points,
   // so that the resulting vector is equilvalent to FOM.
   FaceElementTransformations *tr;
   const int nqe = ir.GetNPoints();
   const int nf = mesh->GetNumFaces();
   Array<double> const& w_el = ir.GetWeights();
   Array<SampleInfo> samples(0);
   for (int f = 0; f < nf; f++)
   {
      tr = mesh->GetInteriorFaceTransformations(f);
      if (tr == NULL) continue;

      for (int q = 0; q < nqe; q++)
      {
         samples.Append({.el = f, .qp = q, .qw = w_el[q]});
      }
   }
   rform->UpdateInteriorFaceIntegratorSampling(0, samples);

   Vector rom_u(num_basis), u(fes->GetTrueVSize());
   for (int k = 0; k < rom_u.Size(); k++)
      rom_u(k) = UniformRandom();

   basis.Mult(rom_u, u);

   Vector rom_y(num_basis), y(fes->GetTrueVSize()), Pty(num_basis);
   nform->Mult(u, y);
   basis.MultTranspose(y, Pty);
   rform->Mult(rom_u, rom_y);

   for (int k = 0; k < rom_y.Size(); k++)
      EXPECT_NEAR(rom_y(k), Pty(k), threshold);

   delete mesh;
   delete dg_coll;
   delete fes;
   delete nform;
   delete rform;
   return;
}

TEST(ROMNonlinearForm_gradient, VectorConvectionTrilinearFormIntegrator)
{
   Mesh *mesh = new Mesh("meshes/test.4x4.mesh");
   const int dim = mesh->Dimension();
   const int order = UniformRandom(1, 3);

   FiniteElementCollection *h1_coll(new H1_FECollection(order, dim));
   FiniteElementSpace *fes(new FiniteElementSpace(mesh, h1_coll, dim));
   const int ndofs = fes->GetTrueVSize();

   const int num_basis = 10;
   // a fictitious basis.
   DenseMatrix basis(ndofs, num_basis);
   for (int i = 0; i < ndofs; i++)
      for (int j = 0; j < num_basis; j++)
         basis(i, j) = UniformRandom();

   IntegrationRule ir = IntRules.Get(fes->GetFE(0)->GetGeomType(),
                                    (int)(ceil(1.5 * (2 * fes->GetMaxElementOrder() - 1))));
   ConstantCoefficient pi(3.141592);
   auto *integ = new VectorConvectionTrilinearFormIntegrator(pi);
   integ->SetIntRule(&ir);

   ROMNonlinearForm *rform(new ROMNonlinearForm(num_basis, fes));
   rform->AddDomainIntegrator(integ);
   rform->SetBasis(basis);

   // we set the full elements/quadrature points,
   // so that the resulting vector is equilvalent to FOM.
   const int nsample = UniformRandom(15, 20);
   const int nqe = ir.GetNPoints();
   const int ne = fes->GetNE();
   Array<double> const& w_el = ir.GetWeights();
   Array<SampleInfo> samples(nsample);
   for (int s = 0; s < nsample; s++)
   {
      samples[s].el = UniformRandom(0, ne-1);
      samples[s].qp = UniformRandom(0, nqe-1);
      samples[s].qw = UniformRandom();
   }
   rform->UpdateDomainIntegratorSampling(0, samples);

   Vector rom_u(num_basis);
   for (int k = 0; k < rom_u.Size(); k++)
      rom_u(k) = UniformRandom();

   Vector rom_y(num_basis);
   rform->Mult(rom_u, rom_y);
   DenseMatrix *jac = dynamic_cast<DenseMatrix *>(&(rform->GetGradient(rom_u)));

   double J0 = 0.5 * (rom_y * rom_y);
   Vector grad(num_basis);
   jac->MultTranspose(rom_y, grad);
   double gg = sqrt(grad * grad);
   printf("J0: %.15E\n", J0);
   printf("grad: %.15E\n", gg);

   Vector du(grad);
   du /= gg;

   Vector rom_y1(num_basis);
   const int Nk = 40;
   double error1 = 1e100, error;
   printf("amp\tJ1\tdJdx\terror\n");
   for (int k = 0; k < Nk; k++)
   {
      double amp = pow(10.0, -0.25 * k);
      Vector rom_u1(rom_u);
      rom_u1.Add(amp, du);

      rform->Mult(rom_u1, rom_y1);
      double J1 = 0.5 * (rom_y1 * rom_y1);
      double dJdx = (J1 - J0) / amp;
      error = abs(dJdx - gg) / gg;

      printf("%.5E\t%.5E\t%.5E\t%.5E\n", amp, J1, dJdx, error);
      if (error >= error1)
         break;
      error1 = error;
   }
   EXPECT_TRUE(min(error, error1) < grad_thre);

   delete mesh;
   delete h1_coll;
   delete fes;
   delete rform;
   return;
}

TEST(ROMNonlinearForm_gradient, IncompressibleInviscidFluxNLFIntegrator)
{
   Mesh *mesh = new Mesh("meshes/test.4x4.mesh");
   const int dim = mesh->Dimension();
   const int order = UniformRandom(1, 3);

   FiniteElementCollection *dg_coll(new DG_FECollection(order, dim));
   FiniteElementSpace *fes(new FiniteElementSpace(mesh, dg_coll, dim));
   const int ndofs = fes->GetTrueVSize();

   const int num_basis = 10;
   // a fictitious basis.
   DenseMatrix basis(ndofs, num_basis);
   for (int i = 0; i < ndofs; i++)
      for (int j = 0; j < num_basis; j++)
         basis(i, j) = UniformRandom();

   IntegrationRule ir = IntRules.Get(fes->GetFE(0)->GetGeomType(),
                                    (int)(ceil(1.5 * (2 * fes->GetMaxElementOrder() - 1))));
   ConstantCoefficient pi(3.141592);
   auto *integ = new IncompressibleInviscidFluxNLFIntegrator(pi);
   integ->SetIntRule(&ir);

   ROMNonlinearForm *rform(new ROMNonlinearForm(num_basis, fes));
   rform->AddDomainIntegrator(integ);
   rform->SetBasis(basis);

   // we set some random quadrature points/weights.
   const int nsample = UniformRandom(15, 20);
   const int nqe = ir.GetNPoints();
   const int ne = fes->GetNE();
   Array<double> const& w_el = ir.GetWeights();
   Array<SampleInfo> samples(nsample);
   for (int s = 0; s < nsample; s++)
   {
      samples[s].el = UniformRandom(0, ne-1);
      samples[s].qp = UniformRandom(0, nqe-1);
      samples[s].qw = UniformRandom();
   }
   rform->UpdateDomainIntegratorSampling(0, samples);

   Vector rom_u(num_basis);
   for (int k = 0; k < rom_u.Size(); k++)
      rom_u(k) = UniformRandom();

   Vector rom_y(num_basis);
   rform->Mult(rom_u, rom_y);
   DenseMatrix *jac = dynamic_cast<DenseMatrix *>(&(rform->GetGradient(rom_u)));

   double J0 = 0.5 * (rom_y * rom_y);
   Vector grad(num_basis);
   jac->MultTranspose(rom_y, grad);
   double gg = sqrt(grad * grad);
   printf("J0: %.15E\n", J0);
   printf("grad: %.15E\n", gg);

   Vector du(grad);
   du /= gg;

   Vector rom_y1(num_basis);
   const int Nk = 40;
   double error1 = 1e100, error;
   printf("amp\tJ1\tdJdx\terror\n");
   for (int k = 0; k < Nk; k++)
   {
      double amp = pow(10.0, -0.25 * k);
      Vector rom_u1(rom_u);
      rom_u1.Add(amp, du);

      rform->Mult(rom_u1, rom_y1);
      double J1 = 0.5 * (rom_y1 * rom_y1);
      double dJdx = (J1 - J0) / amp;
      error = abs(dJdx - gg) / gg;

      printf("%.5E\t%.5E\t%.5E\t%.5E\n", amp, J1, dJdx, error);
      if (error >= error1)
         break;
      error1 = error;
   }
   EXPECT_TRUE(min(error, error1) < grad_thre);

   delete mesh;
   delete dg_coll;
   delete fes;
   delete rform;
   return;
}

TEST(ROMNonlinearForm_gradient, DGLaxFriedrichsFluxIntegrator)
{
   Mesh *mesh = new Mesh("meshes/test.4x4.mesh");
   const int dim = mesh->Dimension();
   const int order = UniformRandom(1, 3);

   FiniteElementCollection *dg_coll(new DG_FECollection(order, dim));
   FiniteElementSpace *fes(new FiniteElementSpace(mesh, dg_coll, dim));
   const int ndofs = fes->GetTrueVSize();

   const int num_basis = 10;
   // a fictitious basis.
   DenseMatrix basis(ndofs, num_basis);
   for (int i = 0; i < ndofs; i++)
      for (int j = 0; j < num_basis; j++)
         basis(i, j) = UniformRandom();

   IntegrationRule ir = IntRules.Get(fes->GetFE(0)->GetGeomType(),
                                    (int)(ceil(1.5 * (2 * fes->GetMaxElementOrder() - 1))));
   ConstantCoefficient pi(3.141592);
   auto *integ = new DGLaxFriedrichsFluxIntegrator(pi);
   integ->SetIntRule(&ir);

   ROMNonlinearForm *rform(new ROMNonlinearForm(num_basis, fes));
   rform->AddInteriorFaceIntegrator(integ);
   rform->SetBasis(basis);

   // we set some random quadrature points/weights.
   const int nsample = UniformRandom(15, 20);
   const int nqe = ir.GetNPoints();
   const int nf = mesh->GetNumFaces();
   Array<double> const& w_el = ir.GetWeights();
   Array<SampleInfo> samples(nsample);

   int s = 0;
   FaceElementTransformations *tr;
   while (s < nsample)
   {
      int f = UniformRandom(0, nf-1);
      tr = mesh->GetInteriorFaceTransformations(f);
      if (tr == NULL) continue;

      samples[s].el = f;
      samples[s].qp = UniformRandom(0, nqe-1);
      samples[s].qw = UniformRandom();

      s++;
   }
   rform->UpdateInteriorFaceIntegratorSampling(0, samples);

   Vector rom_u(num_basis);
   for (int k = 0; k < rom_u.Size(); k++)
      rom_u(k) = UniformRandom();

   Vector rom_y(num_basis);
   rform->Mult(rom_u, rom_y);
   DenseMatrix *jac = dynamic_cast<DenseMatrix *>(&(rform->GetGradient(rom_u)));

   double J0 = 0.5 * (rom_y * rom_y);
   Vector grad(num_basis);
   jac->MultTranspose(rom_y, grad);
   double gg = sqrt(grad * grad);
   printf("J0: %.15E\n", J0);
   printf("grad: %.15E\n", gg);

   Vector du(grad);
   du /= gg;

   Vector rom_y1(num_basis);
   const int Nk = 40;
   double error1 = 1e100, error;
   printf("amp\tJ1\tdJdx\terror\n");
   for (int k = 0; k < Nk; k++)
   {
      double amp = pow(10.0, -0.25 * k);
      Vector rom_u1(rom_u);
      rom_u1.Add(amp, du);

      rform->Mult(rom_u1, rom_y1);
      double J1 = 0.5 * (rom_y1 * rom_y1);
      double dJdx = (J1 - J0) / amp;
      error = abs(dJdx - gg) / gg;

      printf("%.5E\t%.5E\t%.5E\t%.5E\n", amp, J1, dJdx, error);
      if (error >= error1)
         break;
      error1 = error;
   }
   EXPECT_TRUE(min(error, error1) < grad_thre);

   delete mesh;
   delete dg_coll;
   delete fes;
   delete rform;
   return;
}

TEST(ROMNonlinearForm_fast, VectorConvectionTrilinearFormIntegrator)
{
   Mesh *mesh = new Mesh("meshes/test.4x4.mesh");
   const int dim = mesh->Dimension();
   const int order = UniformRandom(1, 3);

   FiniteElementCollection *h1_coll(new H1_FECollection(order, dim));
   FiniteElementSpace *fes(new FiniteElementSpace(mesh, h1_coll, dim));
   const int ndofs = fes->GetTrueVSize();

   const int num_basis = 10;
   // a fictitious basis.
   DenseMatrix basis(ndofs, num_basis);
   for (int i = 0; i < ndofs; i++)
      for (int j = 0; j < num_basis; j++)
         basis(i, j) = UniformRandom();

   IntegrationRule ir = IntRules.Get(fes->GetFE(0)->GetGeomType(),
                                    (int)(ceil(1.5 * (2 * fes->GetMaxElementOrder() - 1))));
   ConstantCoefficient pi(3.141592);
   auto *integ = new VectorConvectionTrilinearFormIntegrator(pi);
   integ->SetIntRule(&ir);

   ROMNonlinearForm *rform(new ROMNonlinearForm(num_basis, fes));
   rform->AddDomainIntegrator(integ);
   rform->SetBasis(basis);

   // we set the full elements/quadrature points,
   // so that the resulting vector is equilvalent to FOM.
   const int nsample = UniformRandom(15, 20);
   const int nqe = ir.GetNPoints();
   const int ne = fes->GetNE();
   Array<double> const& w_el = ir.GetWeights();
   Array<SampleInfo> samples(nsample);
   for (int s = 0; s < nsample; s++)
   {
      samples[s].el = UniformRandom(0, ne-1);
      samples[s].qp = UniformRandom(0, nqe-1);
      samples[s].qw = UniformRandom();
   }
   rform->UpdateDomainIntegratorSampling(0, samples);
   rform->PrecomputeCoefficients();

   Vector rom_u(num_basis);
   for (int k = 0; k < rom_u.Size(); k++)
      rom_u(k) = UniformRandom();

   Vector rom_y(num_basis), rom_yfast(num_basis);
   rform->Mult(rom_u, rom_y);

   rform->SetPrecomputeMode(true);
   rform->Mult(rom_u, rom_yfast);
   for (int k = 0; k < rom_y.Size(); k++)
      EXPECT_NEAR(rom_y(k), rom_yfast(k), threshold);

   DenseMatrix *jac = dynamic_cast<DenseMatrix *>(&(rform->GetGradient(rom_u)));

   double J0 = 0.5 * (rom_y * rom_y);
   Vector grad(num_basis);
   jac->MultTranspose(rom_y, grad);
   double gg = sqrt(grad * grad);
   printf("J0: %.15E\n", J0);
   printf("grad: %.15E\n", gg);

   Vector du(grad);
   du /= gg;

   Vector rom_y1(num_basis);
   const int Nk = 40;
   double error1 = 1e100, error;
   printf("amp\tJ1\tdJdx\terror\n");
   for (int k = 0; k < Nk; k++)
   {
      double amp = pow(10.0, -0.25 * k);
      Vector rom_u1(rom_u);
      rom_u1.Add(amp, du);

      rform->Mult(rom_u1, rom_y1);
      double J1 = 0.5 * (rom_y1 * rom_y1);
      double dJdx = (J1 - J0) / amp;
      error = abs(dJdx - gg) / gg;

      printf("%.5E\t%.5E\t%.5E\t%.5E\n", amp, J1, dJdx, error);
      if (error >= error1)
         break;
      error1 = error;
   }
   EXPECT_TRUE(min(error, error1) < grad_thre);

   delete mesh;
   delete h1_coll;
   delete fes;
   delete rform;
   return;
}

TEST(ROMNonlinearForm_fast, IncompressibleInviscidFluxNLFIntegrator)
{
   Mesh *mesh = new Mesh("meshes/test.4x4.mesh");
   const int dim = mesh->Dimension();
   const int order = UniformRandom(1, 3);

   FiniteElementCollection *dg_coll(new DG_FECollection(order, dim));
   FiniteElementSpace *fes(new FiniteElementSpace(mesh, dg_coll, dim));
   const int ndofs = fes->GetTrueVSize();

   const int num_basis = 10;
   // a fictitious basis.
   DenseMatrix basis(ndofs, num_basis);
   for (int i = 0; i < ndofs; i++)
      for (int j = 0; j < num_basis; j++)
         basis(i, j) = UniformRandom();

   IntegrationRule ir = IntRules.Get(fes->GetFE(0)->GetGeomType(),
                                    (int)(ceil(1.5 * (2 * fes->GetMaxElementOrder() - 1))));
   ConstantCoefficient pi(3.141592);
   auto *integ = new IncompressibleInviscidFluxNLFIntegrator(pi);
   integ->SetIntRule(&ir);

   ROMNonlinearForm *rform(new ROMNonlinearForm(num_basis, fes));
   rform->AddDomainIntegrator(integ);
   rform->SetBasis(basis);

   // we set the full elements/quadrature points,
   // so that the resulting vector is equilvalent to FOM.
   const int nsample = UniformRandom(15, 20);
   const int nqe = ir.GetNPoints();
   const int ne = fes->GetNE();
   Array<double> const& w_el = ir.GetWeights();
   Array<SampleInfo> samples(nsample);
   for (int s = 0; s < nsample; s++)
   {
      samples[s].el = UniformRandom(0, ne-1);
      samples[s].qp = UniformRandom(0, nqe-1);
      samples[s].qw = UniformRandom();
   }
   rform->UpdateDomainIntegratorSampling(0, samples);
   rform->PrecomputeCoefficients();

   Vector rom_u(num_basis);
   for (int k = 0; k < rom_u.Size(); k++)
      rom_u(k) = UniformRandom();

   Vector rom_y(num_basis), rom_yfast(num_basis);
   rform->Mult(rom_u, rom_y);

   rform->SetPrecomputeMode(true);
   rform->Mult(rom_u, rom_yfast);
   for (int k = 0; k < rom_y.Size(); k++)
      EXPECT_NEAR(rom_y(k), rom_yfast(k), threshold);

   DenseMatrix *jac = dynamic_cast<DenseMatrix *>(&(rform->GetGradient(rom_u)));

   double J0 = 0.5 * (rom_y * rom_y);
   Vector grad(num_basis);
   jac->MultTranspose(rom_y, grad);
   double gg = sqrt(grad * grad);
   printf("J0: %.15E\n", J0);
   printf("grad: %.15E\n", gg);

   Vector du(grad);
   du /= gg;

   Vector rom_y1(num_basis);
   const int Nk = 40;
   double error1 = 1e100, error;
   printf("amp\tJ1\tdJdx\terror\n");
   for (int k = 0; k < Nk; k++)
   {
      double amp = pow(10.0, -0.25 * k);
      Vector rom_u1(rom_u);
      rom_u1.Add(amp, du);

      rform->Mult(rom_u1, rom_y1);
      double J1 = 0.5 * (rom_y1 * rom_y1);
      double dJdx = (J1 - J0) / amp;
      error = abs(dJdx - gg) / gg;

      printf("%.5E\t%.5E\t%.5E\t%.5E\n", amp, J1, dJdx, error);
      if (error >= error1)
         break;
      error1 = error;
   }
   EXPECT_TRUE(min(error, error1) < grad_thre);

   delete mesh;
   delete dg_coll;
   delete fes;
   delete rform;
   return;
}

TEST(ROMNonlinearForm_fast, DGLaxFriedrichsFluxIntegrator)
{
   Mesh *mesh = new Mesh("meshes/test.4x4.mesh");
   const int dim = mesh->Dimension();
   const int order = UniformRandom(1, 3);

   FiniteElementCollection *dg_coll(new DG_FECollection(order, dim));
   FiniteElementSpace *fes(new FiniteElementSpace(mesh, dg_coll, dim));
   const int ndofs = fes->GetTrueVSize();

   const int num_basis = 10;
   // a fictitious basis.
   DenseMatrix basis(ndofs, num_basis);
   for (int i = 0; i < ndofs; i++)
      for (int j = 0; j < num_basis; j++)
         basis(i, j) = UniformRandom();

   // a simple choice for the integration order; is this OK?
   const IntegrationRule *ir = NULL;
   {
      const int int_order = (int) (ceil(1.5 * (2 * fes->GetMaxElementOrder() - 1)));
      FaceElementTransformations *T = NULL;
      for (int f = 0; f < mesh->GetNumFaces(); f++)
      {
         T = mesh->GetInteriorFaceTransformations(f);
         if (T != NULL)
            break;
      }
      ir = &IntRules.Get(T->GetGeometryType(), int_order);
   }
   assert(ir);

   ConstantCoefficient pi(3.141592);
   Vector ud(dim);
   for (int d = 0; d < dim; d++) ud(d) = 2.0 * UniformRandom() - 1.0;
   VectorConstantCoefficient ud_coeff(ud);
   auto *integ = new DGLaxFriedrichsFluxIntegrator(pi);
   auto *integ_bdr = new DGLaxFriedrichsFluxIntegrator(pi, &ud_coeff);
   integ->SetIntRule(ir);
   integ_bdr->SetIntRule(ir);

   ROMNonlinearForm *rform(new ROMNonlinearForm(num_basis, fes));
   rform->AddInteriorFaceIntegrator(integ);
   rform->AddBdrFaceIntegrator(integ_bdr);
   rform->SetBasis(basis);

   // we set the full elements/quadrature points,
   // so that the resulting vector is equilvalent to FOM.
   const int nsample = UniformRandom(15, 20);
   const int nqe = ir->GetNPoints();
   const int nf = mesh->GetNumFaces();
   const int nbe = fes->GetNBE();
   Array<double> const& w_el = ir->GetWeights();
   Array<SampleInfo> samples(nsample);

   FaceElementTransformations *tr;

   /* fill up interior face samples first */
   int s = 0;
   while (s < nsample)
   {
      int f = UniformRandom(0, nf-1);
      tr = mesh->GetInteriorFaceTransformations(f);
      if (tr == NULL) continue;

      samples[s].el = f;
      samples[s].qp = UniformRandom(0, nqe-1);
      samples[s].qw = UniformRandom();

      s++;
   }
   rform->UpdateInteriorFaceIntegratorSampling(0, samples);

   /* fill up boundary face samples */
   s = 0;
   while (s < nsample)
   {
      int be = UniformRandom(0, nbe-1);
      tr = mesh->GetBdrFaceTransformations(be);
      if (tr == NULL) continue;

      samples[s].el = be;
      samples[s].qp = UniformRandom(0, nqe-1);
      samples[s].qw = UniformRandom();

      s++;
   }
   rform->UpdateBdrFaceIntegratorSampling(0, samples);

   rform->PrecomputeCoefficients();

   Vector rom_u(num_basis);
   for (int k = 0; k < rom_u.Size(); k++)
      rom_u(k) = UniformRandom();

   Vector rom_y(num_basis), rom_yfast(num_basis);
   rform->Mult(rom_u, rom_y);

   rform->SetPrecomputeMode(true);
   rform->Mult(rom_u, rom_yfast);
   for (int k = 0; k < rom_y.Size(); k++)
      EXPECT_NEAR(rom_y(k), rom_yfast(k), threshold);

   DenseMatrix *jac = dynamic_cast<DenseMatrix *>(&(rform->GetGradient(rom_u)));

   double J0 = 0.5 * (rom_y * rom_y);
   Vector grad(num_basis);
   jac->MultTranspose(rom_y, grad);
   double gg = sqrt(grad * grad);
   printf("J0: %.15E\n", J0);
   printf("grad: %.15E\n", gg);

   Vector du(grad);
   du /= gg;

   Vector rom_y1(num_basis);
   const int Nk = 40;
   double error1 = 1e100, error;
   printf("amp\tJ1\tdJdx\terror\n");
   for (int k = 0; k < Nk; k++)
   {
      double amp = pow(10.0, -0.25 * k);
      Vector rom_u1(rom_u);
      rom_u1.Add(amp, du);

      rform->Mult(rom_u1, rom_y1);
      double J1 = 0.5 * (rom_y1 * rom_y1);
      double dJdx = (J1 - J0) / amp;
      error = abs(dJdx - gg) / gg;

      printf("%.5E\t%.5E\t%.5E\t%.5E\n", amp, J1, dJdx, error);
      if (error >= error1)
         break;
      error1 = error;
   }
   EXPECT_TRUE(min(error, error1) < grad_thre);

   delete mesh;
   delete dg_coll;
   delete fes;
   delete rform;
   return;
}

TEST(ROMNonlinearForm, SetupEQPSystemForDomainIntegrator)
{
   Mesh *mesh = new Mesh("meshes/test.4x4.mesh");
   const int dim = mesh->Dimension();
   const int order = UniformRandom(1, 3);

   FiniteElementCollection *h1_coll(new H1_FECollection(order, dim));
   FiniteElementSpace *fes(new FiniteElementSpace(mesh, h1_coll, dim));
   const int ndofs = fes->GetTrueVSize();
   const int num_snap = UniformRandom(3, 5);
   const int num_basis = num_snap;

   // a fictitious snapshots.
   DenseMatrix snapshots(ndofs, num_snap);
   for (int i = 0; i < ndofs; i++)
      for (int j = 0; j < num_snap; j++)
         snapshots(i, j) = 2.0 * UniformRandom() - 1.0;

   CAROM::Options options(ndofs, num_snap, 1, true);
   options.static_svd_preserve_snapshot = true;
   CAROM::BasisGenerator basis_generator(options, false, "test_basis");
   Vector snapshot(ndofs);
   for (int s = 0; s < num_snap; s++)
   {
      snapshots.GetColumnReference(s, snapshot);
      basis_generator.takeSample(snapshot.GetData());
   }
   basis_generator.endSamples();
   std::shared_ptr<const CAROM::Matrix> carom_snapshots = basis_generator.getSnapshotMatrix();
   std::shared_ptr<const CAROM::Matrix> carom_basis = basis_generator.getSpatialBasis();
   DenseMatrix basis(ndofs, num_basis);
   CAROM::CopyMatrix(*carom_basis, basis);

   IntegrationRule ir = IntRules.Get(fes->GetFE(0)->GetGeomType(),
                                    (int)(ceil(1.5 * (2 * fes->GetMaxElementOrder() - 1))));
   ConstantCoefficient pi(3.141592);
   auto *integ1 = new VectorConvectionTrilinearFormIntegrator(pi);
   integ1->SetIntRule(&ir);
   auto *integ2 = new VectorConvectionTrilinearFormIntegrator(pi);
   integ2->SetIntRule(&ir);

   NonlinearForm *nform(new NonlinearForm(fes));
   nform->AddDomainIntegrator(integ1);

   ROMNonlinearForm *rform(new ROMNonlinearForm(num_basis, fes));
   rform->AddDomainIntegrator(integ2);
   rform->SetBasis(basis);
   rform->SetPrecomputeMode(true);

   CAROM::Vector rhs1(num_snap * num_basis, false);
   CAROM::Vector rhs2(num_snap * num_basis, false);
   CAROM::Matrix Gt(1, 1, true);

   /* exact right-hand side by inner product of basis and fom vectors */
   Vector rhs_vec(ndofs), basis_col(ndofs);
   for (int s = 0; s < num_snap; s++)
   {
      snapshots.GetColumnReference(s, snapshot);

      nform->Mult(snapshot, rhs_vec);
      for (int b = 0; b < num_basis; b++)
      {
         basis.GetColumnReference(b, basis_col);
         rhs1(b + s * num_basis) = basis_col * rhs_vec;
      }
   }

   /*
      TODO(kevin): this is a boilerplate for parallel POD/EQP training.
      Will have to consider parallel-compatible test.
   */
   CAROM::Matrix carom_snapshots_work(*carom_snapshots);
   carom_snapshots_work.gather();

   /* equivalent operation must happen within this routine */
   rform->SetupEQPSystemForDomainIntegrator(carom_snapshots_work, integ2, Gt, rhs2);

   for (int k = 0; k < rhs1.dim(); k++)
      EXPECT_NEAR(rhs1(k), rhs2(k), threshold);

   double eqp_tol = 1.0e-10;
   rform->TrainEQP(*carom_snapshots, eqp_tol);
   if (rform->PrecomputeMode()) rform->PrecomputeCoefficients();

   DenseMatrix rom_rhs1(num_basis, num_snap), rom_rhs2(num_basis, num_snap);
   Vector rom_sol(num_basis), rom_rhs1_vec, rom_rhs2_vec;
   for (int s = 0; s < num_snap; s++)
   {
      snapshots.GetColumnReference(s, snapshot);

      nform->Mult(snapshot, rhs_vec);
      rom_rhs1.GetColumnReference(s, rom_rhs1_vec);
      basis.MultTranspose(rhs_vec, rom_rhs1_vec);

      basis.MultTranspose(snapshot, rom_sol);
      rom_rhs2.GetColumnReference(s, rom_rhs2_vec);
      rform->Mult(rom_sol, rom_rhs2_vec);
   }

   for (int i = 0; i < num_basis; i++)
      for (int j = 0; j < num_snap; j++)
         EXPECT_NEAR(rom_rhs1(i, j), rom_rhs2(i, j), threshold);

   delete mesh;
   delete h1_coll;
   delete fes;
   delete nform;
   delete rform;
}

TEST(ROMNonlinearForm, SetupEQPSystemForInteriorFaceIntegrator)
{
   Mesh *mesh = new Mesh("meshes/test.4x4.mesh");
   const int dim = mesh->Dimension();
   const int order = UniformRandom(1, 3);

   FiniteElementCollection *dg_coll(new DG_FECollection(order, dim));
   FiniteElementSpace *fes(new FiniteElementSpace(mesh, dg_coll, dim));
   const int ndofs = fes->GetTrueVSize();
   const int num_snap = UniformRandom(3, 5);
   const int num_basis = num_snap;

   // a fictitious snapshots.
   DenseMatrix snapshots(ndofs, num_snap);
   for (int i = 0; i < ndofs; i++)
      for (int j = 0; j < num_snap; j++)
         snapshots(i, j) = 2.0 * UniformRandom() - 1.0;

   CAROM::Options options(ndofs, num_snap, 1, true);
   options.static_svd_preserve_snapshot = true;
   CAROM::BasisGenerator basis_generator(options, false, "test_basis");
   Vector snapshot(ndofs);
   for (int s = 0; s < num_snap; s++)
   {
      snapshots.GetColumnReference(s, snapshot);
      basis_generator.takeSample(snapshot.GetData());
   }
   basis_generator.endSamples();
   std::shared_ptr<const CAROM::Matrix> carom_snapshots = basis_generator.getSnapshotMatrix();
   std::shared_ptr<const CAROM::Matrix> carom_basis = basis_generator.getSpatialBasis();
   DenseMatrix basis(ndofs, num_basis);
   CAROM::CopyMatrix(*carom_basis, basis);

   IntegrationRule ir = IntRules.Get(fes->GetFE(0)->GetGeomType(),
                                    (int)(ceil(1.5 * (2 * fes->GetMaxElementOrder() - 1))));
   ConstantCoefficient pi(3.141592);
   auto *integ1 = new DGLaxFriedrichsFluxIntegrator(pi);
   integ1->SetIntRule(&ir);
   auto *integ2 = new DGLaxFriedrichsFluxIntegrator(pi);
   integ2->SetIntRule(&ir);

   NonlinearForm *nform(new NonlinearForm(fes));
   nform->AddInteriorFaceIntegrator(integ1);

   ROMNonlinearForm *rform(new ROMNonlinearForm(num_basis, fes));
   rform->AddInteriorFaceIntegrator(integ2);
   rform->SetBasis(basis);
   rform->SetPrecomputeMode(true);

   CAROM::Vector rhs1(num_snap * num_basis, false);
   CAROM::Vector rhs2(num_snap * num_basis, false);
   CAROM::Matrix Gt(1, 1, true);

   /* exact right-hand side by inner product of basis and fom vectors */
   Vector rhs_vec(ndofs), basis_col(ndofs);
   for (int s = 0; s < num_snap; s++)
   {
      snapshots.GetColumnReference(s, snapshot);

      nform->Mult(snapshot, rhs_vec);
      for (int b = 0; b < num_basis; b++)
      {
         basis.GetColumnReference(b, basis_col);
         rhs1(b + s * num_basis) = basis_col * rhs_vec;
      }
   }

   /*
      TODO(kevin): this is a boilerplate for parallel POD/EQP training.
      Will have to consider parallel-compatible test.
   */
   CAROM::Matrix carom_snapshots_work(*carom_snapshots);
   carom_snapshots_work.gather();

   /* equivalent operation must happen within this routine */
   Array<int> fidxs;
   rform->SetupEQPSystemForInteriorFaceIntegrator(carom_snapshots_work, integ2, Gt, rhs2, fidxs);

   for (int k = 0; k < rhs1.dim(); k++)
      EXPECT_NEAR(rhs1(k), rhs2(k), threshold);

   double eqp_tol = 1.0e-10;
   rform->TrainEQP(*carom_snapshots, eqp_tol);
   if (rform->PrecomputeMode()) rform->PrecomputeCoefficients();

   DenseMatrix rom_rhs1(num_basis, num_snap), rom_rhs2(num_basis, num_snap);
   Vector rom_sol(num_basis), rom_rhs1_vec, rom_rhs2_vec;
   for (int s = 0; s < num_snap; s++)
   {
      snapshots.GetColumnReference(s, snapshot);

      nform->Mult(snapshot, rhs_vec);
      rom_rhs1.GetColumnReference(s, rom_rhs1_vec);
      basis.MultTranspose(rhs_vec, rom_rhs1_vec);

      basis.MultTranspose(snapshot, rom_sol);
      rom_rhs2.GetColumnReference(s, rom_rhs2_vec);
      rform->Mult(rom_sol, rom_rhs2_vec);
   }

   for (int i = 0; i < num_basis; i++)
      for (int j = 0; j < num_snap; j++)
         EXPECT_NEAR(rom_rhs1(i, j), rom_rhs2(i, j), threshold);

   delete mesh;
   delete dg_coll;
   delete fes;
   delete nform;
   delete rform;
}

TEST(ROMNonlinearForm, SetupEQPSystemForBdrFaceIntegrator)
{
   Mesh *mesh = new Mesh("meshes/test.4x4.mesh");
   const int dim = mesh->Dimension();
   const int order = UniformRandom(1, 3);

   FiniteElementCollection *dg_coll(new DG_FECollection(order, dim));
   FiniteElementSpace *fes(new FiniteElementSpace(mesh, dg_coll, dim));
   const int ndofs = fes->GetTrueVSize();
   const int num_snap = UniformRandom(3, 5);
   const int num_basis = num_snap;

   // a fictitious snapshots.
   DenseMatrix snapshots(ndofs, num_snap);
   for (int i = 0; i < ndofs; i++)
      for (int j = 0; j < num_snap; j++)
         snapshots(i, j) = 2.0 * UniformRandom() - 1.0;

   CAROM::Options options(ndofs, num_snap, 1, true);
   options.static_svd_preserve_snapshot = true;
   CAROM::BasisGenerator basis_generator(options, false, "test_basis");
   Vector snapshot(ndofs);
   for (int s = 0; s < num_snap; s++)
   {
      snapshots.GetColumnReference(s, snapshot);
      basis_generator.takeSample(snapshot.GetData());
   }
   basis_generator.endSamples();
   std::shared_ptr<const CAROM::Matrix> carom_snapshots = basis_generator.getSnapshotMatrix();
   std::shared_ptr<const CAROM::Matrix> carom_basis = basis_generator.getSpatialBasis();
   DenseMatrix basis(ndofs, num_basis);
   CAROM::CopyMatrix(*carom_basis, basis);

   IntegrationRule ir = IntRules.Get(fes->GetFE(0)->GetGeomType(),
                                    (int)(ceil(1.5 * (2 * fes->GetMaxElementOrder() - 1))));
   ConstantCoefficient pi(3.141592);
   Vector ud(dim);
   for (int d = 0; d < dim; d++) ud(d) = 2.0 * UniformRandom() - 1.0;
   VectorConstantCoefficient ud_coeff(ud);

   auto *integ1 = new DGLaxFriedrichsFluxIntegrator(pi, &ud_coeff);
   integ1->SetIntRule(&ir);
   auto *integ2 = new DGLaxFriedrichsFluxIntegrator(pi, &ud_coeff);
   integ2->SetIntRule(&ir);

   NonlinearForm *nform(new NonlinearForm(fes));
   nform->AddBdrFaceIntegrator(integ1);

   ROMNonlinearForm *rform(new ROMNonlinearForm(num_basis, fes));
   rform->AddBdrFaceIntegrator(integ2);
   rform->SetBasis(basis);
   rform->SetPrecomputeMode(true);

   CAROM::Vector rhs1(num_snap * num_basis, false);
   CAROM::Vector rhs2(num_snap * num_basis, false);
   CAROM::Matrix Gt(1, 1, true);

   /* exact right-hand side by inner product of basis and fom vectors */
   Vector rhs_vec(ndofs), basis_col(ndofs);
   for (int s = 0; s < num_snap; s++)
   {
      snapshots.GetColumnReference(s, snapshot);

      nform->Mult(snapshot, rhs_vec);
      for (int b = 0; b < num_basis; b++)
      {
         basis.GetColumnReference(b, basis_col);
         rhs1(b + s * num_basis) = basis_col * rhs_vec;
      }
   }

   /*
      TODO(kevin): this is a boilerplate for parallel POD/EQP training.
      Will have to consider parallel-compatible test.
   */
   CAROM::Matrix carom_snapshots_work(*carom_snapshots);
   carom_snapshots_work.gather();

   /* equivalent operation must happen within this routine */
   Array<int> bdr_attr_marker(mesh->bdr_attributes.Size() ?
                              mesh->bdr_attributes.Max() : 0);
   bdr_attr_marker = 1;
   Array<int> bidxs;
   rform->SetupEQPSystemForBdrFaceIntegrator(carom_snapshots_work, integ2, bdr_attr_marker, Gt, rhs2, bidxs);

   for (int k = 0; k < rhs1.dim(); k++)
      EXPECT_NEAR(rhs1(k), rhs2(k), threshold);

   double eqp_tol = 1.0e-10;
   rform->TrainEQP(*carom_snapshots, eqp_tol);
   if (rform->PrecomputeMode()) rform->PrecomputeCoefficients();

   DenseMatrix rom_rhs1(num_basis, num_snap), rom_rhs2(num_basis, num_snap);
   Vector rom_sol(num_basis), rom_rhs1_vec, rom_rhs2_vec;
   for (int s = 0; s < num_snap; s++)
   {
      snapshots.GetColumnReference(s, snapshot);

      nform->Mult(snapshot, rhs_vec);
      rom_rhs1.GetColumnReference(s, rom_rhs1_vec);
      basis.MultTranspose(rhs_vec, rom_rhs1_vec);

      basis.MultTranspose(snapshot, rom_sol);
      rom_rhs2.GetColumnReference(s, rom_rhs2_vec);
      rform->Mult(rom_sol, rom_rhs2_vec);
   }

   for (int i = 0; i < num_basis; i++)
      for (int j = 0; j < num_snap; j++)
         EXPECT_NEAR(rom_rhs1(i, j), rom_rhs2(i, j), threshold);

   delete mesh;
   delete dg_coll;
   delete fes;
   delete nform;
   delete rform;
}

int main(int argc, char* argv[])
{
   MPI_Init(&argc, &argv);
   ::testing::InitGoogleTest(&argc, argv);
   int result = RUN_ALL_TESTS();
   MPI_Finalize();
   return result;
}
