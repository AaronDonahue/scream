#ifndef SCREAM_COMM_HPP
#define SCREAM_COMM_HPP

#include <mpi.h>

namespace scream
{

// A small wrapper around an MPI_Comm, together with its rank/size

// NOTE: this class checks that MPI is already init-ed, and errors out
//       if it is not. It is YOUR responsibility to make sure MPI is
//       init-ed before you create a scream::Comm

class Comm
{
public:

  // The default comm creates a wrapper to MPI_COMM_SELF, rather than MPI_COMM_WORLD,
  // because it is safer to assume I'm the only proc in the comm rather than assuming
  // that the whole world is in my group.
  Comm ();

  // This constructor wraps the given MPI_Comm
  explicit Comm (MPI_Comm mpi_comm);

  // This method resets the stored MPI_Comm to the given one,
  // updating m_size and m_rank accordingly.
  // WARNING: it is YOUR responsibility to ensure that
  //   1) MPI is already inited (call check_mpi_init first, if you are not sure)
  //   2) the call is collective on both the stored and input comm's
  void reset_mpi_comm (MPI_Comm new_mpi_comm);

  bool am_i_root () const { return m_rank==0; }
  int  rank () const { return m_rank; }
  int  size () const { return m_size; }
  MPI_Comm mpi_comm () const { return m_mpi_comm; }

private:
  // Checks (with an assert) that MPI is already init-ed.
  void check_mpi_inited () const;

  MPI_Comm  m_mpi_comm;

  int       m_size;
  int       m_rank;
};

} // namespace scream

#endif // SCREAM_COMM_HPP
