/**
 * @file GaussianProcess.h
 * @author Jan Nguyen
 * @date 17.05.19
 */

#pragma once

#include <Eigen/Dense>
#include "autopas/utils/ExceptionHandler.h"

namespace autopas {

/**
 * Different acquisition functions
 */
enum AcquisitionFunction {
  /**
   * Upper confidence bound
   */
  ucb,
  /**
   * Lower confidence bound
   */
  lcb,
  /**
   * mean
   */
  mean
};

/**
 * Gaussian process is a stochastical model. It predicts the
 * output of a blackbox function for given input. To do so some sample
 * input-output pairs of the function should be provided as evidence.
 *
 * Currently the default mean is 0 and squared exponential kernel is used.
 * TODO: maybe offer some options.
 * @tparam Vector
 */
template <class Vector>
class GaussianProcess {
 public:
  /**
   * Construct a gaussian process
   * @param theta prior variance
   * @param dimScale scale each dimension before applying kernel
   * @param sigma fixed noise
   */
  GaussianProcess(double theta, std::vector<double> dimScale, double sigma)
      : _inputs(),
        _outputs(),
        _theta(theta),
        _dimScale(Eigen::Map<Eigen::VectorXd>(dimScale.data(), dimScale.size())),
        _sigma(sigma),
        _covMat(),
        _covMatInv(),
        _weights() {}

  /**
   * Discard all evidences.
   */
  void clear() {
    // As long is _input is empty the matrices ar consided uninitialized.
    // So no further actions needed.
    _inputs.clear();
  }

  /**
   * Get the number of evidences provided.
   * @return
   */
  size_t numEvidences() const { return _inputs.size(); }

  /**
   * Provide a the output for a blackbox function
   * with given output.
   * @param input
   * @param output
   */
  void addEvidence(Vector input, double output) {
    _inputs.push_back(input);
    long newSize = _inputs.size();

    // extend output vector
    _outputs.conservativeResize(newSize, Eigen::NoChange_t());
    _outputs(newSize - 1) = output;

    // extend covariance matrix
    _covMat.conservativeResize(newSize, newSize);
    for (unsigned i = 0; i < newSize - 1; ++i) {
      _covMat(newSize - 1, i) = _covMat(i, newSize - 1) = kernel(input, _inputs[i]);
    }
    _covMat(newSize - 1, newSize - 1) = kernel(input, input) + _sigma;  // add fixed noise to diagonal

    // calculate needed matrix and vector for predictions
    _covMatInv = _covMat.inverse();
    _weights = _covMatInv * _outputs;
  }

  /**
   * Predict the expected output of the blackbox function
   * at input given the evidence so far.
   * @param input
   * @return mean
   */
  double predictMean(const Vector &input) const {
    if (_inputs.size() == 0) return 0.;

    return kernelVector(input).dot(_weights);
  }

  /**
   * The variance of the predicted output of predictMean().
   * @param input
   * @return variance
   */
  double predictVar(const Vector &input) const {
    if (_inputs.size() == 0) return kernel(input, input);

    Eigen::VectorXd kVec = kernelVector(input);
    return kernel(input, input) - kVec.dot(_covMatInv * kVec);
  }

  /**
   * Calculate the acquisition function for given vector
   * @param af
   * @param feature
   * @return
   */
  inline double calcAcquisition(AcquisitionFunction af, const Vector &feature) const {
    switch (af) {
      case ucb: {
        return predictMean(feature) + std::sqrt(predictVar(feature));
      }
      case lcb: {
        return predictMean(feature) - std::sqrt(predictVar(feature));
      }
      case mean: {
        return predictMean(feature);
      }
    }

    autopas::utils::ExceptionHandler::exception("GaussianProcess.calcAcquisition: Unknown acquisition function {}.",
                                                af);
    return 0;
  }

  /**
   * Find Vector in samples which maximizes
   * given aquisition function.
   * TODO: maybe add parameters for hyperparameters of aquisition functions
   * @param af function to maximize
   * @param samples
   * @return
   */
  Vector sampleAquisitionMax(AcquisitionFunction af, const std::vector<Vector> &samples) const {
    int maxIdx = -1;
    double maxVal = 0.;

    // find maximum from samples
    for (unsigned i = 0; i < samples.size(); ++i) {
      double val = calcAcquisition(af, samples[i]);

      if (maxIdx == -1 || val > maxVal) {
        maxIdx = i;
        maxVal = val;
      }
    }

    return samples[maxIdx];
  }

  /**
   * Find Vector in samples which minimizes
   * given aquisition function.
   * TODO: maybe add parameters for hyperparameters of aquisition functions
   * @param af function to minimize
   * @param samples
   * @return
   */
  Vector sampleAquisitionMin(AcquisitionFunction af, const std::vector<Vector> &samples) const {
    int minIdx = -1;
    double minVal = 0.;

    // find maximum from samples
    for (unsigned i = 0; i < samples.size(); ++i) {
      double val = calcAcquisition(af, samples[i]);

      if (minIdx == -1 || val < minVal) {
        minIdx = i;
        minVal = val;
      }
    }

    return samples[minIdx];
  }

 private:
  /**
   * Subtract two vectors
   * @param f1
   * @param f2
   * @return
   */
  Eigen::VectorXd subtract(const Vector &f1, const Vector &f2) const;

  /**
   * Kernel function to describe similarity between two features
   * @param f1
   * @param f2
   * @return
   */
  inline double kernel(const Vector &f1, const Vector &f2) const {
    Eigen::VectorXd r = subtract(f1, f2);
    Eigen::VectorXd rSquared = r.array().square();
    return _theta * exp(-rSquared.dot(_dimScale));
  }

  /**
   * Calulates the kernel between input and all evidences
   * @param input
   * @return Vector of covariances
   */
  Eigen::VectorXd kernelVector(const Vector &input) const {
    std::vector<double> k;
    for (auto &d : _inputs) {
      k.push_back(kernel(input, d));
    }
    return Eigen::Map<Eigen::VectorXd>(k.data(), k.size());
  }

  std::vector<Vector> _inputs;
  Eigen::VectorXd _outputs;

  /**
   * prior variance
   */
  double _theta;
  /**
   * Scale distance of each feature
   */
  Eigen::VectorXd _dimScale;
  /**
   * fixed noise assumed
   */
  const double _sigma;

  Eigen::MatrixXd _covMat;
  Eigen::MatrixXd _covMatInv;
  Eigen::VectorXd _weights;
};

template <class Vector>
inline Eigen::VectorXd GaussianProcess<Vector>::subtract(const Vector &f1, const Vector &f2) const {
  auto r = f1 - f2;
  std::vector<double> rData(std::begin(r), std::end(r));
  return Eigen::Map<Eigen::VectorXd>(rData.data(), rData.size());
}

template <>
inline Eigen::VectorXd GaussianProcess<Eigen::VectorXd>::subtract(const Eigen::VectorXd &f1,
                                                                  const Eigen::VectorXd &f2) const {
  return f1 - f2;
}
}  // namespace autopas
