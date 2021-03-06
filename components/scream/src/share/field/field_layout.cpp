#include "field_layout.hpp"

namespace scream
{

FieldLayout::FieldLayout (const std::initializer_list<FieldTag>& tags)
 : m_rank(tags.size())
 , m_tags(tags)
{
  m_dims.resize(m_rank,-1);
}

FieldLayout::FieldLayout (const std::vector<FieldTag>& tags)
 : m_rank(tags.size())
 , m_tags(tags)
{
  m_dims.resize(m_rank,-1);
}

FieldLayout::FieldLayout (const std::vector<FieldTag>& tags,
                          const std::vector<int>& dims)
 : m_rank(tags.size())
 , m_tags(tags)
{
  m_dims.resize(m_rank,-1);
  set_dimensions(dims);
}

void FieldLayout::set_dimension (const int idim, const int dimension) {
  error::runtime_check(idim>=0 && idim<m_rank, "Error! Index out of bounds.", -1);
  error::runtime_check(m_dims[idim] == -1,
                       "Error! You cannot reset field dimensions once set.\n");
  error::runtime_check(dimension>0, "Error! Dimensions must be positive.");
  m_dims[idim] = dimension;
}

void FieldLayout::set_dimensions (const std::vector<int>& dims) {
  // Check, then set dims
  error::runtime_check(dims.size()==static_cast<size_t>(m_rank),
                       "Error! Input dimensions vector not properly sized.");
  for (int idim=0; idim<m_rank; ++idim) {
    set_dimension(idim,dims[idim]);
  }
}

} // namespace scream
