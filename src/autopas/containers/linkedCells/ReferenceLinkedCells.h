/**
 * @file LinkedCells.h
 * @date 05.04.2020
 * @author lunaticcoding
 */

#pragma once

#include "autopas/cells/ReferenceParticleCell.h"
#include "autopas/containers/CellBlock3D.h"
#include "autopas/containers/CompatibleTraversals.h"
#include "autopas/containers/LoadEstimators.h"
#include "autopas/containers/ParticleContainer.h"
#include "autopas/containers/cellPairTraversals/BalancedTraversal.h"
#include "autopas/containers/linkedCells/ParticleVector.h"
#include "autopas/containers/linkedCells/traversals/LinkedCellTraversalInterface.h"
#include "autopas/iterators/ParticleIterator.h"
#include "autopas/iterators/RegionParticleIterator.h"
#include "autopas/options/DataLayoutOption.h"
#include "autopas/options/LoadEstimatorOption.h"
#include "autopas/particles/OwnershipState.h"
#include "autopas/utils/ArrayMath.h"
#include "autopas/utils/ParticleCellHelpers.h"
#include "autopas/utils/StringUtils.h"
#include "autopas/utils/WrapOpenMP.h"
#include "autopas/utils/inBox.h"

namespace autopas {

/**
 * LinkedCells class.
 * This class uses a list of neighboring cells to store the particles.
 * These cells dimensions are at least as large as the given cutoff radius,
 * therefore short-range interactions only need to be calculated between
 * particles in neighboring cells.
 * @tparam ParticleCell type of the ParticleCells that are used to store the particles
 * @tparam SoAArraysType type of the SoA, needed for verlet lists
 */
template <class Particle, class SoAArraysType = typename Particle::SoAArraysType>
class ReferenceLinkedCells : public ParticleContainer<ReferenceParticleCell<Particle>, SoAArraysType> {
 public:
  /**
   *  Type of the Particle.
   */
  using ParticleType = Particle;
  /**
   *  Type of the ParticleCell.
   */
  using ReferenceCell = ReferenceParticleCell<Particle>;
  /**
   * Constructor of the LinkedCells class
   * @param boxMin
   * @param boxMax
   * @param cutoff
   * @param skin
   * @param cellSizeFactor cell size factor relative to cutoff
   * By default all applicable traversals are allowed.
   */
  ReferenceLinkedCells(const std::array<double, 3> boxMin, const std::array<double, 3> boxMax, const double cutoff,
                       const double skin, const double cellSizeFactor = 1.0)
      : ParticleContainer<ReferenceCell, SoAArraysType>(boxMin, boxMax, cutoff, skin),
        _cellBlock(this->_cells, boxMin, boxMax, cutoff + skin, cellSizeFactor) {}

  /**
   * @copydoc ParticleContainerInterface::getContainerType()
   */
  [[nodiscard]] ContainerOption getContainerType() const override { return ContainerOption::referenceLinkedCells; }

  /**
   * @copydoc ParticleContainerInterface::addParticleImpl()
   */
  void addParticleImpl(const ParticleType &p) override {
    ParticleType pCopy = p;
    _particleList.push_back(pCopy);
    updateDirtyParticleReferences();
  }

  /**
   * @copydoc ParticleContainerInterface::addHaloParticleImpl()
   */
  void addHaloParticleImpl(const ParticleType &haloParticle) override {
    ParticleType pCopy = haloParticle;
    pCopy.setOwnershipState(OwnershipState::halo);
    _particleList.push_back(pCopy);
    updateDirtyParticleReferences();
  }

  /**
   * @copydoc ParticleContainerInterface::updateHaloParticle()
   */
  bool updateHaloParticle(const ParticleType &haloParticle) override {
    ParticleType pCopy = haloParticle;
    pCopy.setOwnershipState(OwnershipState::halo);
    auto cells = _cellBlock.getNearbyHaloCells(pCopy.getR(), this->getSkin());
    for (auto cellptr : cells) {
      bool updated = internal::checkParticleInCellAndUpdateByID(*cellptr, pCopy);
      if (updated) {
        return true;
      }
    }
    AutoPasLog(trace, "UpdateHaloParticle was not able to update particle: {}", pCopy.toString());
    return false;
  }

  /**
   * @copydoc ParticleContainerInterface::deleteHaloParticles()
   */
  void deleteHaloParticles() override { _cellBlock.clearHaloCells(); }

  /**
   * @copydoc ParticleContainerInterface::rebuildNeighborLists()
   */
  void rebuildNeighborLists(TraversalInterface *traversal) override { updateDirtyParticleReferences(); }

  /**
   * Updates all the References in the cells that are out of date.
   */
  void updateDirtyParticleReferences() {
    if (_particleList.isDirty()) {
      for (auto &cell : this->_cells) {
        cell.clear();
      }
    }
    for (auto it = _particleList.beginDirty(); it < _particleList.endDirty(); it++) {
      ReferenceCell &cell = _cellBlock.getContainingCell(it->getR());
      auto address = &(*it);
      cell.addParticleReference(address);
    }
    _particleList.markAsClean();
  }

  void iteratePairwise(TraversalInterface *traversal) override {
    // Check if traversal is allowed for this container and give it the data it needs.
    auto *traversalInterface = dynamic_cast<LinkedCellTraversalInterface<ReferenceCell> *>(traversal);
    auto *cellPairTraversal = dynamic_cast<CellPairTraversal<ReferenceCell> *>(traversal);
    if (traversalInterface && cellPairTraversal) {
      cellPairTraversal->setCellsToTraverse(this->_cells);
    } else {
      autopas::utils::ExceptionHandler::exception(
          "Trying to use a traversal of wrong type in ReferenceLinkedCells::iteratePairwise. TraversalID: {}",
          traversal->getTraversalType());
    }

    traversal->initTraversal();
    traversal->traverseParticlePairs();
    traversal->endTraversal();
  }

  std::vector<ParticleType> updateContainer() override {
    this->deleteHaloParticles();
    std::vector<ParticleType> invalidParticles;
#ifdef AUTOPAS_OPENMP
#pragma omp parallel
#endif  // AUTOPAS_OPENMP
    {
      // private for each thread!
      std::vector<ParticleType> myInvalidParticles, myInvalidNotOwnedParticles;
#ifdef AUTOPAS_OPENMP
#pragma omp for
#endif  // AUTOPAS_OPENMP
      for (size_t cellId = 0; cellId < this->getCells().size(); ++cellId) {
        // if empty
        if (not this->getCells()[cellId].isNotEmpty()) continue;

        auto [cellLowerCorner, cellUpperCorner] = this->getCellBlock().getCellBoundingBox(cellId);

        for (auto &&pIter = this->getCells()[cellId].begin(); pIter.isValid(); ++pIter) {
          // if not in cell
          if (utils::notInBox(pIter->getR(), cellLowerCorner, cellUpperCorner)) {
            myInvalidParticles.push_back(*pIter);
            internal::deleteParticle(pIter);
          }
        }
      }
      // implicit barrier here
      // the barrier is needed because iterators are not threadsafe w.r.t. addParticle()

      // this loop is executed for every thread and thus parallel. Don't use #pragma omp for here!
      for (auto &&p : myInvalidParticles) {
        // if not in halo
        if (utils::inBox(p.getR(), this->getBoxMin(), this->getBoxMax())) {
          this->template addParticle<false>(p);
        } else {
          myInvalidNotOwnedParticles.push_back(p);
        }
      }
#ifdef AUTOPAS_OPENMP
#pragma omp critical
#endif
      {
        // merge private vectors to global one.
        invalidParticles.insert(invalidParticles.end(), myInvalidNotOwnedParticles.begin(),
                                myInvalidNotOwnedParticles.end());
      }
    }
    return invalidParticles;
  }

  /**
   * @copydoc ParticleContainerInterface::getTraversalSelectorInfo()
   */
  [[nodiscard]] TraversalSelectorInfo getTraversalSelectorInfo() const override {
    return TraversalSelectorInfo(this->getCellBlock().getCellsPerDimensionWithHalo(), this->getInteractionLength(),
                                 this->getCellBlock().getCellLength(), 0);
  }

  ParticleIteratorWrapper<ParticleType, true> begin(
      IteratorBehavior behavior = IteratorBehavior::haloAndOwned) override {
    return ParticleIteratorWrapper<ParticleType, true>(
        new internal::ParticleIterator<ParticleType, ReferenceCell, true>(&this->_cells, 0, &_cellBlock, behavior));
  }

  ParticleIteratorWrapper<ParticleType, false> begin(
      IteratorBehavior behavior = IteratorBehavior::haloAndOwned) const override {
    return ParticleIteratorWrapper<ParticleType, false>(
        new internal::ParticleIterator<ParticleType, ReferenceCell, false>(&this->_cells, 0, &_cellBlock, behavior));
  }

  ParticleIteratorWrapper<ParticleType, true> getRegionIterator(
      const std::array<double, 3> &lowerCorner, const std::array<double, 3> &higherCorner,
      IteratorBehavior behavior = IteratorBehavior::haloAndOwned) override {
    // We increase the search region by skin, as particles can move over cell borders.
    auto startIndex3D =
        this->_cellBlock.get3DIndexOfPosition(utils::ArrayMath::subScalar(lowerCorner, this->getSkin()));
    auto stopIndex3D =
        this->_cellBlock.get3DIndexOfPosition(utils::ArrayMath::addScalar(higherCorner, this->getSkin()));

    size_t numCellsOfInterest = (stopIndex3D[0] - startIndex3D[0] + 1) * (stopIndex3D[1] - startIndex3D[1] + 1) *
                                (stopIndex3D[2] - startIndex3D[2] + 1);
    std::vector<size_t> cellsOfInterest(numCellsOfInterest);

    int i = 0;
    for (size_t z = startIndex3D[2]; z <= stopIndex3D[2]; ++z) {
      for (size_t y = startIndex3D[1]; y <= stopIndex3D[1]; ++y) {
        for (size_t x = startIndex3D[0]; x <= stopIndex3D[0]; ++x) {
          cellsOfInterest[i++] =
              utils::ThreeDimensionalMapping::threeToOneD({x, y, z}, this->_cellBlock.getCellsPerDimensionWithHalo());
        }
      }
    }

    return ParticleIteratorWrapper<ParticleType, true>(
        new internal::RegionParticleIterator<ParticleType, ReferenceCell, true>(
            &this->_cells, lowerCorner, higherCorner, cellsOfInterest, &_cellBlock, behavior));
  }

  ParticleIteratorWrapper<ParticleType, false> getRegionIterator(
      const std::array<double, 3> &lowerCorner, const std::array<double, 3> &higherCorner,
      IteratorBehavior behavior = IteratorBehavior::haloAndOwned) const override {
    // We increase the search region by skin, as particles can move over cell borders.
    auto startIndex3D =
        this->_cellBlock.get3DIndexOfPosition(utils::ArrayMath::subScalar(lowerCorner, this->getSkin()));
    auto stopIndex3D =
        this->_cellBlock.get3DIndexOfPosition(utils::ArrayMath::addScalar(higherCorner, this->getSkin()));

    size_t numCellsOfInterest = (stopIndex3D[0] - startIndex3D[0] + 1) * (stopIndex3D[1] - startIndex3D[1] + 1) *
                                (stopIndex3D[2] - startIndex3D[2] + 1);
    std::vector<size_t> cellsOfInterest(numCellsOfInterest);

    int i = 0;
    for (size_t z = startIndex3D[2]; z <= stopIndex3D[2]; ++z) {
      for (size_t y = startIndex3D[1]; y <= stopIndex3D[1]; ++y) {
        for (size_t x = startIndex3D[0]; x <= stopIndex3D[0]; ++x) {
          cellsOfInterest[i++] =
              utils::ThreeDimensionalMapping::threeToOneD({x, y, z}, this->_cellBlock.getCellsPerDimensionWithHalo());
        }
      }
    }

    return ParticleIteratorWrapper<ParticleType, false>(
        new internal::RegionParticleIterator<ParticleType, ReferenceCell, false>(
            &this->_cells, lowerCorner, higherCorner, cellsOfInterest, &_cellBlock, behavior));
  }

  /**
   * Get the cell block, not supposed to be used except by verlet lists
   * @return the cell block
   */
  internal::CellBlock3D<ReferenceCell> &getCellBlock() { return _cellBlock; }

  /**
   * @copydoc getCellBlock()
   * @note const version
   */
  const internal::CellBlock3D<ReferenceCell> &getCellBlock() const { return _cellBlock; }

  /**
   * Returns reference to the data of ReferenceLinkedCells
   * @return the data
   */
  std::vector<ReferenceCell> &getCells() { return this->_cells; }

  /**
   * @copydoc getCells()
   * @note const version
   */
  const std::vector<ReferenceCell> &getCells() const { return this->_cells; }

 protected:
  /**
   * object that stores the actual Particles and keeps track of the references.
   */
  ParticleVector<ParticleType> _particleList;
  /**
   * object to manage the block of cells.
   */
  internal::CellBlock3D<ReferenceCell> _cellBlock;
  // ThreeDimensionalCellHandler
};

}  // namespace autopas
