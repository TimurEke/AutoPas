/**
*
*@file C08BasedKokkosTraversal.h
*@author M. Geitner
*@date 16.07.2019
*
*
*/

#pragma once

#include "autopas/containers/cellPairTraversals/CBasedKokkosTraversal.h"
#include "autopas/utils/ArrayMath.h"

namespace autopas {

/**
 * This class provides the base for traversals using the c08 base step.
 *
 * The traversal is defined in the function c08Traversal and uses 8 colors, such that interactions between the base
 * cell and all adjacent cells with greater ID in each direction are safe, even when using newton3 optimizations.
 *
 * @tparam ParticleCell the type of cells
 * @tparam PairwiseFunctor The functor that defines the interaction of two particles.
 * @tparam dataLayout
 * @tparam useNewton3
 */
    template <class ParticleCell, class PairwiseFunctor, DataLayoutOption dataLayout, bool useNewton3>
    class C08BasedKokkosTraversal : public CBasedKokkosTraversal<ParticleCell, PairwiseFunctor, dataLayout, useNewton3> {
    public:
        /**
         * Constructor of the c08 traversal.
         * @param dims The dimensions of the cellblock, i.e. the number of cells in x,
         * y and z direction.
         * @param pairwiseFunctor The functor that defines the interaction of two particles.
         * @param cutoff Cutoff radius.
         * @param cellLength cell length.
         */
        explicit C08BasedKokkosTraversal(const std::array<unsigned long, 3> &dims, PairwiseFunctor *pairwiseFunctor,
                                   const double cutoff = 1.0, const std::array<double, 3> &cellLength = {1.0, 1.0, 1.0})
                : CBasedKokkosTraversal<ParticleCell, PairwiseFunctor, dataLayout, useNewton3>(dims, pairwiseFunctor, cutoff,
                                                                                         cellLength) {}

    protected:
        /**
         * The main traversal of the C08Traversal.
         * @copydetails C01BasedTraversal::c01Traversal()
         */
        template <typename LoopBody>
        inline void c08Traversal(LoopBody &&loopBody);
    };

    template <class ParticleCell, class PairwiseFunctor, DataLayoutOption DataLayout, bool useNewton3>
    template <typename LoopBody>
    inline void C08BasedKokkosTraversal<ParticleCell, PairwiseFunctor, DataLayout, useNewton3>::c08Traversal(
            LoopBody &&loopBody) {
      const auto end = ArrayMath::sub(this->_cellsPerDimension, this->_overlap);
      const auto stride = ArrayMath::addScalar(this->_overlap, 1ul);
      this->cTraversal(std::forward<LoopBody>(loopBody), end, stride);
    }
}  // namespace autopas