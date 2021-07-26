/**
 * @file ForEachInRegionTest.cpp
 * @author F. Gratl
 * @date 08.03.21
 */
#include "ForEachInRegionTest.h"

#include "ForEachTestHelper.h"
#include "autopas/AutoPas.h"
#include "testingHelpers/EmptyFunctor.h"

template <typename AutoPasT>
auto ForEachInRegionTest::defaultInit(AutoPasT &autoPas, autopas::ContainerOption &containerOption,
                                      double cellSizeFactor) {
  autoPas.setBoxMin({0., 0., 0.});
  autoPas.setBoxMax({10., 10., 10.});
  autoPas.setCutoff(1);
  autoPas.setVerletSkin(0.2);
  autoPas.setVerletRebuildFrequency(2);
  autoPas.setNumSamples(2);
  autoPas.setAllowedContainers(std::set<autopas::ContainerOption>{containerOption});
  autoPas.setAllowedTraversals(autopas::compatibleTraversals::allCompatibleTraversals(containerOption));
  autoPas.setAllowedCellSizeFactors(autopas::NumberSetFinite<double>(std::set<double>({cellSizeFactor})));

#ifdef AUTOPAS_CUDA
  autoPas.setVerletClusterSize(32);
#endif

  autoPas.init();

  auto haloBoxMin =
      autopas::utils::ArrayMath::subScalar(autoPas.getBoxMin(), autoPas.getVerletSkin() + autoPas.getCutoff());
  auto haloBoxMax =
      autopas::utils::ArrayMath::addScalar(autoPas.getBoxMax(), autoPas.getVerletSkin() + autoPas.getCutoff());

  return std::make_tuple(haloBoxMin, haloBoxMax);
}

TEST_P(ForEachInRegionTest, testRegionAroundCorner) {
  auto [containerOption, cellSizeFactor, useConstIterator, priorForceCalc, behavior] = GetParam();

  // init autopas and fill it with some particles
  autopas::AutoPas<Molecule> autoPas;
  defaultInit(autoPas, containerOption, cellSizeFactor);

  using ::autopas::utils::ArrayMath::add;
  using ::autopas::utils::ArrayMath::mulScalar;
  using ::autopas::utils::ArrayMath::sub;
  auto domainLength = sub(autoPas.getBoxMax(), autoPas.getBoxMin());
  // draw a box around the lower corner of the domain
  auto searchBoxLengthHalf = mulScalar(domainLength, 0.3);
  std::array<double, 3> searchBoxMin = sub(autoPas.getBoxMin(), searchBoxLengthHalf);
  std::array<double, 3> searchBoxMax = add(autoPas.getBoxMin(), searchBoxLengthHalf);

  auto [particleIDsOwned, particleIDsHalo, particleIDsInBoxOwned, particleIDsInBoxHalo] =
      ForEachTestHelper::fillContainerAroundBoundary(autoPas, searchBoxMin, searchBoxMax);

  if (priorForceCalc) {
    // the prior force calculation is partially wanted as this sometimes changes the state of the internal containers.
    EmptyFunctor<Molecule> eFunctor;
    autoPas.iteratePairwise(&eFunctor);
  }

  std::vector<size_t> expectedIDs;
  switch (behavior) {
    case autopas::IteratorBehavior::owned: {
      expectedIDs = particleIDsInBoxOwned;
      break;
    }
    case autopas::IteratorBehavior::halo: {
      expectedIDs = particleIDsInBoxHalo;
      break;
    }
    case autopas::IteratorBehavior::ownedOrHalo: {
      expectedIDs = particleIDsInBoxOwned;
      expectedIDs.insert(expectedIDs.end(), particleIDsInBoxHalo.begin(), particleIDsInBoxHalo.end());
      break;
    }
    default: {
      GTEST_FAIL() << "IteratorBehavior::" << behavior
                   << "  should not be tested through this test!\n"
                      "Container behavior with dummy particles is not uniform.\n"
                      "Using forceSequential is not supported.";
      break;
    }
  }

  // sanity check: there should be particles in the expected region
  ASSERT_THAT(expectedIDs, ::testing::Not(::testing::IsEmpty()));

  // actual test
  auto bh = behavior;  // necessary for compiler, behavior not detected as variable
  auto forEachInRegionLambda = [&, bh](auto lambda) {
    autoPas.forEachInRegion(lambda, searchBoxMin, searchBoxMax, bh);
  };
  ForEachTestHelper::findParticles(autoPas, forEachInRegionLambda, expectedIDs);

  //  IteratorTestHelper::provideRegionIterator(
  //      useConstIterator, autoPas, behavior, searchBoxMin, searchBoxMax,
  //      [&](const auto &autopas, auto &iter) { ForEachTestHelper::findParticles(autoPas, iter, expectedIDs); });
}

using ::testing::Combine;
using ::testing::UnorderedElementsAreArray;
using ::testing::Values;
using ::testing::ValuesIn;

static inline auto getTestableContainerOptions() {
#ifdef AUTOPAS_CUDA
  return autopas::ContainerOption::getAllOptions();
#else
  auto containerOptions = autopas::ContainerOption::getAllOptions();
  return containerOptions;
#endif
}

INSTANTIATE_TEST_SUITE_P(Generated, ForEachInRegionTest,
                         Combine(ValuesIn(getTestableContainerOptions()), /*cell size factor*/ Values(0.5, 1., 1.5),
                                 /*use const*/ Values(true, false), /*prior force calc*/ Values(true, false),
                                 ValuesIn(autopas::IteratorBehavior::getMostOptions())),
                         ForEachInRegionTest::PrintToStringParamName());