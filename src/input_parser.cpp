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

#include "input_parser.hpp"

YAML::Node InputParser::FindNode(const std::string &keys)
{
   // Per tutorial of yaml-cpp, operator= *seems* to be a shallow copy.
   // However, in fact they are deep copy, and the following recursive = operation screws up the dict.
   // Now we store the node in a vector.
   std::vector<YAML::Node> nodes(0);
   nodes.push_back(dict_);

   std::istringstream key_iterator(keys);
   int dd = 0;
   for (std::string s; std::getline(key_iterator, s, '/'); )
   {
      nodes.push_back(nodes.back()[s]);

      if (!(nodes.back())) return nodes.back();
   }
   return nodes.back();
}

// template int InputParser::GetRequiredOption<int>(const std::string&);

InputParser config;