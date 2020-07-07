/**
 * file   behler_feature_impl.hh
 *
 * @author Till Junge <till.junge@epfl.ch>
 *
 * @date   15 Jan 2019
 *
 * @brief  implementation for input node contribution tools
 *
 * Copyright © 2019 Till Junge, COSMO (EPFL), LAMMM (EPFL)
 *
 * librascal is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3, or (at
 * your option) any later version.
 *
 * librascal is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with librascal; see the file COPYING. If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef SRC_RASCAL_REPRESENTATIONS_BEHLER_FEATURE_IMPL_HH_
#define SRC_RASCAL_REPRESENTATIONS_BEHLER_FEATURE_IMPL_HH_

#include "rascal/utils/for_each_at_order.hh"

namespace rascal {
  /* ---------------------------------------------------------------------- */
  template <SymmetryFunctionType... SymFunTypes>
  template <SymmetryFunctionType... SymFunTypes_>
  struct BehlerFeatureBase<SymFunTypes...>::SymFunctionsVTable {};

  /* ---------------------------------------------------------------------- */
  template <size_t Order, SymmetryFunctionType Head,
            SymmetryFunctionType... SymFunTypes>
  struct BehlerFeatureOrderSelector {};
  /* ---------------------------------------------------------------------- */
  template <SymmetryFunctionType Head, SymmetryFunctionType... SymFunTypes>
  struct BehlerFeatureOrderSelector<PairOrder, Head, SymFunTypes...> {
    using type = BehlerPairFeature<Head, SymFunTypes...>;
  };
  /* ---------------------------------------------------------------------- */
  template <SymmetryFunctionType Head, SymmetryFunctionType... SymFunTypes>
  struct BehlerFeatureOrderSelector<TripletOrder, Head, SymFunTypes...> {
    using type = BehlerTripletFeature<Head, SymFunTypes...>;
  };

  template <size_t Order, SymmetryFunctionType Head,
            SymmetryFunctionType... SymFunTypes>
  using BehlerFeatureOrderSelector_t =
      typename BehlerFeatureOrderSelector<Order, Head, SymFunTypes...>::type;

  /* ---------------------------------------------------------------------- */
  template <SymmetryFunctionType... SymFunTypes>
  template <SymmetryFunctionType Head, SymmetryFunctionType... Tail>
  struct BehlerFeatureBase<SymFunTypes...>::SymFunctionsVTable<Head, Tail...> {
    template <RepeatedSpecies RepSpecies, typename Permutation,
              class StructureManager, class... PropertyPtr>
    static void compute(const BehlerFeatureBase & behler_feature,
                        StructureManager & manager, PropertyPtr... outputs) {
      if (behler_feature.sym_fun_type == Head) {
        constexpr auto Order{SymmetryFunction<Head>::Order};
        using Feature_t =
            BehlerFeatureOrderSelector_t<Order, Head, SymFunTypes...>;
        auto & feature{dynamic_cast<const Feature_t &>(behler_feature)};
        feature.compute_helper(manager, outputs...);
      } else {
        SymFunctionsVTable<Tail...>::template compute<RepSpecies, Permutation>(
            behler_feature, manager, outputs...);
      }
    }
  };

  /**
   * Recursion end: return the last remaining function call or throw a
   * runtime_error
   */
  template <SymmetryFunctionType... SymFunTypes>
  template <SymmetryFunctionType Head>
  struct BehlerFeatureBase<SymFunTypes...>::SymFunctionsVTable<Head> {
    template <RepeatedSpecies RepSpecies, typename Permutation,
              class StructureManager, class... PropertyPtr>
    static void compute(const BehlerFeatureBase & behler_feature,
                        StructureManager & manager, PropertyPtr... outputs) {
      if (behler_feature.sym_fun_type == Head) {
        constexpr auto Order{SymmetryFunction<Head>::Order};
        using Feature_t =
            BehlerFeatureOrderSelector_t<Order, Head, SymFunTypes...>;
        auto & feature{dynamic_cast<const Feature_t &>(behler_feature)};
        feature.template compute_helper<RepSpecies, Permutation>(manager,
                                                                 outputs...);
      } else {
        std::stringstream err{};
        err << "Symmetry function type " << behler_feature.sym_fun_type
            << " is not known.";
        throw std::runtime_error(err.str());
      }
    }
  };

  /* ---------------------------------------------------------------------- */
  template <SymmetryFunctionType... SymFunTypes>
  template <RepeatedSpecies RepSpecies, typename Permutation,
            class StructureManager>
  void BehlerFeatureBase<SymFunTypes...>::compute(
      StructureManager & manager, std::shared_ptr<PropertyBase> prop) const {
    SymFunctionsVTable<SymFunTypes...>::template compute<RepSpecies,
                                                         Permutation>(
        *this, manager, prop);
  }

  /* ---------------------------------------------------------------------- */
  template <SymmetryFunctionType... SymFunTypes>
  template <RepeatedSpecies RepSpecies, typename Permutation,
            class StructureManager>
  void BehlerFeatureBase<SymFunTypes...>::compute(
      StructureManager & manager, std::shared_ptr<PropertyBase> prop,
      std::shared_ptr<PropertyBase> prop_self_der,
      std::shared_ptr<PropertyBase> prop_other_der) const {
    SymFunctionsVTable<SymFunTypes...>::template compute<RepSpecies,
                                                         Permutation>(
        *this, manager, prop, prop_self_der, prop_other_der);
  }

  /* ---------------------------------------------------------------------- */
  // todo(jungestricker): outsource the calculation of multiplication of sf
  // value and cutoff value to symmetry function for consistency with
  // BehlerTripletFeature?!
  template <SymmetryFunctionType MySymFunType,
            SymmetryFunctionType... SymFunTypes>
  template <RepeatedSpecies RepSpecies, typename Permutation,
            class StructureManager>
  void BehlerPairFeature<MySymFunType, SymFunTypes...>::compute_helper(
      StructureManager & manager, std::shared_ptr<PropertyBase> output) const {
    static_assert(Permutation::Size == Order,
                  "Permutation size needs to equal cluster order");
    auto & cutoffs{this->cut_fun->get_pair_value(manager)};

    // eval
    using Output_t = Property<double, AtomOrder, StructureManager>;
    Output_t & fun_vals{dynamic_cast<Output_t &>(*output)};
    auto & pair_distances{manager.get_distance()};

    fun_vals.resize();

    auto & neigh_to_i_atom{
        manager.template get_neighbours_to_i_atoms<PairOrder>()};
    for (auto && atom : manager) {
      for (auto && pair : atom.pairs()) {
        // compute the increment to the G function value
        auto && G_incr{this->sym_fun.f_sym(pair_distances[pair]) *
                       cutoffs[pair]};

        auto && atom_cluster_indices{neigh_to_i_atom[pair]};
        auto && i_atom{manager[atom_cluster_indices(Permutation::leading())]};

        switch (RepSpecies) {
        case RepeatedSpecies::Not: {
          fun_vals[i_atom] += G_incr;
          break;
        }

        case RepeatedSpecies::All: {
          auto && j_atom{manager[atom_cluster_indices(Permutation::second())]};
          fun_vals[i_atom] += G_incr;
          fun_vals[j_atom] += G_incr;
          break;
        }

        default:
          throw std::runtime_error("Unknown species repetition pattern");
          break;
        }
      }
    }
  }

  /* ---------------------------------------------------------------------- */
  template <SymmetryFunctionType MySymFunType,
            SymmetryFunctionType... SymFunTypes>
  template <RepeatedSpecies RepSpecies, typename Permutation,
            class StructureManager>
  void BehlerTripletFeature<MySymFunType, SymFunTypes...>::compute_helper(
      StructureManager & manager, std::shared_ptr<PropertyBase> output) const {
    static_assert(Permutation::Size == Order,
                  "Permutation size needs to equal cluster order");

    auto && triplet_cutoffs{this->cut_fun->get_triplet_value(manager)};

    // eval
    using Output_t = Property<double, AtomOrder, StructureManager>;
    Output_t & fun_vals{dynamic_cast<Output_t &>(*output)};
    auto & triplet_distances{manager.get_triplet_distance()};
    auto & cos_angles{get_cos_angles(manager)};

    fun_vals.resize();
    auto & neigh_to_i_atom{
        manager.template get_neighbours_to_i_atoms<TripletOrder>()};

    auto orderings{Permutation::template get_triplet_orderings<RepSpecies>()};
    for (auto && atom : manager) {
      for (auto && triplet : atom.triplets()) {
        auto && trip_dist{triplet_distances[triplet]};
        auto && trip_cutoffs{triplet_cutoffs[triplet]};
        auto && trip_cos{cos_angles[triplet]};
        auto && atom_cluster_indices{neigh_to_i_atom[triplet]};

        for (auto && ordering : orderings) {
          auto && G_incr{
              this->sym_fun.f_sym(trip_cos, trip_dist, trip_cutoffs, ordering)};
          auto && i_atom{manager[atom_cluster_indices(ordering[0])]};
          fun_vals[i_atom] += G_incr;
        }
      }
    }
  }

  /* ---------------------------------------------------------------------- */
  template <SymmetryFunctionType MySymFunType,
            SymmetryFunctionType... SymFunTypes>
  template <RepeatedSpecies RepSpecies, typename Permutation,
            class StructureManager>
  void BehlerPairFeature<MySymFunType, SymFunTypes...>::compute_helper(
      StructureManager & manager, std::shared_ptr<PropertyBase> output_values,
      std::shared_ptr<PropertyBase> output_self_derivatives,
      std::shared_ptr<PropertyBase> output_other_derivatives) const {
    auto && cutoff_tup{this->cut_fun->get_pair_derivative(manager)};
    auto & cutoff_values{std::get<0>(cutoff_tup)};
    auto & cutoff_derivatives{std::get<1>(cutoff_tup)};

    // eval
    using OutputVal_t = Property<double, AtomOrder, StructureManager>;
    OutputVal_t & fun_vals{dynamic_cast<OutputVal_t &>(*output_values)};

    constexpr auto Order{SymmetryFunction_t::Order};
    using OutputSelfDerivative_t =
        Property<double, AtomOrder, StructureManager, ThreeD>;
    OutputSelfDerivative_t & fun_self_derivatives{
        dynamic_cast<OutputSelfDerivative_t &>(*output_self_derivatives)};

    using OutputOtherDerivative_t =
        Property<double, PairOrder, StructureManager, ThreeD, 2>;
    OutputOtherDerivative_t & fun_other_derivatives{
        dynamic_cast<OutputOtherDerivative_t &>(*output_other_derivatives)};

    auto & pair_distances{manager.get_distance()};
    auto & pair_direction_vectors{manager.get_direction_vector()};

    auto & neigh_to_i_atom{
        manager
            .template get_neighbours_to_i_atoms<SymmetryFunction_t::Order>()};

    fun_vals.resize();
    fun_self_derivatives.resize();
    fun_other_derivatives.resize();

    auto && pair_inversion{Permutation::pair_inversion()[0]};
    for (auto && atom : manager) {
      for (auto && pair : atom.pairs()) {
        // compute the increment to the G function value
        auto && sym_fun_tup{this->sym_fun.df_sym(pair_distances[pair])};
        double & sym_fun_value{std::get<0>(sym_fun_tup)};
        double & sym_fun_derivative{std::get<1>(sym_fun_tup)};
        double & cut_fun_value{cutoff_values[pair]};
        double & cut_fun_derivative{cutoff_derivatives[pair]};

        auto && G_incr{sym_fun_value * cut_fun_value};

        auto && atom_cluster_indices{neigh_to_i_atom[pair]};
        auto && i_atom{manager[atom_cluster_indices(Permutation::leading())]};

        auto && dir_vec{pair_direction_vectors[pair] *
                        (pair_inversion ? -1 : 1)};

        auto && dG_incr{dir_vec * (sym_fun_value * cut_fun_derivative +
                                   sym_fun_derivative * cut_fun_value)};

        switch (RepSpecies) {
        case RepeatedSpecies::Not: {
          fun_vals[i_atom] += G_incr;
          fun_self_derivatives[i_atom] += dG_incr;
          fun_other_derivatives[pair].col(pair_inversion) -= dG_incr;
          break;
        }

        case RepeatedSpecies::All: {
          fun_vals[i_atom] += G_incr;
          auto && j_atom{manager[atom_cluster_indices(Permutation::second())]};
          fun_vals[j_atom] += G_incr;

          fun_self_derivatives[i_atom] += dG_incr;
          fun_self_derivatives[j_atom] += -dG_incr;

          fun_other_derivatives[pair].col(pair_inversion) -= dG_incr;
          fun_other_derivatives[pair].col(not pair_inversion) -= -dG_incr;
          break;
        }

        default:
          throw std::runtime_error("Unknown species repetition pattern");
          break;
        }
      }
    }
  }

  /* ---------------------------------------------------------------------- */
  template <SymmetryFunctionType MySymFunType,
            SymmetryFunctionType... SymFunTypes>
  template <RepeatedSpecies RepSpecies, typename Permutation,
            class StructureManager>
  void BehlerTripletFeature<MySymFunType, SymFunTypes...>::compute_helper(
      StructureManager & manager, std::shared_ptr<PropertyBase> output_values,
      std::shared_ptr<PropertyBase>
          output_self_derivatives,  // vectorial atom-property
      std::shared_ptr<PropertyBase> output_other_derivatives)
      const {  // bi-vectorial pair-property (forwards and backwards)
    auto && cutoff_tup{this->cut_fun->get_triplet_derivative(manager)};
    auto & cutoff_values{std::get<0>(cutoff_tup)};
    auto & cutoff_derivatives{std::get<1>(cutoff_tup)};

    // eval
    using OutputVal_t = Property<double, AtomOrder, StructureManager>;
    OutputVal_t & fun_vals{dynamic_cast<OutputVal_t &>(*output_values)};

    using OutputSelfDerivative_t =
        Property<double, AtomOrder, StructureManager, ThreeD>;
    OutputSelfDerivative_t & fun_self_derivatives{
        dynamic_cast<OutputSelfDerivative_t &>(*output_self_derivatives)};

    using OutputOtherDerivative_t =
        Property<double, PairOrder, StructureManager, ThreeD, 2>;
    OutputOtherDerivative_t & fun_other_derivatives{
        dynamic_cast<OutputOtherDerivative_t &>(*output_other_derivatives)};

    auto & triplet_distances{manager.get_triplet_distance()};
    auto & direction_vectors{manager.get_direction_vector()};
    auto & cos_angles{get_cos_angles(manager)};

    auto & neigh_to_i_atom{
        manager
            .template get_neighbours_to_i_atoms<SymmetryFunction_t::Order>()};

    fun_vals.resize();
    fun_self_derivatives.resize();
    fun_other_derivatives.resize();

    const auto ordering_weight{Permutation::template get_triplet_orderings<
        RepSpecies,
        SymmetryFunction<MySymFunType>::jk_are_indistinguishable()>()};

    const auto & orderings{std::get<0>(ordering_weight)};
    const auto & weight{std::get<1>(ordering_weight)};

    auto && pairs_container{
        manager.template get_sub_clusters<PairOrder, TripletOrder>()};
    for (auto && atom : manager) {
      for (auto && triplet : atom.triplets()) {
        auto && trip_cos{cos_angles[triplet]};
        auto && trip_dist{triplet_distances[triplet]};
        auto && trip_cutoffs{cutoff_values[triplet]};
        auto && trip_cutoffs_derivatives{cutoff_derivatives[triplet]};

        auto && atom_cluster_indices{neigh_to_i_atom[triplet]};
        // get the pairs in each triplet

        auto && triplet_pairs{pairs_container[triplet]};

        for (auto && ordering_inversion : orderings) {
          auto && ordering{std::get<0>(ordering_inversion)};
          auto && inversion{std::get<1>(ordering_inversion)};
          auto && G_tup{this->sym_fun.df_sym(trip_cos, trip_dist, trip_cutoffs,
                                             trip_cutoffs_derivatives,
                                             ordering)};
          auto && G_incr{std::get<0>(G_tup)};
          auto && dG_incr{std::get<1>(G_tup)};

          using AtomClusterRef_t =
              typename StructureManager::template ClusterRef<AtomOrder>;
          std::array<AtomClusterRef_t, 3> atoms{
              manager[atom_cluster_indices(ordering[0])],
              manager[atom_cluster_indices(ordering[1])],
              manager[atom_cluster_indices(ordering[2])]};

          using PairClusterRef_t =
              typename StructureManager::template ClusterRef<PairOrder>;
          std::array<PairClusterRef_t, 3> pairs{triplet_pairs[ordering[0]],
                                                triplet_pairs[ordering[1]],
                                                triplet_pairs[ordering[2]]};

          fun_vals[atoms[0]] += weight * G_incr;
          for (int id{0}; id < pairs.size(); ++id) {
            auto && dir_vec{direction_vectors[pairs[id]] *
                            (inversion[id] ? -1 : 1)};
            auto && dG_incr_vec{weight * dir_vec * dG_incr[ordering[id]]};
            // contribution
            fun_self_derivatives[atoms[id]] += dG_incr_vec;

            fun_other_derivatives[pairs[id]].col(inversion[id]) -= dG_incr_vec;
          }
        }
      }
    }

    // throw std::runtime_error("Not yet implemented");
  }

  /* ---------------------------------------------------------------------- */
  template <SymmetryFunctionType MySymFunType,
            SymmetryFunctionType... SymFunTypes>
  void BehlerPairFeature<MySymFunType, SymFunTypes...>::init(
      const UnitStyle & /*units*/) {
    // if (this->is_initialised) {
    //   throw std::runtime_error("double initialisation");
    // }
    // // counting the number of parameters to store per cutoff
    // for (const auto & param : this->raw_params) {
    //   auto && r_cut{param.at("r_cut").template get<double>()};
    //   nb_param_per_cutoff[r_cut]++;
    // }

    // // allocate storage
    // for (const auto & key_val : nb_param_per_cutoff) {
    //   auto && r_cut{key_val.first};
    //   this->params[r_cut].resize(SymmetryFun<SymFunType>::NbParams,
    //                              nb_param_per_cutoff[r_cut]);
    //   this->params[r_cut].setZero();
    // }

    // // store params in storage
    // std::map<double, size_t> nb_param_counter{};
    // for (auto && param : this->raw_params) {
    //   auto && r_cut{param.at("r_cut").template get<double>()};

    //   this->params[r_cut].col(nb_param_counter[r_cut]++) =
    //       SymmetryFun<SymFunType>::read(param, units);
    // }

    // // determine which species repetition scenario we are in
    // for (auto && species : param.at("species").template get<std::string>())
    // {
    //   this->species_combo
    // }
    // auto species{};

    // this->is_initialised = true;
  }
  template <SymmetryFunctionType MySymFunType,
            SymmetryFunctionType... SymFunTypes>
  void BehlerTripletFeature<MySymFunType, SymFunTypes...>::init(
      const UnitStyle & /*units*/) {}

}  // namespace rascal

#endif  // SRC_RASCAL_REPRESENTATIONS_BEHLER_FEATURE_IMPL_HH_
