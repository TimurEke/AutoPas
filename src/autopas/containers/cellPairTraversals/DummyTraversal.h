/**
 * @file DummyTraversal.h
 * @author F. Gratl
 * @date 11/9/18
 */

#pragma once

#include "CellPairTraversal.h"

namespace autopas {

/**
 * Dummy traversal doing nothing.
 *
 * This traversal can be used as a workaround for containers which do not yet use traversals.
 *
 * @tparam ParticleCell
 */
template <class ParticleCell, DataLayoutOption dataLayout, bool useNewton3>
class DummyTraversal : public CellPairTraversal<ParticleCell> {
 public:
  /**
   * Constructor
   * @param dims
   */
  explicit DummyTraversal(const std::array<unsigned long, 3> &dims) : CellPairTraversal<ParticleCell>(dims){};

  ~DummyTraversal() = default;

  TraversalOption getTraversalType() const override { return TraversalOption::dummyTraversal; }

  bool isApplicable() const override { return true; }

  void traverseParticlePairs() override {
    // do nothing
  }

  void initTraversal() override {
    // do nothing
  }

  void endTraversal() override {
    // do nothing
  }

  DataLayoutOption getDataLayout() const override { return dataLayout; }

  bool getUseNewton3() const override { return useNewton3; }
};

}  // namespace autopas
