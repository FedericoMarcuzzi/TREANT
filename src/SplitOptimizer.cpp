#include <cassert>
#include <future>
#include <set>
#include <stdexcept>

#include <omp.h>

#include "SplitOptimizer.h"

// NLOPT test
#include <cmath>
#include <nlopt.hpp>
// TODO: delete this include (only for debug purposes)
#include <iomanip>
#include <iostream>

struct OptimizeOutput {
  gain_t bestGain;
  index_t bestSplitFeatureId;
  split_value_t bestSplitValue;
  split_value_t bestNextSplitValue;
  prediction_t bestPredLeft;
  prediction_t bestPredRight;
  double bestSSEuma;
};

SplitOptimizer::SplitOptimizer(Impurity impurityType) {

  switch (impurityType) {
  case Impurity::GINI:
    throw std::runtime_error("Implement GINI");
    break;
  case Impurity::SSE:
    getLoss_ = [](const Dataset &dataset, const indexes_t &validInstances,
                  label_t yPred) {
      double sum = 0.0;
      const auto &labels = dataset.getLabels();
      for (const auto &i : validInstances) {
        const double diff = labels[i] - yPred;
        sum += diff * diff;
      }
      return sum;
    };
    break;
  case Impurity::MSE:
    throw std::runtime_error("Implement MSE");
    break;
  case Impurity::ENTROPY:
    throw std::runtime_error("Implement ENTROPY");
    break;
  default:
    throw std::runtime_error("Not valid impurity type");
    break;
  }
}

std::tuple<indexes_t, indexes_t, indexes_t> SplitOptimizer::simulateSplit(
    const Dataset &dataset, const indexes_t &validInstances,
    const Attacker &attacker, const std::unordered_map<index_t, cost_t> &costs,
    const feature_t &splittingValue, const index_t &splittingFeature) const {

  indexes_t leftSplit, rightSplit, unknownSplit;

  // Prepare the output
  if (!(leftSplit.empty() && rightSplit.empty() && unknownSplit.empty())) {
    throw std::runtime_error(
        "ERROR in simulateSplit: all outputs must be empty at the beginning");
  }

  bool isNumerical = dataset.isFeatureNumerical(splittingFeature);

  for (const auto &i : validInstances) {
    const auto cost = costs.at(i);
    // The attack on a specific instance 'i' to its feature 'splittingFeature'
    // generates a set of new feature,
    // of those we are interested only in the i-th column
    const auto attacks =
        attacker.attack(dataset.getRecord(i), splittingFeature, cost);

    // See line 1014 of parallel_robust_forest.py
    bool allLeft = true;
    bool allRight = true;

    for (const auto &atk : attacks) {
      if (isNumerical) {
        if (atk.first[splittingFeature] <= splittingValue) {
          allRight = false;
        } else {
          allLeft = false;
        }
      } else {
        if (atk.first[splittingFeature] == splittingValue) {
          allRight = false;
        } else {
          allLeft = false;
        }
      }
      //
      if (!allLeft && !allRight) {
        break;
      }
    }
    // Modify the output vectors accordingly (see __simulate_split in python
    // code)
    if (allLeft) {
      leftSplit.push_back(i);
    } else if (allRight) {
      rightSplit.push_back(i);
    } else {
      unknownSplit.push_back(i);
    }
  } // end of loop over instances
  return {leftSplit, rightSplit, unknownSplit};
}

double SplitOptimizer::sseCostFunction(const std::vector<double> &x,
                                       std::vector<double> &grad,
                                       void *my_func_data) {
  // This function calculates f (see __sse_under_max_attack)
  static auto f =
      [](const std::vector<prediction_t> &y, const indexes_t &leftIndexes,
         const indexes_t &rightIndexes, const indexes_t &unknownIndexes,
         const std::vector<double> &leftRight) -> double {
    const auto &left = leftRight[0];
    const auto &right = leftRight[1];
    double ret = 0.0;
    // np.sum((L - left)**2.0)
    for (const auto &leftIndex : leftIndexes) {
      const double diff = static_cast<double>(y[leftIndex]) - left;
      ret += diff * diff;
    }
    // + np.sum((R - right)**2.0)
    for (const auto &rightIndex : rightIndexes) {
      const double diff = static_cast<double>(y[rightIndex]) - right;
      ret += diff * diff;
    }
    // + np.sum(np.maximum((U - left)**2.0, (U - right)**2.0))
    for (const auto &unknownIndex : unknownIndexes) {
      const double diffL = static_cast<double>(y[unknownIndex]) - left;
      const double diffR = static_cast<double>(y[unknownIndex]) - right;
      const double diffLsquared = diffL * diffL;
      const double diffRsquared = diffR * diffR;
      const auto max =
          diffLsquared > diffRsquared ? diffLsquared : diffRsquared;
      ret += max;
    }
    return ret;
  };

  // See slsqp.py line 31, function approx_jacobian
  static auto approxJacobian =
      [](const std::vector<prediction_t> &y, const indexes_t &leftIndexes,
         const indexes_t &rightIndexes, const indexes_t &unknownIndexes,
         const std::vector<double> &x0, const double &f0) {
        // See scipy.optimize.slsqp code default eps value
        static const double eps = 1.4901161193847656e-08;
        static const double zero = 0.0f;
        // Quick check on x0 size
        if (x0.size() != 2) {
          throw std::runtime_error("Gradient must have size = 2");
        }
        // Calculate the jacobian that is a vector of dimension 2
        std::vector<double> jac(2, zero);
        std::vector<double> xIncr(x0);
        for (std::size_t i = 0; i < x0.size(); ++i) {
          xIncr[i] += eps; // perturb the i-th entry of x0 with eps
          const auto f_xIncr =
              f(y, leftIndexes, rightIndexes, unknownIndexes, xIncr);
          jac[i] = (f_xIncr - f0) / eps;
          xIncr[i] = x0[i]; // reset xIncr[i]
        }
        return jac;
      };

  // Cast the extra data
  auto *d = reinterpret_cast<ExtraData *>(my_func_data);
  // Update the iteration count
  d->count_ += 1;
  // Initialize the returning value
  double fx =
      f(d->y_, d->leftIndexes_, d->rightIndexes_, d->unknownIndexes_, x);

  // Update the gradient
  if (!grad.empty()) {
    const auto jac = approxJacobian(d->y_, d->leftIndexes_, d->rightIndexes_,
                                    d->unknownIndexes_, x, fx);
    grad[0] = jac[0];
    grad[1] = jac[1];
  }
  //
  return fx;
}

double SplitOptimizer::constraintFunction(const std::vector<double> &x,
                                          std::vector<double> &grad,
                                          void *data) {

  // Cast to Constraint pointer
  auto *d = reinterpret_cast<Constraint *>(data);
  const auto direction = d->getDirection();
  const auto y = d->getY();
  const auto inequality = d->getInequality();
  const auto bound = d->getBound();

  double boundMinusYSquared = (bound - y) * (bound - y);
  double x0minusY = x[0] - y;
  double x1minusY = x[1] - y;
  double x0minusYSquared = x0minusY * x0minusY;
  double x1minusYSquared = x1minusY * x1minusY;

  if (!grad.empty()) {
    switch (direction) {
    case 'L':
      grad[0] = !inequality ? (2.0 * x0minusY) : (-2.0 * x0minusY);
      grad[1] = 0.0;
      break;
    case 'R':
      grad[0] = 0.0;
      grad[1] = !inequality ? (2.0 * x1minusY) : (-2.0 * x1minusY);
      break;
    case 'U':
      if (!inequality) {
        // Take the minimum
        if (x0minusYSquared < x1minusYSquared) {
          grad[0] = x0minusY;
          grad[1] = 0.0;
        } else {
          grad[0] = 0.0;
          grad[1] = x1minusY;
        }
      } else {
        // Take the maximum
        if (x0minusYSquared < x1minusYSquared) {
          grad[0] = 0.0;
          grad[1] = x1minusY;
        } else {
          grad[0] = x0minusY;
          grad[1] = 0.0;
        }
      }
      break;
    default:
      throw std::runtime_error(
          "Unexpected direction in SplitOptimizer::constraintFunction");
    }
  }

  double ret;
  // The inequality cases are swapped in respect to python code because in the
  // minimize function the constraints are supposed to be '>=' and in nlopt '<='
  switch (direction) {
  case 'L':
    ret = !inequality ? (x0minusYSquared - boundMinusYSquared)
                      : (-x0minusYSquared + boundMinusYSquared);
    break;
  case 'R':
    ret = !inequality ? (x1minusYSquared - boundMinusYSquared)
                      : (-x1minusYSquared + boundMinusYSquared);
    break;
  case 'U':
    if (!inequality) {
      // Take the minimum
      const double diff =
          x0minusYSquared < x1minusYSquared ? x0minusYSquared : x1minusYSquared;
      ret = diff - boundMinusYSquared;
    } else {
      // Take the maximum
      const double diff =
          x0minusYSquared < x1minusYSquared ? x1minusYSquared : x0minusYSquared;
      ret = diff - boundMinusYSquared;
    }
    break;
  default:
    throw std::runtime_error(
        "Unexpected direction in SplitOptimizer::constraintFunction");
  }

  return ret;
}

bool SplitOptimizer::optimizeSSE(const std::vector<label_t> &y,
                                 const indexes_t &leftSplit,
                                 const indexes_t &rightSplit,
                                 const indexes_t &unknownSplit,
                                 std::vector<Constraint> &constraints,
                                 label_t &yHatLeft, label_t &yHatRight,
                                 gain_t &sse) const {
  //
  // The method is hardcoded to nlopt::LD_SLSQP like in the python code.
  // The dimension of the problem is 2: we are looking for yHatLeft and
  // yHatRight.
  nlopt::opt opt(nlopt::LD_SLSQP, 2);
  auto extraData = ExtraData(y, leftSplit, rightSplit, unknownSplit);
  // Set the cost function to minimize
  opt.set_min_objective(sseCostFunction, &extraData);
  // Add the constraints
  for (auto &constraint : constraints) {
    opt.add_inequality_constraint(constraintFunction, &constraint, 1e-8);
  }
  // Set the tolerance (ftol in python -> acc -> fortran code)
  opt.set_ftol_abs(1e-6);
  opt.set_maxeval(100);
  // Initialize the x vector
  std::vector<feature_t> x = {yHatLeft, yHatRight};
  // Initialize the value of the function
  double f = 0.0;
  try {
    nlopt::result result = opt.optimize(x, f);
    if (result < nlopt::result::SUCCESS) {
      std::cout << "The result is not SUCCESS. Error code is " << result;
      return false;
    } else {
      //      std::cout << "found minimum after " << extraData.count_ << "
      //      evaluations"
      //                << std::endl;
      //      std::cout << "found minimum at f(" << x[0] << "," << x[1]
      //                << ") = " << std::setprecision(10) << f << std::endl;
      // Update the outputs
      yHatLeft = x[0];
      yHatRight = x[1];
      sse = f;
      return true;
    }
  } catch (std::exception &e) {
    std::cout << "nlopt failed: '" << e.what() << "', the leftSplit.size is "
              << leftSplit.size() << ", the rightSplit.size is "
              << rightSplit.size() << ", the unknownSplit.size is "
              << unknownSplit.size() << std::endl;
    return false;
  }
}

bool SplitOptimizer::optimizeGain(
    const Dataset &dataset, const indexes_t &validInstances,
    const indexes_t &validFeatures, const Attacker &attacker,
    const std::unordered_map<index_t, cost_t> &costs,
    const std::vector<Constraint> &constraints, const double &currentScore,
    const double &currentPredictionScore, // (used by)/(forward to) optimizeSSE
    const unsigned& numThreads,
    // outputs
    // TODO: better put this in 2 structs (one left, one right) and return it
    gain_t &bestGain, indexes_t &bestSplitLeft, indexes_t &bestSplitRight,
    index_t &bestSplitFeatureId, split_value_t &bestSplitValue,
    split_value_t &bestNextSplitValue, prediction_t &bestPredLeft,
    prediction_t &bestPredRight, double &bestSSEuma,
    std::vector<Constraint> &constraintsLeft,
    std::vector<Constraint> &constraintsRight,
    std::unordered_map<index_t, cost_t> &costsLeft,
    std::unordered_map<index_t, cost_t> &costsRight) const {

  // In general, assert diagnoses an error in the implementation: at this point
  // the validInstances vector can not be empty.
  assert(!validInstances.empty());

  const auto optimizeOnSubset =
      [this](const Dataset &dataset,
             const std::unordered_map<index_t, cost_t> &costs,
             const Attacker &attacker, const indexes_t &validFeaturesSubset,
             const indexes_t &validInstances,
             const std::vector<Constraint> &constraints,
             const double &currentScore,
             const double &currentPredictionScore) -> OptimizeOutput {
    auto ret = OptimizeOutput();
    ret.bestGain = -1.0f;
    ret.bestSplitFeatureId = *validFeaturesSubset.begin();
    ret.bestSplitValue = 0.0f;
    ret.bestNextSplitValue = 0.0f;
    ret.bestPredLeft = 0.0f;
    ret.bestPredRight = 0.0f;
    ret.bestSSEuma = 0.0f;

    for (const auto &splittingFeature : validFeaturesSubset) {
      // Build a set of unique feature values
      bool isNumerical = dataset.isFeatureNumerical(splittingFeature);
      const auto &currentColumn = dataset.getFeatureColumn(splittingFeature);
      // If not numerical the order can change with respect of dictionary
      // "feature_map" in python for example ("Male":5, "Female":10) in
      // python ->
      // ("Female", "Male") but here we maintain the original order: (5,
      // 10), in practise does not change anything
      const std::set<feature_t> uniqueFeatureValues(currentColumn.begin(),
                                                    currentColumn.end());

      for (auto it = uniqueFeatureValues.begin();
           it != uniqueFeatureValues.end(); ++it) {
        // Using the iterator in order to evaluate next value (see
        // bestNextSplitValue in the code below)
        const auto &splittingValue = *it;

        // find the best split with this value
        // line 1169 of the python code it is called self.__simulate_split
        auto [leftSplit, rightSplit, unknownSplit] =
            simulateSplit(dataset, validInstances, attacker, costs,
                          splittingValue, splittingFeature);
        // Propagate the constraints (see lines 1177-1190)
        std::vector<Constraint> updatedConstraints;
        for (const auto &c : constraints) {
          // This part can be optimized: do we need all the Constraint
          // object
          const auto cLeft = c.propagateLeft(attacker, splittingFeature,
                                             splittingValue, isNumerical);
          const auto cRight = c.propagateRight(attacker, splittingFeature,
                                               splittingValue, isNumerical);
          if (cLeft.has_value() && cRight.has_value()) {
            updatedConstraints.push_back(c);
            updatedConstraints.back().setDirection('U');
          } else if (cLeft.has_value()) {
            updatedConstraints.push_back(c);
            updatedConstraints.back().setDirection('L');
          } else if (cRight.has_value()) {
            updatedConstraints.push_back(c);
            updatedConstraints.back().setDirection('R');
          }
        }

        feature_t yHatLeft = currentPredictionScore;
        feature_t yHatRight = currentPredictionScore;
        feature_t sse = 0.0;
        bool optSuccess = optimizeSSE(
            dataset.getLabels(), leftSplit, rightSplit, unknownSplit,
            updatedConstraints, yHatLeft, yHatRight, sse);

        if (optSuccess) {
          const double currGain = currentScore - sse;
          if (currGain > ret.bestGain) {
            ret.bestGain = currGain;
            ret.bestSplitFeatureId = splittingFeature;
            ret.bestSplitValue = splittingValue;
            ret.bestNextSplitValue = std::next(it) == uniqueFeatureValues.end()
                                         ? ret.bestSplitValue
                                         : *std::next(it);
            ret.bestPredLeft = yHatLeft;
            ret.bestPredRight = yHatRight;
            ret.bestSSEuma = sse;
          }
        }
      } // end loop on feature values
    }   // end loop on valid features
    return ret;
  };

  // Split in subsets the features
  const std::vector<std::vector<index_t>> batches =
      [](const unsigned &numThreads, const indexes_t &validFeatures) {
        std::vector<std::vector<index_t>> ret;
        if (!numThreads) {
          throw std::runtime_error("Invalid number of threads");
        } else if (numThreads == 1) {
          ret.emplace_back(validFeatures.begin(), validFeatures.end());
        } else if (numThreads < validFeatures.size()) {
          unsigned chunkSize = validFeatures.size() / numThreads;
          for (unsigned i = 0; i < numThreads; ++i) {
            auto start = validFeatures.begin() + (chunkSize * i);
            auto end =
                (i == numThreads - 1) ? validFeatures.end() : start + chunkSize;
            ret.emplace_back(start, end);
          }
        } else { // one thread each feature
          for (const auto& f : validFeatures) {
            ret.push_back({f});
          }
        }
        return ret;
      }(numThreads, validFeatures);

  std::vector<std::future<OptimizeOutput>> batchResults(numThreads);
  for (std::size_t i = 0; i < batches.size(); ++i) {
    batchResults[i] =
        std::async(std::launch::async, optimizeOnSubset, dataset, costs,
                   attacker, batches[i], validInstances, constraints,
                   currentScore, currentPredictionScore);
  }

  // Default value of the gain, returns how much we can improve the current
  // score.
  bestGain = -1.0f;
  // Reduction
  for (auto& fut : batchResults) {
    const auto result = fut.get();
    if (result.bestGain > bestGain) {
      bestGain = result.bestGain;
      bestSplitFeatureId = result.bestSplitFeatureId;
      bestSplitValue = result.bestSplitValue;
      bestNextSplitValue = result.bestNextSplitValue;;
      bestPredLeft = result.bestPredLeft;
      bestPredRight = result.bestPredRight;
      bestSSEuma = result.bestSSEuma;
    }
  }

  // If the bestGain is > 0.0 than there is an improvement on the solution
  // otherwise we return false
  indexes_t bestSplitUnknown;
  if (bestGain > 0.0) {
    // Recover the best split: bestSplitLeft, bestSplitRight, bestSplitUnknown
    {
      auto [leftSplit, rightSplit, unknownSplit] =
          simulateSplit(dataset, validInstances, attacker, costs,
                        bestSplitValue, bestSplitFeatureId);
      bestSplitLeft = std::move(leftSplit);
      bestSplitRight = std::move(rightSplit);
      bestSplitUnknown = std::move(unknownSplit);
    }
    // Update constraints (see line 1289 of python code)
    if (!(constraintsLeft.empty() && constraintsRight.empty())) {
      throw std::runtime_error(
          "left and right constraints must be empty at the beginning");
    }
    if (!(costsLeft.empty() && costsRight.empty())) {
      throw std::runtime_error(
          "left and right costs must be empty at the beginning");
    }

    for (const auto &c : constraints) {
      auto cLeftOpt =
          c.propagateLeft(attacker, bestSplitFeatureId, bestSplitValue,
                          dataset.isFeatureNumerical(bestSplitFeatureId));
      if (cLeftOpt.has_value()) {
        constraintsLeft.push_back(cLeftOpt.value());
      }
      auto cRightOpt =
          c.propagateRight(attacker, bestSplitFeatureId, bestSplitValue,
                           dataset.isFeatureNumerical(bestSplitFeatureId));
      if (cRightOpt.has_value()) {
        constraintsRight.push_back(cRightOpt.value());
      }
    }
    // Manage the unknown indexes, where are they going?
    const auto &y = dataset.getLabels();
    // see loop at line 1299
    indexes_t unknownIndexesToLeft;
    indexes_t unknownIndexesToRight;
    for (const auto &unknownIndex : bestSplitUnknown) {
      const auto instance = dataset.getRecord(unknownIndex);
      const auto attacks =
          attacker.attack(instance, bestSplitFeatureId, costs.at(unknownIndex));

      // Assuming the
      cost_t costMinLeft = attacks[0].second;
      // TODO: now I should find the minCost among the attacks, on the left side
      //       there is always an attack with the original instance (that has
      //       the lowest cost and, I suppose, is the first)
      //       is it ok?
      const auto diffLeft = y[unknownIndex] - bestPredLeft;
      const auto unknownToLeft = diffLeft < 0.0 ? -diffLeft : diffLeft;
      const auto diffRight = y[unknownIndex] - bestPredRight;
      const auto unknownToRight = diffRight < 0.0 ? -diffRight : diffRight;
      if (unknownToLeft > unknownToRight) { // see line 1324 python
        costsLeft[unknownIndex] = costMinLeft;
        // Update the left indexes
        unknownIndexesToLeft.emplace_back(unknownIndex);
        // Checked with Lucchese: it is correct to have bestPredRight as bound
        constraintsLeft.emplace_back(instance, y[unknownIndex], costMinLeft,
                                     true, bestPredRight);
        constraintsRight.emplace_back(instance, y[unknownIndex], costMinLeft,
                                      false, bestPredRight);
      } else {
        const cost_t costMinRight = [&]() {
          std::vector<cost_t> costsOnRight;
          for (const auto &atk : attacks) {
            if (dataset.isFeatureNumerical(bestSplitFeatureId)) {
              if (atk.first[bestSplitFeatureId] > bestSplitValue) {
                costsOnRight.push_back(atk.second);
              }
            } else {
              if (atk.first[bestSplitFeatureId] != bestSplitValue) {
                costsOnRight.push_back(atk.second);
              }
            }
          }
          if (costsOnRight.empty()) {
            throw std::runtime_error(
                "Cannot determine a valid cost for the right");
          }
          std::sort(costsOnRight.begin(), costsOnRight.end());
          return *costsOnRight.begin();
        }();
        costsRight[unknownIndex] = costMinRight;
        // Update the right indexes
        unknownIndexesToRight.emplace_back(unknownIndex);
        // TODO: check with Lucchese if it is correct to have bestPredLeft as
        //   bound
        constraintsLeft.emplace_back(instance, y[unknownIndex], costMinRight,
                                     false, bestPredLeft);
        constraintsRight.emplace_back(instance, y[unknownIndex], costMinRight,
                                      true, bestPredLeft);
      }
    }
    // Update the left and right costs (no unknown indexes considered)
    for (const auto &leftIndex : bestSplitLeft) {
      costsLeft[leftIndex] = costs.at(leftIndex);
    }
    for (const auto &rightIndex : bestSplitRight) {
      costsRight[rightIndex] = costs.at(rightIndex);
    }
    // Update the indexes on the left and on the right with the unknown indexes
    for (const auto &i : unknownIndexesToLeft) {
      bestSplitLeft.emplace_back(i);
    }
    for (const auto &i : unknownIndexesToRight) {
      bestSplitRight.emplace_back(i);
    }
    //
    return true;
  } else {
    return false;
  }
}

double SplitOptimizer::evaluateSplit(const Dataset &dataset,
                                     const indexes_t &rows,
                                     prediction_t prediction) const {
  return getLoss_(dataset, rows, prediction);
}

SplitOptimizer::ExtraData::ExtraData(const std::vector<label_t> &y,
                                     const indexes_t &leftIndexes,
                                     const indexes_t &rightIndexes,
                                     const indexes_t &unknownIndexes)
    : y_(y), leftIndexes_(leftIndexes), rightIndexes_(rightIndexes),
      unknownIndexes_(unknownIndexes), count_(0) {}