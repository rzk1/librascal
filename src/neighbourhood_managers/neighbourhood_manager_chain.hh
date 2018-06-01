/**
 * file   neighbourhood_manager_chain.hh
 *
 * @author Markus Stricker <markus.stricker@epfl.ch>
 *
 * @date   30 May 2018
 *
 * @brief Neighbourhood manager for polyalanine chain, reading
 *        structure from json file
 *
 * Copyright © 2018 Markus Stricker, COSMO (EPFL), LAMMM (EPFL)
 *
 * rascal is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3, or (at
 * your option) any later version.
 *
 * rascal is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Emacs; see the file COPYING. If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifndef NEIGHBOURHOOD_MANAGER_CHAIN_H
#define NEIGHBOURHOOD_MANAGER_CHAIN_H

#include "neighbourhood_managers/neighbourhood_manager_base.hh"
#include "neighbourhood_managers/property.hh"

#include <Eigen/Dense>

#include <stdexcept>
#include <vector>

namespace rascal {
  //! forward declaration for traits
  class NeighbourhoodManagerChain;

  //! traits specialisation for Chain manager
  template <>
  struct NeighbourhoodManager_traits<NeighbourhoodManagerChain> {
    constexpr static int Dim{3};
    constexpr static int MaxLevel{3}; // triplets needed for angle
  };
  class NeighbourhoodManagerChain:
    public NeighbourhoodManagerBase<NeighbourhoodManagerChain>
  {
  public:
    using traits = NeighbourhoodManager_traits<NeighbourhoodManagerChain>;
    using Parent = NeighbourhoodManagerBase<NeighbourhoodManagerChain>;
    using Vector_ref = typename Parent::Vector_ref;
    using AtomRef_t = typename Parent::AtomRef;

    // JSON-specific and respective maps
    using PositionJson_t = std::vector<std::vector<double>>;
    using Position_ref = Eigen::Map<PositionJson_t>;
    using AtomTypeJson_t = std::vector<int>;
    using AtomType_ref = Eigen::Map<AtomTypeJson_t>;
    using CellJson_t = std::vector<std::vector<double>>;
    using CellType_ref = Eigen::Map<CellJson_t>;
    using PBCJson_t = std<vector<bool>>;
    using PBC_ref = Eigen::Map<PBCJson_t>;


    using NeighbourList_t = Eigen::Matrix<int, Eigen::Dynamic, Eigen::Dynamic>;
    using Numneigh_t = Eigen::Matrix<int, Eigen::Dynamic>;
    using Ilist_t = Eigen::Matrix<int, Eigen::Dynamic>;
    template <int Level, int MaxLevel>
    using ClusterRef_t = typename Parent::template ClusterRef<Level, MaxLevel>;

    //! Default constructor
    NeighbourhoodManagerChain() = default;

    //! Copy constructor
    NeighbourhoodManagerChain(const NeighbourhoodManagerChain &other) = delete;

    //! Move constructor
    NeighbourhoodManagerChain(NeighbourhoodManagerChain &&other) = default;

    //! Destructor
    virtual ~NeighbourhoodManagerChain() = default;

    //! Copy assignment operator
    NeighbourhoodManagerChain&
    operator=(const NeighbourhoodManagerChain &other) = delete;

    //! Move assignment operator
    NeighbourhoodManagerChain&
    operator=(NeighbourhoodManagerChain &&other) = default;

    //! something like this!
    void reset_impl(const int & natoms);
    // void reset_impl(int * ilist, int * numneigh, int ** firstneigh,
    //                 double ** x, double ** f, int * type,
    //                 double * eatom, double ** vatom);
    // required for the construction of vectors, etc
    // TODO
    constexpr static int dim() {return traits::Dim;}


    // return position vector
    inline Vector_ref get_position(const AtomRef_t& atom) {
      // auto index{atom.get_index()};
      // auto * xval{this->x[index]};
      // return Vector_ref(xval);
      // TODO
    }

    // return neighbour positions if Level > 1, temporary
    inline Vector_ref get_neighbour_position(const AtomRef_t& atom,
					     const AtomRef_t&,
					     const int&) {
      // auto index{atom.get_index()};
      // auto * xval{this->x[index]};
      // return Vector_ref(xval);
      // TODO
    }

    // return force vector

    // return position vector
    inline int get_atom_type(const AtomRef_t& atom) {
      // auto index{atom.get_index()};
      // return this->type[index];
      // TODO
    }


    // return number of I atoms in the list
    inline size_t get_size() const {
      return this->natoms;
    }

    // return the number of neighbours of a given atom
    template<int Level, int MaxLevel>
    inline size_t get_cluster_size(const ClusterRef_t<Level,
				   MaxLevel>& cluster) const {
      static_assert(Level == traits::MaxLevel-1,
                    "this implementation only handles atoms, "
		    "pairs and triplets");
      return this->numneigh[cluster.get_atoms().back().get_index()];
    }

    // return the number of atoms forming the next higher cluster with this one
    template<int Level, int MaxLevel>
    inline size_t get_atom_id(const ClusterRef_t<Level, MaxLevel>& cluster,
                              int j_atom_id) const {
      static_assert(Level == traits::MaxLevel-1,
                    "this implementation only handles atoms, pairs "
		    "and triplets");
      auto && i_atom_id{cluster.get_atoms().back().get_index()};
      return this->firstneigh[std::move(i_atom_id)][j_atom_id];
    }

    // return the number of neighbours of a given atom
    inline size_t get_atom_id(const Parent& /*cluster*/,
                              int i_atom_id) const {
      return this->ilist[i_atom_id];
    }

    /**
     * return the linear index of cluster (i.e., the count at which
     * this cluster appears in an iteration
     */
    template<int Level, int MaxLevel>
    inline int
    get_offset_impl(const ClusterRef_t<Level, MaxLevel>& cluster) const;

    size_t get_nb_clusters(int cluster_size);

    PositionJson_t read_structure_from_json();

  protected:
    // Given by example json file (for transfer from JSON
    PositionJson_t position;
    AtomTypeJson_t type; // atom type - according to atomic number in periodic table
    CellJson_t cell; // unit cell of structure
    PBCJson_t pbc; // boolean for periodicity of unit cell

    // To be initialized by contruction of manager for actual neighbour use
    size_t natoms{}; // total number of atoms in structure
    size_t nb_pairs{};
    size_t nb_triplets{};
    Ilist_t ilist; // adhering to lammps-naming
    NeighbourList_t firstneigh; // adhering to lammps-naming
    NumNeigh_t numneigh; // adhering to lammps-naming


  private:
  };


  /* ---------------------------------------------------------------------- */
  // adjust for triplets
  template<int Level, int MaxLevel>
  inline int NeighbourhoodManagerChain::
  get_offset_impl(const ClusterRef_t<Level, MaxLevel>& cluster) const {
    static_assert(Level == 3,
		  "This class cas only handle single atoms, pairs "
		  "and triplets");
    static_assert(MaxLevel == traits::MaxLevel, "Wrong maxlevel");

    auto atoms{cluster.get_atoms()};
    auto i{atoms.front().get_index()};
    auto j{cluster.get_index()};
    auto main_offset{this->offsets[i]};
    return main_offset + j;
  }

  /* ---------------------------------------------------------------------- */
  // specialisation for just atoms
  template <>
  inline int NeighbourhoodManagerChain:: template
  get_offset_impl<1, 2>(const ClusterRef_t<1, 2>& cluster) const {
    return cluster.get_atoms().back().get_index();
  }

}  // rascal

#endif /* NEIGHBOURHOOD_MANAGER_CHAIN_H */
