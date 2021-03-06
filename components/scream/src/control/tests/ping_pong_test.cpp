#include <catch2/catch.hpp>
#include "share/atmosphere_process.hpp"
#include "share/scream_pack.hpp"
#include "share/grid/user_provided_grids_manager.hpp"
#include "share/grid/default_grid.hpp"
#include "control/atmosphere_driver.hpp"

namespace scream {

// === A dummy atm process, on Physics grid === //

template<typename DeviceType, int PackSize>
class DummyProcess : public scream::AtmosphereProcess {
public:
  using device_type = DeviceType;

  explicit DummyProcess (const ParameterList& params)
   : m_input(FieldIdentifier("INVALID",{FieldTag::Invalid}))
   , m_output(FieldIdentifier("INVALID",{FieldTag::Invalid}))
  {
    m_params = params;
  }

  // The type of the block (dynamics or physics)
  AtmosphereProcessType type () const { return AtmosphereProcessType::Physics; }

  std::set<std::string> get_required_grids () const {
    // TODO: define what grid the coupling runs on. Check with MOAB folks.
    static std::set<std::string> s;
    s.insert(e2str(GridType::Undefined));
    return s;
  }

  // Return some sort of name, linked to PType
  std::string name () const { return "Dummy process"; }

  // The communicator associated with this atm process
  const Comm& get_comm () const { return m_comm; }

  void initialize (const Comm& comm, const std::shared_ptr<const GridsManager> grids_manager) {
    m_comm = comm;
    m_id = comm.rank();
    auto size = comm.size();

    m_grid = grids_manager->get_grid("Physics");
    auto num_cols = m_grid->num_dofs();

    std::vector<FieldTag> tags = {FieldTag::Column,FieldTag::Component};
    std::vector<int> dims = {num_cols, m_params.get<int>("Number of vector components")};
    FieldLayout layout (tags,dims);

    std::string in_name = "field_" + std::to_string(m_id);
    std::string out_name = "field_" + std::to_string( (m_id + size - 1) % size );

    m_input_fids.emplace(in_name,layout,m_grid->name());
    m_output_fids.emplace(out_name,layout,m_grid->name());
  }

  void run () {
    auto in = m_input.get_view();
    auto out = m_output.get_view();
    auto id = m_id;
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0,16),
      KOKKOS_LAMBDA(const int i) {
        out(i) = sin(in(i)+id);
    });
    Kokkos::fence();
  }

  // Clean up
  void finalize ( ) {}

  // Register all fields in the given repo
  void register_fields (FieldRepository<Real, device_type>& field_repo) const {
    using pack_type = pack::Pack<Real,PackSize>;
    field_repo.template register_field<pack_type>(*m_input_fids.begin());
    field_repo.template register_field<pack_type>(*m_output_fids.begin());
  }

  // Providing a list of required and computed fields
  const std::set<FieldIdentifier>&  get_required_fields () const { return m_input_fids; }
  const std::set<FieldIdentifier>&  get_computed_fields () const { return m_output_fids; }

protected:

  // Setting the field in the atmosphere process
  void set_required_field_impl (const Field<const Real, device_type>& f) {
    error::runtime_check(f.get_header().get_identifier()==*m_input_fids.begin(),
                         "Error! This is not one of my input fields...\n");
    m_input = f;
  }
  void set_computed_field_impl (const Field<      Real, device_type>& f) {
    error::runtime_check(f.get_header().get_identifier()==*m_output_fids.begin(),
                         "Error! This is not one of my input fields...\n");
    m_output = f;
  }

  std::set<FieldIdentifier> m_input_fids;
  std::set<FieldIdentifier> m_output_fids;

  Field<const Real,device_type> m_input;
  Field<Real,device_type>       m_output;

  std::shared_ptr<AbstractGrid> m_grid;

  ParameterList m_params;
  int     m_id;

  Comm    m_comm;
};

template<typename DeviceType, int PackSize>
AtmosphereProcess* create_dummy_process (const ParameterList& p) {
  return new DummyProcess<DeviceType,PackSize>(p);
}

// === A dummy physics grids for this test === //

class DummyPhysicsGrid : public DefaultGrid<GridType::Physics>
{
public:
  DummyPhysicsGrid (const int num_cols)
   : DefaultGrid<GridType::Physics>("Physics")
  {
    m_num_dofs = num_cols;
  }
  ~DummyPhysicsGrid () = default;

protected:
};

TEST_CASE("ping-pong", "") {
  using namespace scream;
  using namespace scream::control;

  using device_type = AtmosphereDriver::device_type;

  constexpr int num_iters = 10;
  constexpr int num_cols  = 32;

  // Create a parameter list for inputs
  ParameterList ad_params("Atmosphere Driver");
  auto& proc_params = ad_params.sublist("Atmosphere Processes");

  proc_params.set("Number of Entries",2);
  proc_params.set<std::string>("Schedule Type","Sequential");

  auto& p0 = proc_params.sublist("Process 0");
  p0.set<std::string>("Process Name", "Dummy");
  p0.set<int>("Number of vector components",2);

  auto& p1 = proc_params.sublist("Process 1");
  p1.set<std::string>("Process Name", "Dummy");
  p1.set<int>("Number of vector components",2);

  auto& gm_params = ad_params.sublist("Grids Manager");
  gm_params.set<std::string>("Type","User Provided");

  // Need to register products in the factory *before* we create any AtmosphereProcessGroup,
  // which rely on factory for process creation. The initialize method of the AD does that.
  // While we're at it, check that the case insensitive key of the factory works.
  auto& proc_factory = AtmosphereProcessFactory::instance();
  proc_factory.register_product("duMmy",&create_dummy_process<device_type,SCREAM_PACK_SIZE>);

  // Need to register grids managers before we create the driver
  auto& gm_factory = GridsManagerFactory::instance();
  gm_factory.register_product("User Provided",create_user_provided_grids_manager);

  // Set the dummy grid in the UserProvidedGridManager
  // Recall that this class stores *static* members, so whatever
  // we set here, will be reflected in the GM built by the factory.
  UserProvidedGridsManager upgm;
  upgm.set_grid(std::make_shared<DummyPhysicsGrid>(num_cols));

  // Create a comm
  Comm atm_comm (MPI_COMM_WORLD);

  // Create the driver
  AtmosphereDriver ad;

  // Init, run, and finalize
  ad.initialize(atm_comm,ad_params);
  for (int i=0; i<num_iters; ++i) {
    ad.run();
  }
  ad.finalize();

  // Every atm proc does out(:) = sin(in(:)+rank)
  Real answer = 0;
  for (int i=0; i<num_iters; ++i) {
    for (int pid=0; pid<atm_comm.size(); ++pid) {
      answer = std::sin(answer+pid);
    }
  }

  // Get the field repo, and check the answer
  const auto& repo = ad.get_field_repo();

  std::vector<FieldTag> tags = {FieldTag::Column,FieldTag::Component};
  std::vector<int> dims = {num_cols, 2};
  FieldLayout layout (tags,dims);
  FieldIdentifier final_fid("field_" + std::to_string(atm_comm.size()-1),layout,"Physics");
  const auto& final_field = repo.get_field(final_fid);

  auto h_view = Kokkos::create_mirror_view(final_field.get_view());
  for (int i=0; i<h_view.extent_int(0); ++i) {
    REQUIRE (h_view(i) == answer);
  }
}

} // empty namespace
