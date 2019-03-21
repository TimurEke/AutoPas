/**
 * @file ContainerOption.h
 * @author F. Gratl
 * @date 1/18/19
 */

#pragma once

#include <vector>

namespace autopas {

/**
 * Possible choices for the particle container type.
 */
enum ContainerOption {
  directSum = 0,
  linkedCells = 1,
  verletLists = 2,
  verletListsCells = 3,
  verletClusterLists = 4,
};

/**
 * Provides a way to iterate over the possible choices of ContainerOption.
 */
static const std::vector<ContainerOption> allContainerOptions = {
    ContainerOption::directSum,        ContainerOption::linkedCells,        ContainerOption::verletLists,
    ContainerOption::verletListsCells, ContainerOption::verletClusterLists,
};

}  // namespace autopas