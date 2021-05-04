/**
 * @file FlopCounterFunctor.h
 *
 * @date 22 Jan 2018
 * @author tchipevn
 */

#pragma once

#include "autopas/pairwiseFunctors/Functor.h"
#include "autopas/utils/ArrayMath.h"

namespace autopas {
/**
 * This class helps in getting the number of performed floating point
 * operations. It is a functor that only calculated the amount of floating point
 * operations.
 * @todo this class currently is limited to the following case:
 *  - constant cutoff radius
 *  - constant amount of floating point operations for one kernel call (distance < cutoff)
 * @tparam Particle
 * @tparam ParticleCell
 */
template <class Particle>
class FlopCounterFunctor : public Functor<Particle, FlopCounterFunctor<Particle>> {
  using SoAFloatPrecision = typename Particle::ParticleSoAFloatPrecision;
 public:
  bool isRelevantForTuning() override { return false; }

  bool allowsNewton3() override { return true; }

  bool allowsNonNewton3() override { return true; }

  [[nodiscard]] bool isAppropriateClusterSize(unsigned int /*clusterSize*/,
                                              DataLayoutOption::Value dataLayout) const override {
    return dataLayout == DataLayoutOption::aos;  // no support for clusters yet, unless aos.
  }

  /**
   * constructor of FlopCounterFunctor
   * @param cutoffRadius the cutoff radius
   */
  explicit FlopCounterFunctor<Particle>(double cutoffRadius)
      : autopas::Functor<Particle, FlopCounterFunctor<Particle>>(cutoffRadius),
        _cutoffSquare(cutoffRadius * cutoffRadius),
        _distanceCalculations(0ul),
        _kernelCalls(0ul) {}

  void AoSFunctor(Particle &i, Particle &j, bool /*newton3*/) override {
    if (i.isDummy() or j.isDummy()) {
      return;
    }
    auto dr = utils::ArrayMath::sub(i.getR(), j.getR());
    auto dr2 = utils::ArrayMath::dot(dr, dr);
    _distanceCalculations.fetch_add(1, std::memory_order_relaxed);

    if (dr2 <= _cutoffSquare) {
      _kernelCalls.fetch_add(1, std::memory_order_relaxed);
    }
  }

  /**
   * See Functor::SoAFunctorSingle()
   * @param soa
   */
  void SoAFunctorSingle(SoAView<typename Particle::SoAArraysType> soa, bool /*newton3*/) override {
    if (soa.getNumParticles() == 0) return;

    auto *const __restrict x1ptr = soa.template begin<Particle::AttributeNames::posX>();
    auto *const __restrict y1ptr = soa.template begin<Particle::AttributeNames::posY>();
    auto *const __restrict z1ptr = soa.template begin<Particle::AttributeNames::posZ>();

    for (size_t i = 0; i < soa.getNumParticles(); ++i) {
      size_t distanceCalculationsAcc = 0;
      size_t kernelCallsAcc = 0;

// icpc vectorizes this.
// g++ only with -ffast-math or -funsafe-math-optimizations
#pragma omp simd reduction(+ : kernelCallsAcc, distanceCalculationsAcc)
      for (size_t j = i + 1; j < soa.getNumParticles(); ++j) {
        ++distanceCalculationsAcc;

        const auto drx = x1ptr[i] - x1ptr[j];
        const auto dry = y1ptr[i] - y1ptr[j];
        const auto drz = z1ptr[i] - z1ptr[j];

        const auto drx2 = drx * drx;
        const auto dry2 = dry * dry;
        const auto drz2 = drz * drz;

        const auto dr2 = drx2 + dry2 + drz2;

        if (dr2 <= _cutoffSquare) {
          ++kernelCallsAcc;
        }
      }
      _distanceCalculations.fetch_add(distanceCalculationsAcc, std::memory_order_relaxed);
      _kernelCalls.fetch_add(kernelCallsAcc, std::memory_order_relaxed);
    }
  }

  /**
   * See Functor::SoAFunctorPair()
   * @param soa1
   * @param soa2
   */
  void SoAFunctorPair(SoAView<typename Particle::SoAArraysType> soa1, SoAView<typename Particle::SoAArraysType> soa2,
                      bool /*newton3*/) override {
    auto *const __restrict x1ptr = soa1.template begin<Particle::AttributeNames::posX>();
    auto *const __restrict y1ptr = soa1.template begin<Particle::AttributeNames::posY>();
    auto *const __restrict z1ptr = soa1.template begin<Particle::AttributeNames::posZ>();
    auto *const __restrict x2ptr = soa2.template begin<Particle::AttributeNames::posX>();
    auto *const __restrict y2ptr = soa2.template begin<Particle::AttributeNames::posY>();
    auto *const __restrict z2ptr = soa2.template begin<Particle::AttributeNames::posZ>();

    for (size_t i = 0; i < soa1.getNumParticles(); ++i) {
      size_t distanceCalculationsAcc = 0;
      size_t kernelCallsAcc = 0;

// icpc vectorizes this.
// g++ only with -ffast-math or -funsafe-math-optimizations
#pragma omp simd reduction(+ : kernelCallsAcc, distanceCalculationsAcc)
      for (size_t j = 0; j < soa2.getNumParticles(); ++j) {
        ++distanceCalculationsAcc;

        const auto drx = x1ptr[i] - x2ptr[j];
        const auto dry = y1ptr[i] - y2ptr[j];
        const auto drz = z1ptr[i] - z2ptr[j];

        const auto drx2 = drx * drx;
        const auto dry2 = dry * dry;
        const auto drz2 = drz * drz;

        const auto dr2 = drx2 + dry2 + drz2;

        if (dr2 <= _cutoffSquare) {
          ++kernelCallsAcc;
        }
      }
      _distanceCalculations.fetch_add(distanceCalculationsAcc, std::memory_order_relaxed);
      _kernelCalls.fetch_add(kernelCallsAcc, std::memory_order_relaxed);
    }
  }

  /**
   * See Functor::SoAFunctorVerlet()
   * @param soa
   * @param indexFirst
   * @param neighborList
   */
  void SoAFunctorVerlet(SoAView<typename Particle::SoAArraysType> soa, const size_t indexFirst,
                        const std::vector<size_t, autopas::AlignedAllocator<size_t>> &neighborList,
                        bool /*newton3*/) override {
    auto numParts = soa.getNumParticles();

    if (numParts == 0) return;

    auto *const __restrict xptr = soa.template begin<Particle::AttributeNames::posX>();
    auto *const __restrict yptr = soa.template begin<Particle::AttributeNames::posY>();
    auto *const __restrict zptr = soa.template begin<Particle::AttributeNames::posZ>();

    const size_t listSizeI = neighborList.size();
    const size_t *const __restrict currentList = neighborList.data();

    // this is a magic number, that should correspond to at least
    // vectorization width*N have testet multiple sizes:
    // 4: small speedup compared to AoS
    // 8: small speedup compared to AoS
    // 12: small but best speedup compared to Aos
    // 16: smaller speedup
    // in theory this is a variable, we could auto-tune over...
#ifdef __AVX512F__
    // use a multiple of 8 for avx
    const size_t vecsize = 16;
#else
    // for everything else 12 is faster
    const size_t vecsize = 12;
#endif
    size_t joff = 0;

    // if the size of the verlet list is larger than the given size vecsize,
    // we will use a vectorized version.
    if (listSizeI >= vecsize) {
      alignas(64) std::array<SoAFloatPrecision, vecsize> xtmp{}, ytmp{}, ztmp{}, xArr{}, yArr{}, zArr{};
      // broadcast of the position of particle i
      for (size_t tmpj = 0; tmpj < vecsize; tmpj++) {
        xtmp[tmpj] = xptr[indexFirst];
        ytmp[tmpj] = yptr[indexFirst];
        ztmp[tmpj] = zptr[indexFirst];
      }
      // loop over the verlet list from 0 to x*vecsize
      for (; joff < listSizeI - vecsize + 1; joff += vecsize) {
        size_t distanceCalculationsAcc = 0;
        size_t kernelCallsAcc = 0;
        // in each iteration we calculate the interactions of particle i with
        // vecsize particles in the neighborlist of particle i starting at
        // particle joff

        // gather position of particle j
#pragma omp simd safelen(vecsize)
        for (size_t tmpj = 0; tmpj < vecsize; tmpj++) {
          xArr[tmpj] = xptr[currentList[joff + tmpj]];
          yArr[tmpj] = yptr[currentList[joff + tmpj]];
          zArr[tmpj] = zptr[currentList[joff + tmpj]];
        }

        // do omp simd with reduction of the interaction
#pragma omp simd reduction(+ : kernelCallsAcc, distanceCalculationsAcc) safelen(vecsize)
        for (size_t j = 0; j < vecsize; j++) {
          ++distanceCalculationsAcc;
          const auto drx = xtmp[j] - xArr[j];
          const auto dry = ytmp[j] - yArr[j];
          const auto drz = ztmp[j] - zArr[j];

          const auto drx2 = drx * drx;
          const auto dry2 = dry * dry;
          const auto drz2 = drz * drz;

          const auto dr2 = drx2 + dry2 + drz2;

          const unsigned long mask = (dr2 <= _cutoffSquare) ? 1 : 0;

          kernelCallsAcc += mask;
        }
        _distanceCalculations.fetch_add(distanceCalculationsAcc, std::memory_order_relaxed);
        _kernelCalls.fetch_add(kernelCallsAcc, std::memory_order_relaxed);
      }
    }
    size_t distanceCalculationsAcc = 0;
    size_t kernelCallsAcc = 0;
    // this loop goes over the remainder and uses no optimizations
    for (size_t jNeighIndex = joff; jNeighIndex < listSizeI; ++jNeighIndex) {
      size_t j = neighborList[jNeighIndex];
      if (indexFirst == j) continue;

      ++distanceCalculationsAcc;
      const auto drx = xptr[indexFirst] - xptr[j];
      const auto dry = yptr[indexFirst] - yptr[j];
      const auto drz = zptr[indexFirst] - zptr[j];

      const auto drx2 = drx * drx;
      const auto dry2 = dry * dry;
      const auto drz2 = drz * drz;

      const auto dr2 = drx2 + dry2 + drz2;

      if (dr2 <= _cutoffSquare) {
        ++kernelCallsAcc;
      }
    }
    _distanceCalculations.fetch_add(distanceCalculationsAcc, std::memory_order_relaxed);
    _kernelCalls.fetch_add(kernelCallsAcc, std::memory_order_relaxed);
  }

  void CudaFunctor(CudaSoA<typename Particle::CudaDeviceArraysType> &device_handle, bool /*newton3*/) override {
#if defined(AUTOPAS_CUDA)
    // estimate flops on GPU
    size_t size = device_handle.template get<Particle::AttributeNames::posX>().size();
    _distanceCalculations += size * size;
    _kernelCalls += size * size;
#else
    utils::ExceptionHandler::exception("AutoPas was compiled without CUDA support!");
#endif
  }

  void CudaFunctor(CudaSoA<typename Particle::CudaDeviceArraysType> &device_handle1,
                   CudaSoA<typename Particle::CudaDeviceArraysType> &device_handle2, bool /*newton3*/) override {
#if defined(AUTOPAS_CUDA)
    // estimate flops on GPU
    size_t size1 = device_handle1.template get<Particle::AttributeNames::posX>().size();
    size_t size2 = device_handle2.template get<Particle::AttributeNames::posX>().size();

    _distanceCalculations += size1 * size2;
    _kernelCalls += size1 * size2;
#else
    utils::ExceptionHandler::exception("AutoPas was compiled without CUDA support!");
#endif
  }

  /**
   * @copydoc Functor::deviceSoALoader
   */
  void deviceSoALoader(::autopas::SoA<typename Particle::SoAArraysType> &soa,
                       CudaSoA<typename Particle::CudaDeviceArraysType> &device_handle) override {
#if defined(AUTOPAS_CUDA)
    size_t size = soa.getNumParticles();
    if (size == 0) return;

    device_handle.template get<Particle::AttributeNames::posX>().copyHostToDevice(
        size, soa.template begin<Particle::AttributeNames::posX>());
    device_handle.template get<Particle::AttributeNames::posY>().copyHostToDevice(
        size, soa.template begin<Particle::AttributeNames::posY>());
    device_handle.template get<Particle::AttributeNames::posZ>().copyHostToDevice(
        size, soa.template begin<Particle::AttributeNames::posZ>());

    device_handle.template get<Particle::AttributeNames::forceX>().copyHostToDevice(
        size, soa.template begin<Particle::AttributeNames::forceX>());
    device_handle.template get<Particle::AttributeNames::forceY>().copyHostToDevice(
        size, soa.template begin<Particle::AttributeNames::forceY>());
    device_handle.template get<Particle::AttributeNames::forceZ>().copyHostToDevice(
        size, soa.template begin<Particle::AttributeNames::forceZ>());
#else
    utils::ExceptionHandler::exception("AutoPas was compiled without CUDA support!");
#endif
  }

  /**
   * @copydoc Functor::deviceSoAExtractor
   */
  void deviceSoAExtractor(::autopas::SoA<typename Particle::SoAArraysType> &soa,
                          CudaSoA<typename Particle::CudaDeviceArraysType> &device_handle) override {
#if defined(AUTOPAS_CUDA)
    size_t size = soa.getNumParticles();
    if (size == 0) return;

    device_handle.template get<Particle::AttributeNames::forceX>().copyDeviceToHost(
        size, soa.template begin<Particle::AttributeNames::forceX>());
    device_handle.template get<Particle::AttributeNames::forceY>().copyDeviceToHost(
        size, soa.template begin<Particle::AttributeNames::forceY>());
    device_handle.template get<Particle::AttributeNames::forceZ>().copyDeviceToHost(
        size, soa.template begin<Particle::AttributeNames::forceZ>());
#else
    utils::ExceptionHandler::exception("AutoPas was compiled without CUDA support!");
#endif
  }

  /**
   * @copydoc Functor::getNeededAttr()
   */
  constexpr static std::array<typename Particle::AttributeNames, 3> getNeededAttr() {
    return std::array<typename Particle::AttributeNames, 3>{
        Particle::AttributeNames::posX, Particle::AttributeNames::posY, Particle::AttributeNames::posZ};
  }

  /**
   * @copydoc Functor::getNeededAttr(std::false_type)
   */
  constexpr static std::array<typename Particle::AttributeNames, 3> getNeededAttr(std::false_type) {
    return getNeededAttr();
  }

  /**
   * @copydoc Functor::getComputedAttr()
   */
  constexpr static std::array<typename Particle::AttributeNames, 0> getComputedAttr() {
    return std::array<typename Particle::AttributeNames, 0>{/*Nothing*/};
  }

  /**
   * get the hit rate of the pair-wise interaction, i.e. the ratio of the number
   * of kernel calls compared to the number of distance calculations
   * @return the hit rate
   */
  double getHitRate() { return static_cast<double>(_kernelCalls) / static_cast<double>(_distanceCalculations); }

  /**
   * get the total number of flops
   * @param numFlopsPerKernelCall
   * @return
   */
  [[nodiscard]] size_t getFlops(size_t numFlopsPerKernelCall) const {
    const auto distFlops = numFlopsPerDistanceCalculation * _distanceCalculations;
    const auto kernFlops = numFlopsPerKernelCall * _kernelCalls;
    return distFlops + kernFlops;
  }

  /**
   * get the number of calculated distance operations
   * @return
   */
  [[nodiscard]] size_t getDistanceCalculations() const { return _distanceCalculations; }

  /**
   * get the number of kernel calls, i.e. the number of pairs of particles with
   * a distance not larger than the cutoff
   * @return
   */
  [[nodiscard]] size_t getKernelCalls() const { return _kernelCalls; }

  /**
   * number of flops for one distance calculation.
   * 3 sub + 3 square + 2 add
   */
  static constexpr double numFlopsPerDistanceCalculation = 8.0;

 private:
  double _cutoffSquare;
  std::atomic<size_t> _distanceCalculations, _kernelCalls;
};

}  // namespace autopas
