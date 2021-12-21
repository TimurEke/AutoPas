/**
 * @file KokkosParticleCell.h
 * @date 20.10.2021
 * @author lgaertner
 */

#pragma once

#include <Kokkos_Core.hpp>

#include "autopas/cells/ParticleCell.h"

namespace autopas {

/**
 * Dummy particle cell to use CellBlock3D with Kokkos Containers
 * @tparam Particle
 */
template <class Particle>
class KokkosParticleCell {
 public:
  KokkosParticleCell() : particlesPtr(nullptr), begin(0ul), cellSize(0ul){};

  Kokkos::RangePolicy<> getKokkosRangePolicy() { return {begin, begin + cellSize}; }

  size_t begin;
  size_t cellSize;
  Kokkos::View<Particle *> *particlesPtr;
};
}  // namespace autopas