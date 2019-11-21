/**
 * @file ThermostatTest.cpp
 * @author N. Fottner
 * @date 28/08/19.
 */

#include "ThermostatTest.h"

void ThermostatTest::initContainer(AutoPasType &autopas, const Molecule &dummy, std::array<size_t, 3> particlesPerDim) {
  constexpr double particleSpacing = 1.;
  constexpr double cutoff = 1.;

  double minimalBoxLength = cutoff + autopas.getVerletSkin();
  std::array<double, 3> boxmax = {std::max(particlesPerDim[0] * particleSpacing, minimalBoxLength),
                                  std::max(particlesPerDim[1] * particleSpacing, minimalBoxLength),
                                  std::max(particlesPerDim[2] * particleSpacing, minimalBoxLength)};
  autopas.setBoxMax(boxmax);
  autopas.setBoxMin({0., 0., 0.});
  autopas.setCutoff(cutoff);
  autopas.init();
  // place particles grid in the middle of the domain
  GridGenerator::fillWithParticles(autopas, particlesPerDim, dummy, {particleSpacing, particleSpacing, particleSpacing},
                                   {particleSpacing / 2, particleSpacing / 2, particleSpacing / 2});
}

void ThermostatTest::testBrownianMotion(const Molecule &dummyMolecule, bool useCurrentTemp) {
  initContainer(_autopas, dummyMolecule, {2, 1, 1});

  Thermostat::addBrownianMotion(_autopas, _particlePropertiesLibrary, useCurrentTemp);
  // check that velocities have actually changed
  for (auto iter = _autopas.begin(); iter.isValid(); ++iter) {
    for (int i = 0; i < 3; ++i) {
      EXPECT_THAT(iter->getV()[i], ::testing::Not(::testing::DoubleEq(dummyMolecule.getV()[i])));
    }
  }
}

TEST_F(ThermostatTest, BrownianMotionTest_useCurrentTempFalse) { testBrownianMotion(Molecule(), false); }

TEST_F(ThermostatTest, BrownianMotionTest_useCurrentTempTrue) {
  Molecule m;
  m.setV({1., 1., 1.});
  testBrownianMotion(m, true);
}

/**
 * Initialize a system with the given temperature and scale it in steps of delta to the target temperature.
 * @param initialTemperature
 * @param targetTemperature
 * @param deltaTemperature
 */
TEST_P(ThermostatTest, testApplyAndCalcTemperature) {
  const double initialTemperature = std::get<0>(GetParam());
  const double targetTemperature = std::get<1>(GetParam());
  const double deltaTemperature = std::get<2>(GetParam());
  Molecule m;
  initContainer(_autopas, m, {2, 2, 2});
  _particlePropertiesLibrary.addType(0, 1., 1., 1.);
  //  ThermostatType thermo(init 1.,target  2., delta .5, _particlePropertiesLibrary);
  EXPECT_NEAR(Thermostat::calcTemperature(_autopas, _particlePropertiesLibrary), 0, 1e-12);
  // add random velocities so that we do not scale zero vectors
  Thermostat::addBrownianMotion(_autopas, _particlePropertiesLibrary, false);
  // expect temperature to have changed from zero
  EXPECT_THAT(Thermostat::calcTemperature(_autopas, _particlePropertiesLibrary),
              ::testing::Not(::testing::DoubleNear(0, 1e-12)));
  // set system to initial temperature
  Thermostat::apply(_autopas, _particlePropertiesLibrary, initialTemperature, std::numeric_limits<double>::max());
  EXPECT_NEAR(Thermostat::calcTemperature(_autopas, _particlePropertiesLibrary), initialTemperature, 1e-12);

  //  for (int i = 1; std::abs(initialTemperature + i * deltaTemperature) < std::abs(targetTemperature); ++i) {
  auto expectedIterations = std::ceil(std::abs((targetTemperature - initialTemperature) / deltaTemperature));
  for (int i = 1; i <= expectedIterations; ++i) {
    Thermostat::apply(_autopas, _particlePropertiesLibrary, targetTemperature, deltaTemperature);
    if (i != expectedIterations) {
      EXPECT_NEAR(Thermostat::calcTemperature(_autopas, _particlePropertiesLibrary),
                  initialTemperature + i * deltaTemperature, 1e-12);
    } else {
      EXPECT_NEAR(Thermostat::calcTemperature(_autopas, _particlePropertiesLibrary), targetTemperature, 1e-12);
    }
  }

  // apply once more to check that temperature stays the same
  Thermostat::apply(_autopas, _particlePropertiesLibrary, targetTemperature, deltaTemperature);
  EXPECT_NEAR(Thermostat::calcTemperature(_autopas, _particlePropertiesLibrary), targetTemperature, 1e-12);
}

/**
 * Aspects to consider:
 *  - increase temperature
 *  - decrease temperature
 *  - delta temperature hits target temperature exactly
 *  - delta temperature does not hit target temperature exactly
 */
INSTANTIATE_TEST_SUITE_P(Generated, ThermostatTest,
                         // tuple (initialTemperature, targetTemperature, deltaTemperature)
                         ::testing::Values(std::tuple(1., 2., .3), std::tuple(1., 2., .5), std::tuple(2., 1., -.3),
                                           std::tuple(2., 1., -.5)));
