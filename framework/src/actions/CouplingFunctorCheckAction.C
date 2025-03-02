//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "CouplingFunctorCheckAction.h"
#include "Kernel.h"
#include "MooseApp.h"
#include "FEProblemBase.h"
#include "NonlinearSystemBase.h"

#include "libmesh/system.h"

registerMooseAction("MooseApp", CouplingFunctorCheckAction, "coupling_functor_check");

template <>
InputParameters
validParams<CouplingFunctorCheckAction>()
{
  return validParams<Action>();
}

CouplingFunctorCheckAction::CouplingFunctorCheckAction(InputParameters parameters)
  : Action(parameters)
{
}

void
CouplingFunctorCheckAction::act()
{
  // If we're doing Jacobian-free, then we have no matrix and we can just return
  if (_problem->solverParams()._type == Moose::ST_JFNK)
    return;

  auto & nl = _problem->getNonlinearSystemBase();
  auto & kernels = nl.getKernelWarehouse();
  auto & nbcs = nl.getNodalBCWarehouse();
  auto & ibcs = nl.getIntegratedBCWarehouse();

  // If we have any of these, we need default coupling (e.g. element intra-dof coupling)
  if (kernels.size() || nbcs.size() || ibcs.size())
  {
    auto original_size = _app.relationshipManagers().size();

    // It doesn't really matter what T is in validParams<T> as long as it will add our default
    // coupling functor object (ElementSideNeighborLayers with n_levels of 0)
    addRelationshipManagers(Moose::RelationshipManagerType::COUPLING, validParams<Kernel>());

    // See whether we've actually added anything new
    if (original_size != _app.relationshipManagers().size())
    {
      // There's no concern of duplicating functors previously added here since functors are stored
      // as std::sets
      _app.attachRelationshipManagers(Moose::RelationshipManagerType::COUPLING);

      // Make sure that coupling matrices are attached to the coupling functors
      _app.dofMapReinitForRMs();

      // Reinit the libMesh (Implicit)System. This re-computes the sparsity pattern and then applies
      // it to the ImplicitSystem's matrices. Note that does NOT make a call to DofMap::reinit,
      // hence we have to call GhostingFunctor::dofmap_reinit ourselves in the call above
      nl.system().reinit();
    }
  }
}
