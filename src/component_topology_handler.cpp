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

#include "component_topology_handler.hpp"
#include "hdf5.h"
#include "hdf5_utils.hpp"
#include <fstream>

using namespace std;
using namespace mfem;

inline bool FileExists(const std::string& name)
{
   std::ifstream f(name.c_str());
   return f.good();
   // ifstream f will be closed upon the end of the function.
}

ComponentTopologyHandler::ComponentTopologyHandler()
   : TopologyHandler()
{
   verbose = config.GetOption<bool>("mesh/component-wise/verbose", false);

   // read global file.
   std::string filename = config.GetRequiredOption<std::string>("mesh/component-wise/global_config");
   ReadGlobalConfigFromFile(filename);

   SetupComponents();

   // Assume all components have the same spatial dimension.
   dim = components[0]->Dimension();

   if (num_ref_ports > 0)
      SetupReferencePorts();

   // Do we really need to copy all meshes?
   SetupMeshes();

   SetupReferenceInterfaces();

   SetupPorts();

   // Do we really need to set boundary attributes of all meshes?
   SetupBoundaries();
}

void ComponentTopologyHandler::ExportInfo(Array<Mesh*> &mesh_ptrs, TopologyData &topol_data)
{
   mesh_ptrs = meshes;

   topol_data.dim = dim;
   topol_data.numSub = numSub;
   topol_data.global_bdr_attributes = &bdr_attributes;
}

void ComponentTopologyHandler::SetupComponents()
{
   assert(num_comp > 0);

   YAML::Node comp_list = config.FindNode("mesh/component-wise/components");
   if (!comp_list) mfem_error("ComponentTopologyHandler: component list does not exist!\n");

   // We only read the components that are specified in the global configuration.
   components.SetSize(num_comp);
   components = NULL;
   for (int c = 0; c < comp_list.size(); c++)
   {
      std::string comp_name = config.GetRequiredOptionFromDict<std::string>("name", comp_list[c]);
      // do not read if this component is not used in the global config.
      if (!comp_names.count(comp_name)) continue;

      int idx = comp_names[comp_name];
      std::string filename = config.GetRequiredOptionFromDict<std::string>("file", comp_list[c]);
      components[idx] = new BlockMesh(filename.c_str());
   }

   for (int c = 0; c < components.Size(); c++) assert(components[c] != NULL);

   // Uniform refinement if specified.
   int num_refinement = config.GetOption<int>("mesh/uniform_refinement", 0);
   if (num_refinement > 0)
   {
      mfem_warning("ComponentTopologyHandler: component meshes are refined. Existing ports may not work for refined meshes.\n");
      for (int c = 0; c < components.Size(); c++)
         for (int k = 0; k < num_refinement; k++)
            components[c]->UniformRefinement();
   }
}

void ComponentTopologyHandler::SetupReferencePorts()
{
   assert(num_ref_ports > 0);

   ref_ports.SetSize(num_ref_ports);
   ref_ports = NULL;

   YAML::Node port_list = config.FindNode("mesh/component-wise/ports");
   if (!port_list)
      mfem_error("ComponentTopologyHandler: port list does not exist!\n");
   else
   {
      for (int p = 0; p < port_list.size(); p++)
      {
         // Read hdf5 files.
         std::string filename = config.GetRequiredOptionFromDict<std::string>("file", port_list[p]);

         if (FileExists(filename))
            ReadPortsFromFile(filename);
         else
            BuildPortFromInput(port_list[p]);
      }
   }
   
   for (int p = 0; p < ref_ports.Size(); p++) assert(ref_ports[p] != NULL);
}

void ComponentTopologyHandler::ReadGlobalConfigFromFile(const std::string filename)
{
   hid_t file_id;
   hid_t grp_id;
   herr_t errf = 0;
   file_id = H5Fopen(filename.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
   assert(file_id >= 0);
   
   {  // Component list.
      // This line below currently does not work.
      // hdf5_utils::ReadDataset(file_id, "components", comp_names);
      grp_id = H5Gopen2(file_id, "components", H5P_DEFAULT);
      assert(grp_id >= 0);

      hdf5_utils::ReadAttribute(grp_id, "number_of_components", num_comp);
      for (int c = 0; c < num_comp; c++)
      {
         std::string tmp;
         hdf5_utils::ReadAttribute(grp_id, std::to_string(c).c_str(), tmp);
         comp_names[tmp] = c;
      }

      // Mesh list.
      hdf5_utils::ReadDataset(grp_id, "meshes", mesh_types);

      // Configuration (translation, rotation) of meshes.
      Array2D<double> tmp;
      hdf5_utils::ReadDataset(grp_id, "configuration", tmp);
      assert(mesh_types.Size() == tmp.NumRows());
      numSub = mesh_types.Size();

      mesh_configs.SetSize(numSub);
      for (int m = 0; m < numSub; m++)
      {
         const double *m_cf = tmp.GetRow(m);
         for (int d = 0; d < 3; d++)
         {
            mesh_configs[m].trans[d] = m_cf[d];
            mesh_configs[m].rotate[d] = m_cf[d + 3];
         }
      }

      errf = H5Gclose(grp_id);
      assert(errf >= 0);
   }

   {  // Port list.
      grp_id = H5Gopen2(file_id, "ports", H5P_DEFAULT);
      assert(grp_id >= 0);

      hdf5_utils::ReadAttribute(grp_id, "number_of_references", num_ref_ports);
      // // hdf5_utils::ReadDataset(file_id, "ports", ports);
      for (int p = 0; p < num_ref_ports; p++)
      {
         std::string tmp;
         hdf5_utils::ReadAttribute(grp_id, std::to_string(p).c_str(), tmp);
         port_names[tmp] = p;
      }

      // Global interface port data.
      Array2D<int> tmp;
      hdf5_utils::ReadDataset(grp_id, "interface", tmp);
      num_ports = tmp.NumRows();
      port_infos.SetSize(num_ports);
      port_types.SetSize(num_ports);

      for (int p = 0; p < num_ports; p++)
      {
         const int *p_data = tmp.GetRow(p);
         port_infos[p].Mesh1 = p_data[0];
         port_infos[p].Mesh2 = p_data[1];
         port_infos[p].Attr1 = p_data[2];
         port_infos[p].Attr2 = p_data[3];
         port_types[p] = p_data[4];
      }

      errf = H5Gclose(grp_id);
      assert(errf >= 0);
   }

   {  // Boundary data.
      Array2D<int> tmp;
      hdf5_utils::ReadDataset(file_id, "boundary", tmp);

      bdr_c2g.SetSize(numSub);
      for (int m = 0; m < numSub; m++) bdr_c2g[m] = new std::unordered_map<int,int>;
      bdr_attributes.SetSize(0);

      for (int b = 0; b < tmp.NumRows(); b++)
      {
         const int *b_data = tmp.GetRow(b);
         int m = b_data[1];
         (*bdr_c2g[m])[b_data[2]] = b_data[0];

         int idx = bdr_attributes.Find(b_data[0]);
         if (idx < 0) bdr_attributes.Append(b_data[0]);
      }
   }

   errf = H5Fclose(file_id);
   assert(errf >= 0);
}

void ComponentTopologyHandler::ReadPortsFromFile(const std::string filename)
{
   hid_t file_id;
   herr_t errf = 0;
   file_id = H5Fopen(filename.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
   assert(file_id >= 0);

   // number of ports stored in the given hdf5 file.
   int size = -1;
   hdf5_utils::ReadAttribute(file_id, "number_of_ports", size);
   assert(size > 0);

   for (int k = 0; k < size; k++)
   {
      hid_t grp_id;
      grp_id = H5Gopen2(file_id, std::to_string(k).c_str(), H5P_DEFAULT);
      assert(grp_id >= 0);

      std::string port_name;
      hdf5_utils::ReadAttribute(grp_id, "name", port_name);
      // Read only the ports that are specified in the global configuration.
      if (!port_names.count(port_name))
      {
         errf = H5Gclose(grp_id);
         assert(errf >= 0);
         continue;
      }

      int battr1, battr2;
      hdf5_utils::ReadAttribute(grp_id, "bdr_attr1", battr1);
      hdf5_utils::ReadAttribute(grp_id, "bdr_attr2", battr2);
      printf("port: %d - %d\n", battr1, battr2);

      std::string name1, name2;
      hdf5_utils::ReadAttribute(grp_id, "component1", name1);
      hdf5_utils::ReadAttribute(grp_id, "component2", name2);

      int comp1 = comp_names[name1];
      int comp2 = comp_names[name2];
      assert((comp1 >= 0) && (comp2 >= 0));

      Array<int> vtx1, vtx2;
      hdf5_utils::ReadDataset(grp_id, "vtx1", vtx1);
      hdf5_utils::ReadDataset(grp_id, "vtx2", vtx2);
      assert(vtx1.Size() == vtx2.Size());

      printf("vtx1: ");
      for (int v = 0; v < vtx1.Size(); v++) printf("%d\t", vtx1[v]);
      printf("\n");
      printf("vtx2: ");
      for (int v = 0; v < vtx2.Size(); v++) printf("%d\t", vtx2[v]);
      printf("\n");

      Array<int> be1, be2;
      hdf5_utils::ReadDataset(grp_id, "be1", be1);
      hdf5_utils::ReadDataset(grp_id, "be2", be2);
      assert(be1.Size() == be2.Size());

      printf("be1: ");
      for (int v = 0; v < be1.Size(); v++) printf("%d\t", be1[v]);
      printf("\n");
      printf("be2: ");
      for (int v = 0; v < be2.Size(); v++) printf("%d\t", be2[v]);
      printf("\n");

      errf = H5Gclose(grp_id);
      assert(errf >= 0);

      int port_idx = port_names[port_name];
      ref_ports[port_idx] = new PortData;
      PortData *port = ref_ports[port_idx];
      port->Component1 = comp1;
      port->Component2 = comp2;
      port->Attr1 = battr1;
      port->Attr2 = battr2;
      for (int v = 0; v < vtx1.Size(); v++)
         port->vtx2to1[vtx2[v]] = vtx1[v];
      port->be_pairs.SetSize(be1.Size(), 2);
      for (int v = 0; v < be1.Size(); v++)
      {
         int *be_pair = port->be_pairs.GetRow(v);
         be_pair[0] = be1[v];
         be_pair[1] = be2[v];
      }
   }  // for (int k = 0; k < size; k++)

   errf = H5Fclose(file_id);
   assert(errf >= 0);

   return;
}

void ComponentTopologyHandler::SetupMeshes()
{
   assert(numSub > 0);
   assert(mesh_types.Size() == numSub);
   assert(mesh_configs.Size() == numSub);

   meshes.SetSize(numSub);
   meshes = NULL;

   for (int m = 0; m < numSub; m++)
   {
      meshes[m] = new Mesh(*components[mesh_types[m]]);

      for (int d = 0; d < 3; d++)
      {
         mesh_config::trans[d] = mesh_configs[m].trans[d];
         mesh_config::rotate[d] = mesh_configs[m].rotate[d];
      }
      meshes[m]->Transform(mesh_config::Transform2D);
   }

   for (int m = 0; m < numSub; m++) assert(meshes[m] != NULL);
}

void ComponentTopologyHandler::SetupBoundaries()
{
   assert(meshes.Size() == numSub);

   for (int m = 0; m < numSub; m++)
   {
      std::unordered_map<int,int> *c2g_map = bdr_c2g[m];
      for (int be = 0; be < meshes[m]->GetNBE(); be++)
      {
         int c_attr = meshes[m]->GetBdrAttribute(be);
         if(!c2g_map->count(c_attr)) continue;

         meshes[m]->SetBdrAttribute(be, (*c2g_map)[c_attr]);
      }

      UpdateBdrAttributes(*meshes[m]);
   }
}

void ComponentTopologyHandler::SetupReferenceInterfaces()
{
   ref_interfaces.SetSize(num_ref_ports);
   
   for (int i = 0; i < num_ref_ports; i++)
   {
      Array2D<int> *be_pairs = &(ref_ports[i]->be_pairs);
      std::unordered_map<int,int> *vtx2to1 = &(ref_ports[i]->vtx2to1);
      int comp1 = ref_ports[i]->Component1;
      int comp2 = ref_ports[i]->Component2;
      int attr1 = ref_ports[i]->Attr1;
      int attr2 = ref_ports[i]->Attr2;

      ref_interfaces[i] = new Array<InterfaceInfo>(0);

      for (int be = 0; be < be_pairs->NumRows(); be++)
      {
         InterfaceInfo tmp;

         int *pair = be_pairs->GetRow(be);
         tmp.BE1 = pair[0];
         tmp.BE2 = pair[1];

         // use the face index from each component mesh.
         int f1 = components[comp1]->GetBdrFace(tmp.BE1);
         int f2 = components[comp2]->GetBdrFace(tmp.BE2);
         int Inf1, Inf2, dump;
         components[comp1]->GetFaceInfos(f1, &Inf1, &dump);
         components[comp2]->GetFaceInfos(f2, &Inf2, &dump);
         tmp.Inf1 = Inf1;

         // determine orientation of the face with respect to mesh2/elem2.
         Array<int> vtx1, vtx2;
         components[comp1]->GetBdrElementVertices(tmp.BE1, vtx1);
         components[comp2]->GetBdrElementVertices(tmp.BE2, vtx2);
         for (int v = 0; v < vtx2.Size(); v++) vtx2[v] = (*vtx2to1)[vtx2[v]];
         switch (dim)
         {
            case 1:
            {
               break;
            }
            case 2:
            {
               if ((vtx1[1] == vtx2[0]) && (vtx1[0] == vtx2[1]))
               {
                  tmp.Inf2 = 64 * (Inf2 / 64) + 1;
               }
               else if ((vtx1[0] == vtx2[0]) && (vtx1[1] == vtx2[1]))
               {
                  tmp.Inf2 = 64 * (Inf2 / 64);
               }
               else
               {
                  mfem_error("orientation error!\n");
               }
               break;
            }
            case 3:
            {
               mfem_error("not implemented yet!\n");
               break;
            }
         }  // switch (dim)

         ref_interfaces[i]->Append(tmp);
      }  // for (int be = 0; be < be_pair->Size(); be++)

      if (verbose)
      {
         std::string format = "";
         for (int k = 0; k < 8; k++) format += "%d\t";
         format += "%d\n";
         printf("Reference Interface %d informations\n", i);
         printf("Attr\tMesh1\tMesh2\tBE1\tBE2\tIdx1\tOri1\tIdx2\tOri2\n");
         for (int k = 0; k < ref_interfaces[i]->Size(); k++)
         {
            InterfaceInfo *tmp = &(*ref_interfaces[i])[k];
            printf(format.c_str(), -1, comp1, comp2,
                                 tmp->BE1, tmp->BE2, tmp->Inf1 / 64,
                                 tmp->Inf1 % 64, tmp->Inf2 / 64, tmp->Inf2 % 64);
         }
         printf("\n");
      }  // if (verbose)
   }  // for (int i = 0; i < num_ref_ports; i++)
}

void ComponentTopologyHandler::SetupPorts()
{
   assert(num_ports > 0);
   assert(port_infos.Size() == num_ports);
   assert(port_types.Size() == num_ports);

   // Port attribute will be setup with a value that does not overlap with any component boundary attribute.
   int attr_offset = 0;
   for (int c = 0; c < num_comp; c++)
      attr_offset = max(attr_offset, components[c]->bdr_attributes.Max());
   // Also does not overlap with global boundary attributes.
   attr_offset = max(attr_offset, bdr_attributes.Max());
   attr_offset += 1;

   interface_infos.SetSize(num_ports);
   for (int p = 0; p < num_ports; p++)
   {
      PortData *ref_port = ref_ports[port_types[p]];
      assert(ref_port != NULL);
      assert(mesh_types[port_infos[p].Mesh1] == ref_port->Component1);
      assert(mesh_types[port_infos[p].Mesh2] == ref_port->Component2);
      assert(port_infos[p].Attr1 == ref_port->Attr1);
      assert(port_infos[p].Attr2 == ref_port->Attr2);

      port_infos[p].PortAttr = attr_offset;

      interface_infos[p] = ref_interfaces[port_types[p]];

      Mesh *mesh1 = meshes[port_infos[p].Mesh1];
      Mesh *mesh2 = meshes[port_infos[p].Mesh2];
      for (int b = 0; b < interface_infos[p]->Size(); b++)
      {
         InterfaceInfo *b_info = &((*interface_infos[p])[b]);
         mesh1->SetBdrAttribute(b_info->BE1, attr_offset);
         mesh2->SetBdrAttribute(b_info->BE2, attr_offset);
      }

      attr_offset += 1;
   }

   for (int p = 0; p < interface_infos.Size(); p++) assert(interface_infos[p] != NULL);

   for (int m = 0; m < numSub; m++) UpdateBdrAttributes(*meshes[m]);
}

void ComponentTopologyHandler::BuildPortFromInput(const YAML::Node port_dict)
{
   std::string port_name = config.GetRequiredOptionFromDict<std::string>("name", port_dict);
   if (!port_names.count(port_name))
      return;

   int port_idx = port_names[port_name];
   ref_ports[port_idx] = new PortData;
   PortData *port = ref_ports[port_idx];

   assert(num_comp > 0);
   assert(components.Size() == num_comp);
   std::string name1 = config.GetRequiredOptionFromDict<std::string>("comp1/name", port_dict);
   std::string name2 = config.GetRequiredOptionFromDict<std::string>("comp2/name", port_dict);
   if (!comp_names.count(name1))
      mfem_error("component 1 for the port building does not exist!\n");
   if (!comp_names.count(name2))
      mfem_error("component 2 for the port building does not exist!\n");
   
   int idx1 = comp_names[name1];
   int idx2 = comp_names[name2];
   Mesh *comp1 = components[idx1];
   Mesh *comp2 = components[idx2];
   assert(comp1 != NULL);
   assert(comp2 != NULL);

   int attr1 = config.GetRequiredOptionFromDict<int>("comp1/attr", port_dict);
   int attr2 = config.GetRequiredOptionFromDict<int>("comp2/attr", port_dict);
   int exists = comp1->bdr_attributes.Find(attr1);
   if (exists < 0) mfem_error("BuildPortFromInput: specified boundary attribute for component 1 does not exist!\n");
   exists = comp2->bdr_attributes.Find(attr2);
   if (exists < 0) mfem_error("BuildPortFromInput: specified boundary attribute for component 2 does not exist!\n");

   port->Component1 = idx1;
   port->Component2 = idx2;
   port->Attr1 = attr1;
   port->Attr2 = attr2;

   Vector trnsf2 = config.GetRequiredOptionFromDict<Vector>("comp2_configuration", port_dict);
   assert(trnsf2.Size() == 6);
   for (int d = 0; d < 3; d++)
   {
      mesh_config::trans[d] = trnsf2[d];
      mesh_config::rotate[d] = trnsf2[d + 3];
   }

   Array<int> be1(0), be2(0);
   for (int b = 0; b < comp1->GetNBE(); b++)
      if (comp1->GetBdrAttribute(b) == attr1) be1.Append(b);
   for (int b = 0; b < comp2->GetNBE(); b++)
      if (comp2->GetBdrAttribute(b) == attr2) be2.Append(b);
   assert(be1.Size() == be2.Size());
   port->be_pairs.SetSize(be1.Size(), 2);
   port->be_pairs = -1;

   Array<int> vtx1(0), vtx2(0);
   for (int b1 = 0; b1 < be1.Size(); b1++)
   {
      Array<int> b_vtx;
      comp1->GetBdrElementVertices(be1[b1], b_vtx);
      for (int v = 0; v < b_vtx.Size(); v++)
         if (vtx1.Find(b_vtx[v]) < 0) vtx1.Append(b_vtx[v]);
   }
   for (int b2 = 0; b2 < be2.Size(); b2++)
   {
      Array<int> b_vtx;
      comp2->GetBdrElementVertices(be2[b2], b_vtx);
      for (int v = 0; v < b_vtx.Size(); v++)
         if (vtx2.Find(b_vtx[v]) < 0) vtx2.Append(b_vtx[v]);
   }
   assert(vtx1.Size() == vtx2.Size());

   // Since comp2 mesh's nodes are already set up,
   // Mesh::Transform transforms the node coordinates, instead of vertices.
   // For actual meshes, this does not matter.
   // comp2->Transform(mesh_config::Transform2D);
   Array<Vector*> x2_trns(vtx2.Size());
   x2_trns = NULL;
   for (int v2 = 0; v2 < vtx2.Size(); v2++)
   {
      double *x2 = comp2->GetVertex(vtx2[v2]);
      Vector tmp(x2, dim);
      x2_trns[v2] = new Vector;
      mesh_config::Transform2D(tmp, *x2_trns[v2]);
   }

   double threshold = 1.e-10;
   for (int v1 = 0; v1 < vtx1.Size(); v1++)
   {
      double *x1 = comp1->GetVertex(vtx1[v1]);
      bool found_match = false;

      for (int v2 = 0; v2 < vtx2.Size(); v2++)
      {
         if (port->vtx2to1.count(vtx2[v2])) continue;

         // double *x2 = comp2->GetVertex(vtx2[v2]);
         const double *x2 = x2_trns[v2]->Read();

         bool match = true;
         for (int d = 0; d < dim; d++)
         {
            match = (abs(x1[d] - x2[d]) < threshold);
            if (!match) break;
         }

         if (match)
         {
            port->vtx2to1[vtx2[v2]] = vtx1[v1];
            found_match = true;
            break;
         }
      }  // for (int v2 = 0; v2 < vtx2.Size(); v2++)

      if (!found_match) mfem_error("BuildPortFromInput: Cannot find the matching vertex!\n");
   }  // for (int v1 = 0; v1 < vtx1.Size(); v1++)

   for (int b2 = 0; b2 < be2.Size(); b2++)
   {
      Array<int> b_vtx2;
      comp2->GetBdrElementVertices(be2[b2], b_vtx2);

      Array<int> b2_vtx1(b_vtx2.Size());
      for (int v = 0; v < b2_vtx1.Size(); v++)
      {
         assert(port->vtx2to1.count(b_vtx2[v]));
         b2_vtx1[v] = port->vtx2to1[b_vtx2[v]];
      }
      b2_vtx1.Sort();

      for (int b1 = 0; b1 < be1.Size(); b1++)
      {
         Array<int> b1_vtx1;
         comp1->GetBdrElementVertices(be1[b1], b1_vtx1);

         assert(b2_vtx1.Size() == b1_vtx1.Size());
         b1_vtx1.Sort();

         bool match = true;
         for (int v = 0; v < b1_vtx1.Size(); v++)
            if (b1_vtx1[v] != b2_vtx1[v])
            {
               match = false;
               break;
            }
         
         if (match)
         {
            int *be_pair = port->be_pairs.GetRow(b2);
            be_pair[0] = be1[b1];
            be_pair[1] = be2[b2];
            break;
         }
      }  // for (int b1 = 0; b1 < be1.Size(); b1++)
   }  // for (int b2 = 0; b2 < be2.Size(); b2++)

   for (int i = 0; i < port->be_pairs.NumRows(); i++)
      for (int j = 0; j < port->be_pairs.NumCols(); j++)
         assert(port->be_pairs(i,j) >= 0);

   if (verbose)
   {
      printf("port %s\n", port_name.c_str());
      printf("comp: %d %d\n", port->Component1, port->Component2);
      printf("attr: %d %d\n", port->Attr1, port->Attr2);
      printf("be1\tbe2\n");
      for (int i = 0; i < port->be_pairs.NumRows(); i++)
         printf("%d\t%d\n", port->be_pairs(i,0), port->be_pairs(i,1));
      printf("vtx2 -> vtx1\n");
      for (auto m : port->vtx2to1)
         printf("%d\t%d\n", m.first, m.second);
      printf("\n");
   }  // if (verbose)
}