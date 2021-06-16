/**
 * @file DomainTools.cpp
 * @author J. Körner
 * @date 13.05.2021
 */
#include "DomainTools.h"

#include <cmath>
#include <list>

namespace DomainTools {
bool isInsideDomain(const std::vector<double> &coordinates, std::vector<double> &boxMin, std::vector<double> &boxMax) {
  bool isInsideLocalDomain = true;
  for (int i = 0; i < coordinates.size(); ++i) {
    if (!isInsideLocalDomain) {
      break;
    }
    isInsideLocalDomain = coordinates[i] >= boxMin[i] && coordinates[i] < boxMax[i];
  }
  return isInsideLocalDomain;
}

bool isInsideDomain(const std::array<double, 3> &coordinates, std::array<double, 3> &boxMin, std::array<double, 3> &boxMax) {
  bool isInsideLocalDomain = true;
  for (int i = 0; i < coordinates.size(); ++i) {
    if (!isInsideLocalDomain) {
      break;
    }
    isInsideLocalDomain = coordinates[i] >= boxMin[i] && coordinates[i] < boxMax[i];
  }
  return isInsideLocalDomain;
}

double getDistanceToDomain(const std::vector<double> &coordinates, std::vector<double> &boxMin, std::vector<double> &boxMax) {
  if ( coordinates.size() == boxMin.size() && coordinates.size() == boxMax.size()) {
    std::vector<double> differences(coordinates.size(), 0.0);
    for (int i = 0; i < coordinates.size(); ++i) {
      if (coordinates[i] < boxMin[i]){
        differences[i] = boxMin[i] - coordinates[i];
      }
      else if (coordinates[i] > boxMax[i]) {
        differences[i] = coordinates[i] - boxMax[i];
      }
    }
  
    double distance = 0.0;
    for (const auto &difference : differences) {
      distance += std::pow(difference, 2.0);
    }
  
    return std::pow(distance, 1.0 / differences.size());
  }
  return -1;
}

double getDistanceToDomain(const std::array<double, 3> &coordinates, std::array<double, 3> &boxMin, std::array<double, 3> &boxMax) {
  if ( coordinates.size() == boxMin.size() && coordinates.size() == boxMax.size()) {
    std::vector<double> differences(coordinates.size(), 0.0);
    for (int i = 0; i < coordinates.size(); ++i) {
      if (coordinates[i] < boxMin[i]){
        differences[i] = boxMin[i] - coordinates[i];
      }
      else if (coordinates[i] > boxMax[i]) {
        differences[i] = coordinates[i] - boxMax[i];
      }
    }
  
    double distance = 0.0;
    for (const auto &difference : differences) {
      distance += std::pow(difference, 2.0);
    }
  
    return std::pow(distance, 1.0 / differences.size());
  }
  return -1;
}

void generateDecomposition(unsigned int subdomainCount, int dimensionCount, std::vector<int> &oDecomposition) {
  std::list<int> primeFactors;
  while (subdomainCount % 2 == 0) {
    primeFactors.push_back(2);
    subdomainCount = subdomainCount / 2;
  }

  for (unsigned int i = 3; i <= subdomainCount; i = i + 2) {
    while (subdomainCount % i == 0) {
      primeFactors.push_back(i);
      subdomainCount = subdomainCount / i;
    }
  }

  while (primeFactors.size() > dimensionCount) {
    primeFactors.sort();
    auto firstElement = primeFactors.front();
    primeFactors.pop_front();
    primeFactors.front() *= firstElement;
  }

  oDecomposition.resize(dimensionCount);

  for (auto &dimensionSize : oDecomposition) {
    if (primeFactors.size() > 0) {
      dimensionSize = primeFactors.front();
      primeFactors.pop_front();
    } else {
      dimensionSize = 1;
    }
  }
}
}  // namespace DomainTools
