#ifndef SCREAM_FIELD_HPP
#define SCREAM_FIELD_HPP

#include "field_header.hpp"
#include "share/scream_types.hpp"
#include "share/scream_kokkos_meta.hpp"
#include "share/util/scream_std_meta.hpp"
#include "share/util/scream_kokkos_utils.hpp"

#include <memory>   // For std::shared_ptr

namespace scream
{

template<typename FieldType>
struct is_scream_field : public std::false_type {};

// ======================== FIELD ======================== //

// A field should be composed of metadata info (the header) and a pointer to the view
// TODO: make a FieldBase class, without the view (just meta-data), without templating on
//       the device type. Then make Field inherit from FieldBase, templating it on the
//       device type. This way, we can pass FieldBase around, and the individual atm
//       atm processes will try to cast to Field<Real,MyDeviceType>. This way, we can
//       allow different atm processes to run on different device types.

template<typename ScalarType, typename Device>
class Field {
public:

  using device_type          = Device;
  using view_type            = typename KokkosTypes<device_type>::template view<ScalarType*>;
  using value_type           = typename view_type::traits::value_type;
  using const_value_type     = typename view_type::traits::const_value_type;
  using non_const_value_type = typename view_type::traits::non_const_value_type;
  using const_field_type     = Field<const_value_type, device_type>;
  using header_type          = FieldHeader;
  using identifier_type      = header_type::identifier_type;

  // Statically check that ScalarType is not an array.
  static_assert(view_type::Rank==1, "Error! ScalarType should not be an array type.\n");

  // Constructor(s)
  Field () = delete;
  explicit Field (const identifier_type& id);

  // This constructor allows const->const, nonconst->nonconst, and nonconst->const copies
  template<typename SrcDT>
  Field (const Field<SrcDT,device_type>& src);

  // Assignment: allows const->const, nonconst->nonconst, and nonconst->const copies
  template<typename SrcDT>
  Field& operator= (const Field<SrcDT,device_type>& src);

  // ---- Getters ---- //
  const header_type& get_header () const { return *m_header; }
        header_type& get_header ()       { return *m_header; }
  const std::shared_ptr<header_type>& get_header_ptr () const { return m_header; }

  const view_type&   get_view   () const { return  m_view;   }

  template<typename DT>
  ko::Unmanaged<typename KokkosTypes<device_type>::template view<DT> >
  get_reshaped_view () const;

  bool is_allocated () const { return m_allocated; }

  // ---- Setters ---- //

  // Allocate the actual view
  void allocate_view ();

protected:

  // Metadata (name, rank, dims, customere/providers, time stamp, ...)
  std::shared_ptr<header_type>    m_header;

  // Actual data.
  view_type                       m_view;

  // Keep track of whether the field has been allocated
  bool                            m_allocated;
};

template<typename ScalarType, typename DeviceType>
struct is_scream_field<Field<ScalarType,DeviceType>> : public std::true_type {};

// ================================= IMPLEMENTATION ================================== //

template<typename ScalarType, typename Device>
Field<ScalarType,Device>::
Field (const identifier_type& id)
 : m_header    (new header_type(id))
 , m_allocated (false)
{
  // At the very least, the allocation properties need to accommodate this field's value_type.
  m_header->get_alloc_properties().request_value_type_allocation<value_type>();
}

template<typename ScalarType, typename Device>
template<typename SrcScalarType>
Field<ScalarType,Device>::
Field (const Field<SrcScalarType,Device>& src)
 : m_header    (src.get_header_ptr())
 , m_view      (src.get_view())
 , m_allocated (src.is_allocated())
{
  using src_field_type = Field<SrcScalarType,Device>;

  // Check that underlying value type
  static_assert(std::is_same<non_const_value_type,
                             typename src_field_type::non_const_value_type
                            >::value,
                "Error! Cannot use copy constructor if the underlying value_type is different.\n");
  // Check that destination is const or source is nonconst
  static_assert(std::is_same<value_type,const_value_type>::value ||
                std::is_same<typename src_field_type::value_type,non_const_value_type>::value,
                "Error! Cannot create a nonconst field from a const field.\n");
}

template<typename ScalarType, typename Device>
template<typename SrcScalarType>
Field<ScalarType,Device>&
Field<ScalarType,Device>::
operator= (const Field<SrcScalarType,Device>& src) {

  using src_field_type = decltype(src);
#ifndef CUDA_BUILD // TODO Figure out why nvcc isn't like this bit of code.
  // Check that underlying value type
  static_assert(std::is_same<non_const_value_type,
                             typename src_field_type::non_const_value_type
                            >::value,
                "Error! Cannot use copy constructor if the underlying value_type is different.\n");
  // Check that destination is const or source is nonconst
  static_assert(std::is_same<value_type,const_value_type>::value ||
                std::is_same<typename src_field_type::value_type,non_const_value_type>::value,
                "Error! Cannot create a nonconst field from a const field.\n");
#endif
  if (&src!=*this) {
    m_header    = src.get_header_ptr();
    m_view      = src.get_view();
    m_allocated = src.is_allocated();
  }

  return *this;
}

template<typename ScalarType, typename Device>
template<typename DT>
ko::Unmanaged<typename KokkosTypes<Device>::template view<DT> >
Field<ScalarType,Device>::get_reshaped_view () const {
  // The dst value types
  using DstValueType = typename util::ValueType<DT>::type;

  // Get src details
  const auto& alloc_prop = m_header->get_alloc_properties();
  const auto& field_layout = m_header->get_identifier().get_layout();

  // Make sure input field is allocated
  error::runtime_check(m_allocated, "Error! Cannot reshape a field that has not been allocated yet.\n");

  // Make sure DstDT has an eligible rank: can only reinterpret if the data type rank does not change or if either src or dst have rank 1.
  constexpr int DstRank = util::GetRanks<DT>::rank;

  // Check the reinterpret cast makes sense for the two value types (need integer sizes ratio)
  error::runtime_check(alloc_prop.template is_allocation_compatible_with_value_type<DstValueType>(),
                       "Error! Source field allocation is not compatible with the destination field's value type.\n");

  // The destination view type
  using DstView = ko::Unmanaged<typename KokkosTypes<Device>::template view<DT> >;
  typename DstView::traits::array_layout kokkos_layout;

  const int num_values = alloc_prop.get_alloc_size() / sizeof(DstValueType);
  if (DstRank==1) {
    // We are staying 1d, possibly changing the data type
    kokkos_layout.dimension[0] = num_values;
  } else {
    int num_last_dim_values = num_values;
    // The destination data type is a multi-dimensional array.
    for (int i=0; i<field_layout.rank()-1; ++i) {
      kokkos_layout.dimension[i] = field_layout.dim(i);

      // Safety check: field_layout.dim(0)*...*field_layout.dim(field_layout.rank()-2) should divide num_values, so we check
      error::runtime_check(num_last_dim_values % field_layout.dim(i) == 0, "Error! Something is wrong with the allocation properties.\n");
      num_last_dim_values /= field_layout.dim(i);
    }
    kokkos_layout.dimension[field_layout.rank()-1] = num_last_dim_values;
  }

  return DstView (reinterpret_cast<DstValueType*>(m_view.data()),kokkos_layout);
}

template<typename ScalarType, typename Device>
void Field<ScalarType,Device>::allocate_view ()
{
  // Not sure if simply returning would be safe enough. Re-allocating
  // would definitely be error prone (someone may have already gotten
  // a subview of the field). However, it *seems* suspicious to call
  // this method twice, and I think it's more likely than not that
  // such a scenario would indicate a bug. Therefore, I am prohibiting it.
  error::runtime_check(!m_allocated, "Error! View was already allocated.\n");

  // Short names
  const auto& id     = m_header->get_identifier();
  const auto& layout = id.get_layout();
  auto& alloc_prop   = m_header->get_alloc_properties();

  // Check the identifier has all the dimensions set
  error::runtime_check(layout.are_dimensions_set(), "Error! Cannot create a field until all the field's dimensions are set.\n");

  // Commit the allocation properties
  alloc_prop.commit();

  // Create the view, by quering allocation properties for the allocation size
  const int view_dim = alloc_prop.get_alloc_size() / sizeof(value_type);

  m_view = view_type(id.name(),view_dim);

  m_allocated = true;
}

} // namespace scream

#endif // SCREAM_FIELD_HPP
