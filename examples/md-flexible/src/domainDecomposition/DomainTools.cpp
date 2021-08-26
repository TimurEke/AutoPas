/**
 * @file DomainTools.cpp
 * @author J. Körner
 * @date 13.05.2021
 */
#include "DomainTools.h"

#include <algorithm>
#include <list>

#include "autopas/utils/ArrayMath.h"

namespace DomainTools {
bool isInsideDomain(const std::array<double, 3> &coordinates, const std::array<double, 3> &boxMin,
                    const std::array<double, 3> &boxMax) {
  bool isInsideLocalDomain = true;
  for (int i = 0; i < coordinates.size(); ++i) {
    if (not isInsideLocalDomain) {
      break;
    }
    isInsideLocalDomain = coordinates[i] >= boxMin[i] and coordinates[i] < boxMax[i];
  }
  return isInsideLocalDomain;
}

void generateDecomposition(unsigned int subdomainCount, std::array<int, 3> &decomposition) {
  std::list<int> primeFactors;
  // Add 2 to prime factorization as many times as subdomainCount is dividable by 2.
  while (subdomainCount % 2 == 0) {
    primeFactors.push_back(2);
    subdomainCount = subdomainCount / 2;
  }

  // Add every uneven number smaller than subdomainCount to the prime factors as long as subdomainCount is
  // dividable by this number. Uneven numbers which are not a prime number will be ignored, because subdomainCount
  // can not be divided by those numbers at the point they are being checked.
  for (int i = 3; i <= subdomainCount; i = i + 2) {
    while (subdomainCount % i == 0) {
      primeFactors.push_back(i);
      subdomainCount = subdomainCount / i;
    }
  }

  // Reduces the primeFactors to 3 elements, one for each dimension of the domain.
  // It multiplies the smallest two factors and stores it in the second factor.
  while (primeFactors.size() > 3) {
    primeFactors.sort();
    auto firstElement = primeFactors.front();
    primeFactors.pop_front();
    primeFactors.front() *= firstElement;
  }

  // If the prime factorization ends up having less factors than dimensions in the domain,
  // fill those dimensions with 1.
  for (auto &dimensionSize : decomposition) {
    if (not primeFactors.empty()) {
      dimensionSize = primeFactors.front();
      primeFactors.pop_front();
    } else {
      dimensionSize = 1;
    }
  }
}

int convertIdToIndex(const std::array<int, 3> &domainId, const std::array<int, 3> decomposition) {
  int domainIndex = 0;

  for (size_t i = 0; i < 3; ++i) {
    domainIndex += getAccumulatedTail(i, decomposition) * domainId[i];
  }

  return domainIndex;
}

std::array<int, 3> convertIndexToId(int domainIndex, const std::array<int, 3> decomposition) {
  std::array<int, 3> id{};

  for (size_t i = 0; i < 3; ++i) {
    int accumulatedTail = getAccumulatedTail(i, decomposition);
    id[i] = domainIndex / accumulatedTail;
    domainIndex -= accumulatedTail * id[i];
  }

  return id;
}

int getAccumulatedTail(const size_t index, const std::array<int, 3> decomposition) {
  int accumulatedTail = 1;
  if (index < decomposition.size() - 1) {
    accumulatedTail = std::accumulate(decomposition.begin() + index + 1, decomposition.end(), 1, std::multiplies<>());
  }
  return accumulatedTail;
}

std::array<int, 6> getExtentOfSubdomain(const int subdomainIndex, const std::array<int, 3> decomposition) {
  std::array<int, 6> extentOfSubdomain{};

  const std::array<int, 3> subdomainId = convertIndexToId(subdomainIndex, decomposition);

  for (size_t i = 0; i < 3; ++i) {
    extentOfSubdomain[2 * i] = subdomainId[i];
    extentOfSubdomain[2 * i + 1] = subdomainId[i] + 1;
  }

  return extentOfSubdomain;
}

}  // namespace DomainTools
