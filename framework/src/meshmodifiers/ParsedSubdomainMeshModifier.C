//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

// MOOSE includes
#include "ParsedSubdomainMeshModifier.h"
#include "Conversion.h"
#include "MooseMesh.h"

#include "libmesh/fparser_ad.hh"
#include "libmesh/elem.h"

registerMooseObjectReplaced("MooseApp",
                            ParsedSubdomainMeshModifier,
                            "11/30/2019 00:00",
                            ParsedSubdomainMeshGenerator);

template <>
InputParameters
validParams<ParsedSubdomainMeshModifier>()
{
  InputParameters params = validParams<MeshModifier>();
  params += FunctionParserUtils<false>::validParams();
  params.addRequiredParam<std::string>("combinatorial_geometry",
                                       "Function expression encoding a combinatorial geometry");
  params.addRequiredParam<SubdomainID>("block_id",
                                       "Subdomain id to set for inside of the combinatorial");
  params.addParam<SubdomainName>("block_name",
                                 "Subdomain name to set for inside of the combinatorial");
  params.addParam<std::vector<SubdomainID>>("excluded_subdomain_ids",
                                            "A set of subdomain ids that will not changed even if "
                                            "they are inside/outside the combinatorial geometry");
  params.addParam<std::vector<std::string>>(
      "constant_names", "Vector of constants used in the parsed function (use this for kB etc.)");
  params.addParam<std::vector<std::string>>(
      "constant_expressions",
      "Vector of values for the constants in constant_names (can be an FParser expression)");
  params.addClassDescription("MeshModifier that uses a parsed expression (combinatorial_geometry) "
                             "to determine if an element (aka its centroid) is inside the "
                             "combinatorial geometry and "
                             "assigns a new block id.");
  return params;
}

ParsedSubdomainMeshModifier::ParsedSubdomainMeshModifier(const InputParameters & parameters)
  : MeshModifier(parameters),
    FunctionParserUtils(parameters),
    _function(parameters.get<std::string>("combinatorial_geometry")),
    _block_id(parameters.get<SubdomainID>("block_id")),
    _excluded_ids(parameters.get<std::vector<SubdomainID>>("excluded_subdomain_ids"))
{
  // base function object
  _func_F = std::make_shared<SymFunction>();

  // set FParser internal feature flags
  setParserFeatureFlags(_func_F);

  // add the constant expressions
  addFParserConstants(_func_F,
                      getParam<std::vector<std::string>>("constant_names"),
                      getParam<std::vector<std::string>>("constant_expressions"));

  // parse function
  if (_func_F->Parse(_function, "x,y,z") >= 0)
    mooseError("Invalid function\n",
               _function,
               "\nin ParsedSubdomainMeshModifier ",
               name(),
               ".\n",
               _func_F->ErrorMsg());

  _func_params.resize(3);
}

void
ParsedSubdomainMeshModifier::modify()
{
  // Check that we have access to the mesh
  if (!_mesh_ptr)
    mooseError(
        "_mesh_ptr must be initialized before calling ParsedSubdomainMeshModifier::modify()");

  // Loop over the elements
  for (const auto & elem : _mesh_ptr->getMesh().active_element_ptr_range())
  {
    _func_params[0] = elem->centroid()(0);
    _func_params[1] = elem->centroid()(1);
    _func_params[2] = elem->centroid()(2);
    bool contains = evaluate(_func_F);

    if (contains && std::find(_excluded_ids.begin(), _excluded_ids.end(), elem->subdomain_id()) ==
                        _excluded_ids.end())
      elem->subdomain_id() = _block_id;
  }

  // Assign block name, if provided
  if (isParamValid("block_name"))
    _mesh_ptr->getMesh().subdomain_name(_block_id) = getParam<SubdomainName>("block_name");
}
