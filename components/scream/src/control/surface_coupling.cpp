#include "surface_coupling.hpp"

namespace scream {

SurfaceCoupling::SurfaceCoupling (const ParameterList& /*params*/)
{
  // Grab what you need from the parameter list
}

void SurfaceCoupling::initialize (const Comm& comm, const std::shared_ptr<const GridsManager> /* grids_manager */) {
  m_comm = comm;
  // Initialize the FieldRepository (FR) for host fields
  // (i.e., fields that are I/O w.r.t the coupler)
}

void SurfaceCoupling::run ( /* inputs ? */ ) {
  // Recall that the surface coupling can (and usually does) happen
  // in the middle of an atm time step. Therefore, we first export
  // atm output fields to the coupler, then import atm input fields
  // The order is in a way pointless (the same fields should not be
  // both exported and imported), but keeping this order makes it
  // more coherent with what is happening.

  // Copy (device->host) data to coupler views

  // Copy (host->device) data from coupler views
}

void SurfaceCoupling::finalize ( /* inputs? */ ) {
  // clean up
}

void SurfaceCoupling::register_fields (FieldRepository<Real, device_type>& /*field_repo*/) const {
  // register device fields in the repo
}

void SurfaceCoupling::set_required_field_impl (const Field<const Real, device_type>& /*f*/) {
  /* impl */
}

void SurfaceCoupling::set_computed_field_impl (const Field<      Real, device_type>& /*f*/) {
  /* impl */
}

}  // namespace scream
