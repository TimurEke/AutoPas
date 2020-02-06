/**
 * @file ClusterTower.h
 * @author humig
 * @date 27.07.19
 */

#pragma once

#include "autopas/cells/FullParticleCell.h"
#include "autopas/containers/verletClusterLists/Cluster.h"

namespace autopas::internal {

/**
 * This class represents one tower for clusters in the VerletClusterLists container.
 *
 * A ClusterTower contains multiple clusters that are stacked on top (z-direction) of each other. It saves all particles
 * in a FullParticleCell, provides methods to generate and work on the clusters contained, and handles the generation of
 * dummy particles to make sure that each cluster is full.
 *
 * Only the last cluster of the ClusterTower is filled up with dummy particles, since all others are guaranteed to
 * already be full.
 *
 * To use this container:
 *  1. Use addParticle() to add all particles you want.
 *  2. Then call generateClusters(). This copies the last particle as often as it is necessary to fill up the last
 * cluster. (maximum clusterSize-1 times).
 *  3. Generate your neighbor lists somehow.
 *  4. Call fillUpWithDummyParticles() to replace the copies of the last particle made in generateClusters() with
 * dummies.
 *
 * If you want to add more particles after calling generateClusters(), definitely make sure to call clear() before
 * calling addParticle() again, since doing otherwise will mess up the dummy particles and actual particles will likely
 * get lost.
 *
 * @tparam Particle
 * @tparam clusterSize
 */
template <class Particle, size_t clusterSize>
class ClusterTower : public ParticleCell<Particle> {
  /**
   * A prototype for the dummy particle to use.
   */
  static const Particle dummy;

 public:
  /**
   * Adds a particle to the cluster tower. If generateClusters() has already been called on this ClusterTower, clear()
   * must be called first, or dummies are messed up and particles get lost!
   *
   * Is allowed to be called in parallel since a lock is used on the internal cell.
   *
   * @param particle The particle to add.
   */
  void addParticle(const Particle &particle) override { _particles.addParticle(particle); }

  /**
   * Clears all particles from the tower and resets it to be ready for new particles.
   */
  void clear() override {
    _clusters.clear();
    _particles.clear();
    _numDummyParticles = 0;
  }

  /**
   * Generates the clusters for the particles in this cluster tower.
   *
   * Copies the last particle as often as necessary to fill up the last cluster. This makes sure that iteration over
   * clusters already works after this, while the bounding box of the last cluster is also not messed up by dummy
   * particles. This is necessary for rebuilding the neighbor lists.
   *
   * @return Returns the number of clusters in the tower.
   */
  size_t generateClusters() {
    if (getNumActualParticles() > 0) {
      _particles.sortByDim(2);

      auto sizeLastCluster = (_particles.numParticles() % clusterSize);
      _numDummyParticles = sizeLastCluster != 0 ? clusterSize - sizeLastCluster : 0;

      const auto lastParticle = _particles[_particles.numParticles() - 1];
      for (size_t i = 0; i < _numDummyParticles; i++) {
        _particles.addParticle(lastParticle);
      }

      size_t numClusters = _particles.numParticles() / clusterSize;
      _clusters.reserve(numClusters);
      for (size_t index = 0; index < numClusters; index++) {
        _clusters.emplace_back(&(_particles[clusterSize * index]));
      }
    }

    return getNumClusters();
  }

  /**
   * Replaces the copies of the last particle made in generateClusters() with dummies. Dummy particles have ID 0.
   *
   * @param dummyStartX The x-coordinate for all dummies.
   * @param dummyDistZ The distance in z-direction that all generated dummies will have from each other.
   */
  void fillUpWithDummyParticles(double dummyStartX, double dummyDistZ) {
    auto &lastCluster = getCluster(getNumClusters() - 1);
    for (size_t index = 1; index <= _numDummyParticles; index++) {
      lastCluster.at(clusterSize - index) = dummy;
      lastCluster.at(clusterSize - index).setR({dummyStartX, 0, dummyDistZ * index});
    }
  }

  /**
   * Loads the particles into the SoA stored in this tower and generates the SoAView for each cluster.
   * @tparam Functor The type of the functor to use.
   * @param functor The functor to use for loading the particles into the SoA.
   */
  template <class Functor>
  void loadSoA(Functor *functor) {
    functor->SoALoader(_particles, _particles._particleSoABuffer);
    for (size_t index = 0; index < getNumClusters(); index++) {
      auto &cluster = getCluster(index);
      cluster.getSoAView() = {&(_particles._particleSoABuffer), index * clusterSize, (index + 1) * clusterSize};
    }
  }

  /**
   * Extracts the SoA into the particles/clusters.
   * @tparam Functor The type of the functor to use.
   * @param functor The functor to use for extracting the SoA into the particles/clusters.
   */
  template <class Functor>
  void extractSoA(Functor *functor) {
    functor->SoAExtractor(_particles, _particles._particleSoABuffer);
  }

  /**
   * Returns a rvalue reference to a std::vector containing all particles of this tower that are not dummies.
   *
   * clear() has to called afterwards!
   * @return
   */
  std::vector<Particle> &&collectAllActualParticles() {
    _particles._particles.resize(getNumActualParticles());
    return std::move(_particles._particles);
  }

  /**
   * Returns the number of dummy particles in the tower (that all are in the last cluster).
   * @return the number of dummy particles in the tower.
   */
  [[nodiscard]] size_t getNumDummyParticles() const { return _numDummyParticles; }

  /**
   * Returns the number of particles in the tower that are not dummies.
   * @return the number of particles in the tower that are not dummies.
   */
  [[nodiscard]] size_t getNumActualParticles() const { return _particles.numParticles() - _numDummyParticles; }

  /**
   * Returns the number of clusters in the tower.
   * @return the number of clusters in the tower.
   */
  [[nodiscard]] size_t getNumClusters() const { return _clusters.size(); }

  /**
   * Returns a reference to the std::vector holding the clusters of this container.
   * @return a reference to the std::vector holding the clusters of this container.
   */
  [[nodiscard]] auto &getClusters() { return _clusters; }

  /**
   * Returns the cluster at position index.
   * @param index The index of the cluster to return.
   * @return the cluster at position index.
   */
  [[nodiscard]] auto &getCluster(size_t index) { return _clusters[index]; }

  /**
   * @copydoc getCluster(size_t)
   */
  [[nodiscard]] auto &getCluster(size_t index) const { return _clusters[index]; }

  /**
   * @copydoc getNumActualParticles()
   */
  [[nodiscard]] unsigned long numParticles() const override { return getNumActualParticles(); }

  /**
   * Returns an iterator over all non-dummy particles contained in this tower.
   * @return an iterator over all non-dummy particles contained in this tower.
   */
  [[nodiscard]] SingleCellIteratorWrapper<Particle, true> begin() override {
    return SingleCellIteratorWrapper<Particle, true>{
        new SingleCellIterator<Particle, ClusterTower<Particle, clusterSize>, true>(this)};
  }

  /**
   * Returns an iterator over all non-dummy particles contained in this tower.
   * @return an iterator over all non-dummy particles contained in this tower.
   */
  [[nodiscard]] SingleCellIteratorWrapper<Particle, false> begin() const override {
    return SingleCellIteratorWrapper<Particle, false>{
        new SingleCellIterator<Particle, ClusterTower<Particle, clusterSize>, false>(this)};
  }

  /**
   * Returns the particle at position index. Needed by SingleCellIterator.
   * @param index the position of the particle to return.
   * @return the particle at position index.
   */
  Particle &at(size_t index) { return _particles._particles.at(index); }

  /**
   * Returns the const particle at position index. Needed by SingleCellIterator.
   * @param index the position of the particle to return.
   * @return the particle at position index.
   */
  const Particle &at(size_t index) const { return _particles._particles.at(index); }

  // Methods from here on: Only to comply with ParticleCell interface. SingleCellIterators work on ParticleCells, and
  // while those methods would not be needed, still complying to the whole interface should be helpful, if
  // maybe someday new necessary pure virtual methods are introduced there.

  [[nodiscard]] bool isNotEmpty() const override { return getNumActualParticles() > 0; }

  void deleteByIndex(size_t index) override {
    // @TODO support deletion of particles somehow
    autopas::utils::ExceptionHandler::exception("Not supported!");
  }

  void setCellLength(std::array<double, 3> &) override {
    autopas::utils::ExceptionHandler::exception("Not supported!");
  }

  [[nodiscard]] std::array<double, 3> getCellLength() const override {
    autopas::utils::ExceptionHandler::exception("Not supported!");
    return {0, 0, 0};
  }

 private:
  /**
   * The clusters that are contained in this tower.
   */
  std::vector<Cluster<Particle, clusterSize>> _clusters;
  /**
   * The particle cell to store the particles and SoA for this tower.
   */
  FullParticleCell<Particle> _particles;
  /**
   * The number of dummy particles in this tower.
   */
  size_t _numDummyParticles{};
};

// Requires all particle classes to have a constructor that takes position, velocity, and id
template <class Particle, size_t clusterSize>
const Particle ClusterTower<Particle, clusterSize>::dummy{
    {std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max()},
    {0, 0, 0},
    0};

}  // namespace autopas::internal
