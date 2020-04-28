/**
 * @file FullSearchMPI.h
 * @author W. Thieme
 * @date 4/17/20
 */

#pragma once

#include <set>
#include <sstream>

#include "TuningStrategyInterface.h"
#include "autopas/containers/CompatibleTraversals.h"
#include "autopas/selectors/OptimumSelector.h"
#include "autopas/utils/ExceptionHandler.h"
#include "autopas/utils/WrapMPI.h"

namespace autopas {

/**
 * Exhaustive full search of the search space by testing every applicable configuration and then selecting the optimum.
 * The search space is divided among the ranks.
 * Use for homogeneous domains.
 */
class FullSearchMPI : public TuningStrategyInterface {
 public:
  /**
   * Constructor for the FullSearch that generates the rank's portion of the search space from the allowed options.
   * @param allowedContainerOptions
   * @param allowedTraversalOptions
   * @param allowedDataLayoutOptions
   * @param allowedNewton3Options
   * @param allowedCellSizeFactors
   */
  FullSearchMPI(const std::set<ContainerOption> &allowedContainerOptions, const std::set<double> &allowedCellSizeFactors,
             const std::set<TraversalOption> &allowedTraversalOptions,
             const std::set<DataLayoutOption> &allowedDataLayoutOptions,
             const std::set<Newton3Option> &allowedNewton3Options)
      : _containerOptions(allowedContainerOptions),
        _optimalConfig(Configuration()),
        _allConfigsTested(false),
        _tuningRequest(nullptr) {

    // @todo distribute the division process, so that not every rank has to traverse all configs
    // Count the total number of allowed and applicable configurations
    int totalNumConfigs = 0;
    for (auto &containerOption : allowedContainerOptions) {
      const std::set<TraversalOption> &allContainerTraversals =
        compatibleTraversals::allCompatibleTraversals(containerOption);
      std::set<TraversalOption> allowedAndApplicable;
      std::set_intersection(allowedTraversalOptions.begin(),    allowedTraversalOptions.end(),
                            allContainerTraversals.begin(),     allContainerTraversals.end(),
                            std::inserter(allowedAndApplicable, allowedAndApplicable.begin()));
      totalNumConfigs += allowedCellSizeFactors.size() *
                         allowedAndApplicable.size() *
                         allowedDataLayoutOptions.size() *
                         allowedNewton3Options.size();
    }

    int worldSize;
    AutoPas_MPI_Comm_size(AUTOPAS_MPI_COMM_WORLD, &worldSize);

    int worldRank;
    AutoPas_MPI_Comm_rank(AUTOPAS_MPI_COMM_WORLD, &worldRank);

    // divide search space into worldSize many blocks.
    const int blockSize = totalNumConfigs / worldSize;
    const int remainder = totalNumConfigs % worldSize;
    int start = blockSize * worldRank;
    int startNext = blockSize * (worldRank + 1);
    // distribute the remaining configurations over the lower ranks.
    if (worldRank < remainder) {
      start += worldRank;
      startNext += worldRank + 1;
    } else {
      start += remainder;
      startNext += remainder;
    }

    if (worldRank == 0) {
        AutoPasLog(debug, "number of ranks: {}", worldSize);
        AutoPasLog(debug, "total number of possible configurations: {}", totalNumConfigs);
    }
    populateSearchSpace(allowedContainerOptions, allowedCellSizeFactors,
                        allowedTraversalOptions, allowedDataLayoutOptions,
                        allowedNewton3Options,   start, startNext);

    if (_searchSpace.empty()) {
      populateSearchSpace(allowedContainerOptions, allowedCellSizeFactors,
                          allowedTraversalOptions, allowedDataLayoutOptions,
                          allowedNewton3Options,   0, totalNumConfigs);
      if (_searchSpace.empty()) {
        autopas::utils::ExceptionHandler::exception("No valid configuration could be generated");
      }
    }
    AutoPasLog(debug, "Points in search space: {}", _searchSpace.size());
    _tuningConfig = _searchSpace.begin();
 }

  /**
   * Constructor for the FullSearch that only contains the given configurations.
   * This constructor assumes only valid configurations are passed! Mainly for easier unit testing.
   * @param allowedConfigurations Set of configurations AutoPas can choose from.
   *
  explicit FullSearchMPI(std::set<Configuration> allowedConfigurations)
      : _containerOptions{}, _searchSpace(std::move(allowedConfigurations)), _currentConfig(_searchSpace.begin()) {
    for (auto config : _searchSpace) {
      _containerOptions.insert(config.container);
    }
  }
   */

  inline const Configuration &getCurrentConfiguration() const override {
    // the cellSizeFactor is -1 if _optimalConfig has been initialized to an invalid value
    if (_optimalConfig.cellSizeFactor == -1.) {
      return *_tuningConfig;
    } else {
      return _optimalConfig;
    }
  }

  inline void removeN3Option(Newton3Option badNewton3Option) override;

  inline void addEvidence(long time) override { _traversalTimes[*_tuningConfig] = time; }

  inline void reset() override {
    _traversalTimes.clear();
    _allConfigsTested = false;
    _tuningRequest = nullptr;
    _optimalConfig = Configuration();
    _tuningConfig = _searchSpace.begin();
  }

  inline bool tune(bool = false) override;

  inline std::set<ContainerOption> getAllowedContainerOptions() const override { return _containerOptions; }

  inline bool searchSpaceIsTrivial() const override { return _searchSpace.size() == 1; }

  inline bool searchSpaceIsEmpty() const override { return _searchSpace.empty(); }

 private:
  /**
   * Fills the search space with a subset of the cartesian product of the given options (minus invalid combinations).
   * @param allowedContainerOptions
   * @param allowedTraversalOptions
   * @param allowedDataLayoutOptions
   * @param allowedNewton3Options
   */
  inline void populateSearchSpace(const std::set<ContainerOption> &allowedContainerOptions,
                                  const std::set<double> &allowedCellSizeFactors,
                                  const std::set<TraversalOption> &allowedTraversalOptions,
                                  const std::set<DataLayoutOption> &allowedDataLayoutOptions,
                                  const std::set<Newton3Option> &allowedNewton3Options,
                                  const int start, const int startNext);

  /**
   * Uses information from all ranks to select the globally optimal configuration.
   */
  inline void selectOptimalConfiguration();

  std::set<ContainerOption> _containerOptions;
  std::set<Configuration> _searchSpace;
  std::set<Configuration>::iterator _tuningConfig;
  Configuration _optimalConfig;
  std::unordered_map<Configuration, size_t, ConfigHash> _traversalTimes;
  bool _allConfigsTested;
  AutoPas_MPI_Request _tuningRequest;
};

void FullSearchMPI::populateSearchSpace(const std::set<ContainerOption> &allowedContainerOptions,
                                        const std::set<double> &allowedCellSizeFactors,
                                        const std::set<TraversalOption> &allowedTraversalOptions,
                                        const std::set<DataLayoutOption> &allowedDataLayoutOptions,
                                        const std::set<Newton3Option> &allowedNewton3Options,
                                        const int start, const int startNext) {
  // Index to test which configurations apply to the current rank
  int i = 0;
  // Generate all potential configs
  for (auto &containerOption : allowedContainerOptions) {
    // Get all traversals of the container and restrict them to the allowed ones
    const std::set<TraversalOption> &allContainerTraversals =
        compatibleTraversals::allCompatibleTraversals(containerOption);
    std::set<TraversalOption> allowedAndApplicable;
    std::set_intersection(allowedTraversalOptions.begin(), allowedTraversalOptions.end(),
                          allContainerTraversals.begin(), allContainerTraversals.end(),
                          std::inserter(allowedAndApplicable, allowedAndApplicable.begin()));

    for (auto &cellSizeFactor : allowedCellSizeFactors) {
      for (auto &traversalOption : allowedAndApplicable) {
        for (auto &dataLayoutOption : allowedDataLayoutOptions) {
          for (auto &newton3Option : allowedNewton3Options) {
            if (i < start) {
              ++i;
              continue;
            }
            if (i == startNext) {
              return;
            }
            _searchSpace.emplace(containerOption, cellSizeFactor, traversalOption,
                                 dataLayoutOption, newton3Option);
            ++i;
          }
        }
      }
    }
  }
}

bool FullSearchMPI::tune(bool currentInvalid) {
  // Repeat as long as traversals are not applicable or we run out of configs
  ++_tuningConfig;

  if (_tuningConfig == _searchSpace.end()) {
    _allConfigsTested = true;
    // Do more testing until everyone is ready to select the optimal configuration
    _tuningConfig = _searchSpace.begin();
    AutoPasLog(debug, "Every configuration in this rank has been tested");
  }
  if (currentInvalid) {
    // maximally send one _tuningRequest per iteration
    return true;
  }

  // This if-statement will always be true after the first _tuningRequest (until next tuning interval)
  if (_tuningRequest != nullptr) {
    // Test if the _tuningRequest from the previous iteration was made by everyone
    int completed;
    AutoPas_MPI_Test(&_tuningRequest, &completed, AUTOPAS_MPI_STATUS_IGNORE);
    if (completed) {
      AutoPasLog(debug, "Initiate global tuning");
      selectOptimalConfiguration();
      return false;
    }
  } else if (_allConfigsTested) {
    AutoPas_MPI_Ibarrier(AUTOPAS_MPI_COMM_WORLD, &_tuningRequest);
    AutoPasLog(debug, "Requested global tuning");
  }

  return true;
}

void FullSearchMPI::selectOptimalConfiguration() {
  // Time measure strategy
  if (_traversalTimes.empty()) {
    utils::ExceptionHandler::exception(
        "FullSearchMPI: Trying to determine fastest configuration without any measurements! "
        "Either selectOptimalConfiguration was called too early or no applicable configurations were found");
  }

  auto optimum = std::min_element(_traversalTimes.begin(), _traversalTimes.end(),
                                  [](std::pair<Configuration, size_t> a, std::pair<Configuration, size_t> b) -> bool {
                                    return a.second < b.second;
                                  });

  int worldRank;
  AutoPas_MPI_Comm_rank(AUTOPAS_MPI_COMM_WORLD, &worldRank);

  AutoPasLog(debug, "Local optimal configuration: {}", optimum->first.toString());

  // Find the globally minimal traversal time.
  struct {
    size_t val;
    int rank;
  }in[1], out[1];
  in->val = optimum->second;
  in->rank = worldRank;
  AutoPas_MPI_Allreduce(in, out, 1, AUTOPAS_MPI_LONG_INT, AUTOPAS_MPI_MINLOC, AUTOPAS_MPI_COMM_WORLD);

  // The rank with the best configuration sends it to all other ranks
  Config_struct config;
  if (out->rank == worldRank) {
    config.container = (autopas::ContainerOption::Value) optimum->first.container;
    config.cellSizeFactor = optimum->first.cellSizeFactor;
    config.traversal = (autopas::TraversalOption::Value) optimum->first.traversal;
    config.dataLayout = (autopas::DataLayoutOption::Value) optimum->first.dataLayout;
    config.newton3 = (autopas::Newton3Option::Value) optimum->first.newton3;
  }
  AutoPas_MPI_Bcast(&config, 1, AUTOPAS_CONFIG, out->rank, AUTOPAS_MPI_COMM_WORLD);

  _optimalConfig = Configuration(
          ContainerOption((ContainerOption::Value)config.container),
          config.cellSizeFactor,
          TraversalOption((TraversalOption::Value)config.traversal),
          DataLayoutOption((DataLayoutOption::Value)config.dataLayout),
          Newton3Option((Newton3Option::Value)config.newton3));

  // There used to be a sanity check here for when the optimal config was not actually part of the search space.
  // Might have to be replaced.

  // Measurements are not needed anymore
  _traversalTimes.clear();

  AutoPasLog(debug, "Selected configuration: {}", _optimalConfig.toString());
}

void FullSearchMPI::removeN3Option(Newton3Option badNewton3Option) {
  for (auto ssIter = _searchSpace.begin(); ssIter != _searchSpace.end();) {
    if (ssIter->newton3 == badNewton3Option) {
      // change current config to the next non-deleted
      // if _tuningConfig is in the search space it is assumed that the process is currently tuning
      if (ssIter == _tuningConfig) {
        ssIter = _searchSpace.erase(ssIter);
        _tuningConfig = ssIter;
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
