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

#include "main_workflow.hpp"
#include "multiblock_solver.hpp"
#include <fstream>
#include <iostream>

using namespace std;
using namespace mfem;

double dbc2(const Vector &x)
{
   return 0.1 - 0.1 * (x(1) - 1.0) * (x(1) - 1.0);
}

double dbc4(const Vector &x)
{
   return -0.1 + 0.1 * (x(1) - 1.0) * (x(1) - 1.0);
}

void RunExample()
{
   MultiBlockSolver test;

   test.InitVariables();
   test.InitVisualization();

   test.AddBCFunction(dbc2, 2);
   test.AddBCFunction(dbc4, 4);
   test.AddRHSFunction(1.0);

   test.BuildOperators();

   test.SetupBCOperators();

   test.Assemble();

   test.Solve();
   test.SaveVisualization();
}

SampleGenerator* InitSampleGenerator(MPI_Comm comm, ParameterizedProblem* problem)
{
   SampleGenerator* generator = NULL;

   std::string type = config.GetOption<std::string>("sample_generation/type", "base");

   if (type == "base")
   {
      generator = new SampleGenerator(comm, problem);
   }
   else if (type == "random")
   {
      generator = new RandomSampleGenerator(comm, problem);
   }
   else
   {
      mfem_error("Unknown sample generator type!\n");
   }

   return generator;
}

void GenerateSamples(MPI_Comm comm)
{
   ParameterizedProblem *problem = InitParameterizedProblem();
   SampleGenerator *sample_generator = InitSampleGenerator(comm, problem);
   sample_generator->GenerateParamSpace();
   MultiBlockSolver *test = NULL;

   for (int s = 0; s < sample_generator->GetTotalSampleSize(); s++)
   {
      if (!sample_generator->IsMyJob(s)) continue;

      test = new MultiBlockSolver();
      if (!test->UseRom()) mfem_error("ROM must be enabled for sample generation!\n");
      test->InitVariables();

      sample_generator->SetSampleParams(s);
      problem->SetParameterizedProblem(test);

      int file_idx = s + sample_generator->GetFileOffset();
      const std::string visual_path = sample_generator->GetSamplePath(file_idx, test->GetVisualizationPrefix());
      test->InitVisualization(visual_path);
      test->BuildOperators();
      test->SetupBCOperators();
      test->Assemble();
      test->Solve();
      test->SaveVisualization();

      test->SaveSnapshot(file_idx);

      delete test;
   }

   delete sample_generator;
   delete problem;
}

void BuildROM(MPI_Comm comm)
{
   ParameterizedProblem *problem = InitParameterizedProblem();
   MultiBlockSolver *test = NULL;

   test = new MultiBlockSolver();
   if (!test->UseRom()) mfem_error("ROM must be enabled for BuildROM!\n");
   test->InitVariables();
   // test->InitVisualization();

   // NOTE: you need this to set bc/rhs coefficients!
   // This case, we can use default parameter values of the problem.
   problem->SetParameterizedProblem(test);

   // TODO: there are skippable operations depending on rom/fom mode.
   test->BuildOperators();
   test->SetupBCOperators();
   test->Assemble();
   
   ROMHandler *rom = test->GetROMHandler();
   if (!rom->UseExistingBasis())
   {
      // TODO: basis for multiple components
      SampleGenerator *sample_generator = InitSampleGenerator(comm, problem);
      sample_generator->SetParamSpaceSizes();
      const int total_samples = sample_generator->GetTotalSampleSize();
      
      test->FormReducedBasis(total_samples);
      delete sample_generator;
   }
   rom->LoadReducedBasis();
   
   TopologyHandlerMode topol_mode = test->GetTopologyMode();
   ROMBuildingLevel save_operator = rom->SaveOperator();
   switch (topol_mode)
   {
      case TopologyHandlerMode::SUBMESH:
      {
         if (save_operator == ROMBuildingLevel::GLOBAL)
            test->ProjectOperatorOnReducedBasis();
         else if (save_operator == ROMBuildingLevel::COMPONENT)
            mfem_error("Unsupported rom building level!\n");
         break;
      }  // case TopologyHandlerMode::SUBMESH:
      case TopologyHandlerMode::COMPONENT:
      {
         switch (save_operator)
         {
            case ROMBuildingLevel::COMPONENT:
            {
               test->AllocateROMElements();
               test->BuildROMElements();
               std::string filename = rom->GetOperatorPrefix() + ".h5";
               test->SaveROMElements(filename);
               break;
            }
            case ROMBuildingLevel::GLOBAL:
            {
               test->ProjectOperatorOnReducedBasis();
               break;
            }
         }  // switch (save_operator)
         break;
      }  // case TopologyHandlerMode::COMPONENT:
      default:
      {
         mfem_error("Unknown TopologyHandler Mode!\n");
         break;
      }
   }  // switch (topol_mode)

   test->SaveBasisVisualization();

   delete test;
   delete problem;
}

double SingleRun()
{
   ParameterizedProblem *problem = InitParameterizedProblem();
   MultiBlockSolver *test = new MultiBlockSolver();
   test->InitVariables();
   test->InitVisualization();

   StopWatch solveTimer;
   std::string solveType = (test->UseRom()) ? "ROM" : "FOM";

   std::string problem_name = problem->GetProblemName();
   std::string param_list_str("single_run/" + problem_name);
   YAML::Node param_list = config.FindNode(param_list_str);
   if (!param_list) printf("Single Run - cannot find the problem name!\n");

   size_t num_params = param_list.size();
   for (int p = 0; p < num_params; p++)
   {
      std::string param_name = config.GetRequiredOptionFromDict<std::string>("parameter_name", param_list[p]);
      double value = config.GetRequiredOptionFromDict<double>("value", param_list[p]);
      problem->SetParams(param_name, value);
   }

   problem->SetParameterizedProblem(test);

   // TODO: there are skippable operations depending on rom/fom mode.
   test->BuildRHSOperators();
   test->SetupRHSBCOperators();
   test->AssembleRHS();

   solveTimer.Start();
   if (test->UseRom())
   {
      printf("ROM with ");
      ROMHandler *rom = test->GetROMHandler();
      ROMBuildingLevel save_operator = rom->SaveOperator();
      TopologyHandlerMode topol_mode = test->GetTopologyMode();
      switch (topol_mode)
      {
         case TopologyHandlerMode::SUBMESH:
         {
            printf("SubMesh Topology - ");
            switch (save_operator)
            {
               case ROMBuildingLevel::GLOBAL:
               {
                  printf("loading operator file.. ");
                  rom->LoadOperatorFromFile();
                  break;
               }
               case ROMBuildingLevel::NONE:
               {
                  printf("building operator file all the way from FOM.. ");
                  test->BuildDomainOperators();
                  test->SetupDomainBCOperators();
                  test->AssembleOperator();
                  test->ProjectOperatorOnReducedBasis();
                  break;
               }
               default:
               {
                  mfem_error("Unsupported rom building level!\n");
                  break;
               }
            }
            break;
         }  // case TopologyHandlerMode::SUBMESH:
         case TopologyHandlerMode::COMPONENT:
         {
            printf("Component-wise Topology - ");
            // TODO: bottom-up assembly.
            switch (save_operator)
            {
               case ROMBuildingLevel::COMPONENT:
               {
                  printf("loading component operator file.. ");
                  test->AllocateROMElements();
                  std::string filename = rom->GetOperatorPrefix() + ".h5";
                  test->LoadROMElements(filename);
                  test->AssembleROM();
                  break;
               }
               case ROMBuildingLevel::GLOBAL:
               {
                  printf("loading global operator file.. ");
                  rom->LoadOperatorFromFile();
                  break;
               }
               case ROMBuildingLevel::NONE:
               {
                  printf("building operator file all the way from FOM.. ");
                  test->BuildDomainOperators();
                  test->SetupDomainBCOperators();
                  test->AssembleOperator();
                  test->ProjectOperatorOnReducedBasis();
                  break;
               }
            }
            break;
         }  // case TopologyHandlerMode::COMPONENT:
         default:
         {
            mfem_error("Unknown TopologyHandler Mode!\n");
            break;
         }
      }  // switch (topol_mode)
      printf("Done!\n");

      printf("Projecting RHS to ROM.. ");
      test->ProjectRHSOnReducedBasis();
      printf("Done!\n");
   }  // if (test->UseRom())
   else
   {
      test->BuildDomainOperators();
      test->SetupDomainBCOperators();
      test->AssembleOperator();
   }  // not if (test->UseRom())
   solveTimer.Stop();
   printf("%s-assemble time: %f seconds.\n", solveType.c_str(), solveTimer.RealTime());

   solveTimer.Clear();
   solveTimer.Start();
   if (test->UseRom())
   {
      test->SolveROM();
   }
   else
   {
      // TODO: move matrix assembly to here.
      test->Solve();
   }
   solveTimer.Stop();
   printf("%s-solve time: %f seconds.\n", solveType.c_str(), solveTimer.RealTime());

   test->SaveVisualization();

   double error = -1.0;
   bool compare_sol = config.GetOption<bool>("model_reduction/compare_solution", false);
   if (test->UseRom() && compare_sol)
   {
      solveTimer.Clear();
      solveTimer.Start();
      test->BuildDomainOperators();
      test->SetupDomainBCOperators();
      test->AssembleOperator();
      solveTimer.Stop();
      printf("FOM-assembly time: %f seconds.\n", solveTimer.RealTime());

      error = test->CompareSolution();
   }
   
   delete test;
   delete problem;

   return error;
}
