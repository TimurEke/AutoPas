/**
 * @file PredictiveTuning.h
 * @author Julian Pelloth
 * @date 01.04.2020
 */

#pragma once

#include <set>
#include <utility>

#include "SetSearchSpaceBasedTuningStrategy.h"
#include "autopas/selectors/OptimumSelector.h"
#include "autopas/utils/ExceptionHandler.h"

namespace autopas {

/**
 * On every tuning phase, this strategy makes a runtime prediction for every configuration and then
 * only tests those which are within a certain range of the best prediction. In the end, the configuration
 * that performed best during testing is selected.
 */
class PredictiveTuning : public SetSearchSpaceBasedTuningStrategy {
 public:
  /**
   * Constructor for the PredictiveTuning that generates the search space from the allowed options.
   * @param allowedContainerOptions
   * @param allowedTraversalOptions
   * @param allowedDataLayoutOptions
   * @param allowedNewton3Options
   * @param allowedCellSizeFactors
   * @param relativeOptimum
   * @param maxTuningIterationsWithoutTest
   */
  PredictiveTuning(const std::set<ContainerOption> &allowedContainerOptions,
                   const std::set<double> &allowedCellSizeFactors,
                   const std::set<TraversalOption> &allowedTraversalOptions,
                   const std::set<DataLayoutOption> &allowedDataLayoutOptions,
                   const std::set<Newton3Option> &allowedNewton3Options, double relativeOptimum,
                   unsigned int maxTuningIterationsWithoutTest)
      : SetSearchSpaceBasedTuningStrategy(allowedContainerOptions, allowedCellSizeFactors, allowedTraversalOptions,
                                          allowedDataLayoutOptions, allowedNewton3Options),
        _currentConfig(_searchSpace.begin()),
        _relativeOptimumRange(relativeOptimum),
        _maxTuningIterationsWithoutTest(maxTuningIterationsWithoutTest) {
    // sets traversalTimesStorage
    for (const auto &configuration : _searchSpace) {
      std::vector<std::pair<size_t, long>> vector;
      _traversalTimesStorage.emplace(configuration, vector);
    }
  }

  /**
   * Constructor for the PredictiveTuning that only contains the given configurations.
   * This constructor assumes only valid configurations are passed! Mainly for easier unit testing.
   * @param allowedConfigurations Set of configurations AutoPas can choose from.
   */
  explicit PredictiveTuning(std::set<Configuration> allowedConfigurations)
      : SetSearchSpaceBasedTuningStrategy(std::move(allowedConfigurations)),
        _currentConfig(_searchSpace.begin()),
        _relativeOptimumRange(1.2),
        _maxTuningIterationsWithoutTest(5) {}

  inline void addEvidence(long time, size_t iteration) override {
    _traversalTimesStorage[*_currentConfig].emplace_back(iteration, time);
    _lastTest[*_currentConfig] = _tuningIterationsCounter;
  }

  inline long getEvidence(Configuration configuration) const override {
    // compute the average of times for this configuration
    auto times = _traversalTimesStorage.at(configuration);
    long result = 0;
    for (auto time : times) {
      result += time.second;
    }
    return result / times.size();
  }

  inline const Configuration &getCurrentConfiguration() const override { return *_currentConfig; }

  inline void reset(size_t iteration) override {
    _configurationPredictions.clear();
    _optimalSearchSpace.clear();
    _tooLongNotTestedSearchSpace.clear();
    _validSearchSpace = _searchSpace;
    _validConfigurationFound = false;
    _iterationBeginTuningPhase = iteration;

    selectOptimalSearchSpace();
  }

  inline bool tune(bool = false) override;

  inline void removeN3Option(Newton3Option badNewton3Option) override;

 private:
  inline void selectOptimalConfiguration();
  /**
   * Selects the configurations that are going to be tested.
   */
  inline void selectOptimalSearchSpace();
  /**
   * Provides different extrapolation methods for the prediction of the traversal time.
   */
  inline void calculatePredictions();
  /**
   * Predicts the traversal time by placing a line trough the last two traversal points and calculating the prediction
   * for the current time.
   */
  inline void linePrediction();
  /**
   * Creates a new optimalSearchSpace if every configuration in the previous one was invalid.
   */
  inline void reselectOptimalSearchSpace();

  std::set<Configuration>::iterator _currentConfig;
  /**
   * Stores the traversal times for each configuration.
   * @param Configuration
   * @param Vector with pairs of the iteration and the traversal time.
   */
  std::unordered_map<Configuration, std::vector<std::pair<size_t, long>>, ConfigHash> _traversalTimesStorage;
  /**
   * Contains the predicted time for each configuration.
   * @param Configuration
   * @param traversal prediction
   */
  std::unordered_map<Configuration, size_t, ConfigHash> _configurationPredictions;
  /**
   * Contains the configuration that are predicted to be optimal and going to be tested.
   */
  std::set<Configuration> _optimalSearchSpace;
  /**
   * Contains the configuration that have not been tested for a period of time and are going to be tested.
   */
  std::set<Configuration> _tooLongNotTestedSearchSpace;
  /**
   * Contains the configurations that are not invalid.
   */
  std::set<Configuration> _validSearchSpace;
  /**
   * Stores the the last tuning phase a configuration got tested.
   * @param Configuration
   * @param last tuning phase
   */
  std::unordered_map<Configuration, size_t, ConfigHash> _lastTest;
  /**
   * Gets incremented after every completed tuning phase.
   * Mainly used for the traversal time storage.
   */
  unsigned int _tuningIterationsCounter = 0;
  /**
   * Stores the iteration at the beginning of a tuning phase.
   */
  unsigned int _iterationBeginTuningPhase = 0;
  /**
   * Indicates if a valid configuration was found in the _optimalSearchSpace.
   */
  bool _validConfigurationFound = false;
  /**
   * Factor of the range of the optimal configurations for the optimalSearchSpace.
   */
  const double _relativeOptimumRange;
  /**
   * After not being tested this number of tuningPhases a configuration is being emplaced in _optimalSearchSpace.
   */
  const unsigned int _maxTuningIterationsWithoutTest;
};

void PredictiveTuning::selectOptimalSearchSpace() {
  if (_searchSpace.size() == 1 or _tuningIterationsCounter < 2) {
    _currentConfig = _searchSpace.begin();
    return;
  }

  calculatePredictions();

  const auto optimum = getOptimum(_configurationPredictions);

  _optimalSearchSpace.emplace(optimum->first);

  // selects configurations that are near the optimal prediction or have not been tested in a certain number of
  // iterations
  for (const auto &configuration : _searchSpace) {
    // Adds configurations that have not been tested for _maxTuningIterationsWithoutTest or are within the
    // _relativeOptimumRange
    if ((float)_configurationPredictions[configuration] / optimum->second <= _relativeOptimumRange) {
      _optimalSearchSpace.emplace(configuration);
    } else if (_tuningIterationsCounter - _lastTest[configuration] > _maxTuningIterationsWithoutTest) {
      _tooLongNotTestedSearchSpace.emplace(configuration);
    }
  }

  // sanity check
  if (_optimalSearchSpace.empty()) {
    autopas::utils::ExceptionHandler::exception("PredicitveTuning: No possible configuration prediction found!");
  }

  _currentConfig = _optimalSearchSpace.begin();
}

void PredictiveTuning::calculatePredictions() {
  linePrediction();
  /*switch (method) {
  * case line: linePrediction(); break;
  * default:

  }*/
}

void PredictiveTuning::linePrediction() {
  for (auto &configuration : _searchSpace) {
    const auto &vector = _traversalTimesStorage[configuration];
    const auto &traversal1 = vector[vector.size() - 1];
    const auto &traversal2 = vector[vector.size() - 2];

    const auto gradient = (traversal1.second - traversal2.second) / (traversal1.first - traversal2.first);
    const auto delta = _iterationBeginTuningPhase - traversal1.first;

    // time1 + (time1 - time2) / (iteration1 - iteration2) / tuningPhase - iteration1)
    _configurationPredictions[configuration] = traversal1.second + gradient * delta;
  }
}

void PredictiveTuning::reselectOptimalSearchSpace() {
  // This is placed here, because there are no unnecessary removals
  for (const auto &configuration : _optimalSearchSpace) {
    _configurationPredictions.erase(configuration);
    _validSearchSpace.erase(configuration);
  }

  _optimalSearchSpace.clear();

  if (_validSearchSpace.size() == 1) {
    _optimalSearchSpace = _validSearchSpace;
    _currentConfig = _optimalSearchSpace.begin();
    return;
  }

  // checks if there are any configurations left that can be tested
  if (_configurationPredictions.empty()) {
    autopas::utils::ExceptionHandler::exception("Predictive Tuning: No valid configuration could be found");
  }

  const auto optimum = getOptimum(_configurationPredictions);

  if (_validSearchSpace.count(optimum->first) == 0) {
    autopas::utils::ExceptionHandler::exception("Predictive Tuning: No valid optimal configuration could be found");
  }

  _optimalSearchSpace.emplace(optimum->first);

  // selects configurations that are near the optimal prediction
  for (const auto &configuration : _validSearchSpace) {
    // Adds configurations that are within the _relativeOptimumRange
    if ((float)_configurationPredictions[configuration] / optimum->second <= _relativeOptimumRange) {
      _optimalSearchSpace.emplace(configuration);
      _tooLongNotTestedSearchSpace.erase(configuration);
    }
  }

  // sanity check
  if (_optimalSearchSpace.empty()) {
    autopas::utils::ExceptionHandler::exception("PredicitveTuning: No possible configuration prediction found!");
  }

  _currentConfig = _optimalSearchSpace.begin();
}

bool PredictiveTuning::tune(bool valid) {
  if (not valid) {
    _validConfigurationFound = true;
  }

  // repeat as long as traversals are not applicable or we run out of configs
  ++_currentConfig;

  if (_currentConfig == _searchSpace.end() or _currentConfig == _optimalSearchSpace.end()) {
    if (_validConfigurationFound) {
      if (_tooLongNotTestedSearchSpace.empty()) {
        selectOptimalConfiguration();
        _tuningIterationsCounter++;
        return false;
      } else {
        _currentConfig = _tooLongNotTestedSearchSpace.begin();
        return true;
      }
    } else {
      reselectOptimalSearchSpace();
    }
  } else if (_currentConfig == _tooLongNotTestedSearchSpace.end()) {
    selectOptimalConfiguration();
    _tuningIterationsCounter++;
    return false;
  }

  return true;
}

void PredictiveTuning::selectOptimalConfiguration() {
  if (_optimalSearchSpace.size() == 1) {
    _currentConfig = _optimalSearchSpace.begin();
    return;
  }
  if (_searchSpace.size() == 1) {
    _currentConfig = _searchSpace.begin();
    return;
  }

  // select the tested traversal times for the current tuning phase
  std::unordered_map<Configuration, size_t, ConfigHash> traversalTimes;
  // In the first couple iterations tune iterates through _searchSpace until predictions are made
  if (_optimalSearchSpace.empty()) {
    for (const auto &configuration : _searchSpace) {
      if (_traversalTimesStorage[configuration].back().first >= _iterationBeginTuningPhase) {
        traversalTimes[configuration] = _traversalTimesStorage[configuration].back().second;
      }
    }
  } else {
    for (const auto &configuration : _optimalSearchSpace) {
      if (_traversalTimesStorage[configuration].back().first >= _iterationBeginTuningPhase) {
        traversalTimes[configuration] = _traversalTimesStorage[configuration].back().second;
      }
    }
    for (const auto &configuration : _tooLongNotTestedSearchSpace) {
      if (_traversalTimesStorage[configuration].back().first >= _iterationBeginTuningPhase) {
        traversalTimes[configuration] = _traversalTimesStorage[configuration].back().second;
      }
    }
  }

  // Time measure strategy
  if (traversalTimes.empty()) {
    utils::ExceptionHandler::exception(
        "PredictiveTuning: Trying to determine fastest configuration without any measurements! "
        "Either selectOptimalConfiguration was called too early or no applicable configurations were found");
  }

  const auto optimum = getOptimum(traversalTimes);

  _currentConfig = _searchSpace.find(optimum->first);

  // sanity check
  if (_currentConfig == _searchSpace.end() or _currentConfig == _optimalSearchSpace.end()) {
    autopas::utils::ExceptionHandler::exception(
        "PredicitveTuning: Optimal configuration not found in list of configurations!");
  }

  AutoPasLog(debug, "Selected Configuration {}", _currentConfig->toString());
}

void PredictiveTuning::removeN3Option(Newton3Option badNewton3Option) {
  for (auto ssIter = _searchSpace.begin(); ssIter != _searchSpace.end();) {
    if (ssIter->newton3 == badNewton3Option) {
      // change current config to the next non-deleted
      if (ssIter == _currentConfig) {
        ssIter = _searchSpace.erase(ssIter);
        _currentConfig = ssIter;
      } else {
        ssIter = _searchSpace.erase(ssIter);
      }
    } else {
      ++ssIter;
    }
  }

  if (this->searchSpaceIsEmpty()) {
    utils::ExceptionHandler::exception(
        "Removing all configurations with Newton 3 {} caused the search space to be empty!", badNewton3Option);
  }
}

}  // namespace autopas
