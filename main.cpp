#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <queue>
#include <unistd.h>

#include "BaggingClassifier.h"
#include "Dataset.h"
#include "DecisionTree.h"
#include "Node.h"
#include "SplitOptimizer.h"
#include "utils.h"

#include <cassert>
#include <fstream>
#include <iomanip>
#include <thread>
#include <cmath>

int main(int argc, char **argv) {

  if (argc < 2) {
    std::cout << "Usage: possible flags are:\n"
              << "-a <name of the attacker json file>, "
              << "-b <budget>, "
              << "-d <max depth>, "
              << "-f <dataset file path>, "
              << "-e <number of estimators>, "
              << "-i <0 for treant, 1 for icml>, "
              << "-o <output model filename>, "
              << "-j <number of threads>\n"
              << "Example:\n./" << argv[0]
              << " -a ../data/attacks.json -b 60 -d 4 -f "
                 "../data/test_training_set_n-1000.txt";
  }

  const auto [attackerFile, datasetFile, maxDepth, budget,
  threads, estimators, icml, outputFile] = [](const int argc, char *const *argv)
      -> std::tuple<std::string, std::string, std::size_t, cost_t, int, int, int, std::string> {
    std::string attackerFile, datasetFile, outputFile;
    std::size_t maxDepth = 1; // default maxDepth value is 1
    cost_t budget = 0.0f;     // default value is 0.0
    int threads = 1;          // default value is 1, sequential execution
    int estimators = 1;       // number of estimators
    int icml = 0;             // train with treant or icml2019
    // parse the arguments
    {
      double bflag = budget;
      int dflag = maxDepth;
      int jflag = threads;
      int eflag = estimators;
      int iflag = icml;
      int c;
      opterr = 0;

      // -a and -f are mandatory
      while ((c = getopt(argc, argv, "a:b:d:f:j:e:i:o:")) != -1)
        switch (c) {
        case 'a':
          attackerFile = std::string(optarg);
          break;
        case 'b':
          bflag = std::stod(std::string(optarg));
          if (budget < 0.0) {
            throw std::runtime_error(
                "Invalid budget argument: it must be >= 0.0");
          }
          budget = bflag;
          break;
        case 'd':
          dflag = std::stoi(std::string(optarg));
          if (dflag < 0) {
            throw std::runtime_error("Invalid depth argument: it must be >= 0");
          }
          maxDepth = dflag;
          break;
        case 'f':
          datasetFile = std::string(optarg);
          break;
        case 'j':
          jflag = std::stoi(std::string(optarg));
          if (jflag < 1) {
            throw std::runtime_error(
                "Invalid threads argument: it must be > 0");
          }
          threads = jflag;
          break;
        case 'e':
          eflag = std::stoi(std::string(optarg));
          if (eflag < 1) {
            throw std::runtime_error(
                "Invalid number of estimators argument: it must be > 0");
          }
          estimators = eflag;
          break;
        case 'i':
          iflag = std::stoi(std::string(optarg));
          if (iflag != 0 && iflag != 1) {
            throw std::runtime_error(
                "Invalid icml argument: it must be > 0");
          }
          icml = iflag;
          break;
        case 'o':
          outputFile = std::string(optarg);
          break;
        case '?':
          if (isprint(optopt)) {
            fprintf(stderr, "Unknown option '-%c'.\n", optopt);
          }
          throw std::runtime_error(
              "Unknown option character, valids are: -a, -b, -d, -f, -j, -e, -i, -o");
        default:
          abort();
        }
    }
    return {attackerFile, datasetFile, maxDepth, budget, threads, estimators, icml, outputFile};
  }(argc, argv);

  // Allocate dataset matrix X and label vector y
  const auto [rows, columnNames] = Dataset::getDatasetInfoFromFile(datasetFile);
  const unsigned cols = columnNames.size();
  // The principal program must manage the memory of X and Y
  feature_t *X = (feature_t *)malloc(sizeof(feature_t) * rows * cols);
  label_t *y = (label_t *)malloc(sizeof(label_t) * rows);
  const auto [isNumerical, notNumericalEntries] =
  Dataset::fillXandYfromFile(X, rows, cols, y, datasetFile);
  std::cout << "The notNumericalEntries size is :" << notNumericalEntries.size()
            << std::endl;

  Dataset dataset(X, rows, cols, y, utils::join(isNumerical, ','),
                  utils::join(notNumericalEntries, ','),
                  utils::join(columnNames, ','));

  if (budget < 0) {
    throw std::runtime_error(
        "ERROR DecisionTree::fit: Invalid "
        "input data (budget must be positive or equal to zero)");
  }
  Attacker attacker(dataset, attackerFile, budget);
  std::cout << "The dataset size is: " << dataset.size() << std::endl;
  std::cout << "internal threads on columns = " << threads << std::endl;
  //
  bool useICML2019 = false;
  if (icml)
    useICML2019 = true;
  // minimum instances per node (under this threshold the node became a leaf)
  const unsigned minPerNode = 20;
  const bool isAffine = false;

  BaggingClassifier baggingClassifier;
  std::cout << "Fitting the BaggingClassifier\n";
  baggingClassifier.setMaxFeatures(1.0);
  baggingClassifier.setEstimators(estimators);
  baggingClassifier.setJobs(2);
  baggingClassifier.setWithReplacement(true);
  baggingClassifier.fit(dataset, attacker, useICML2019, maxDepth, minPerNode,
                        isAffine);
  std::cout << "End of fitting the BaggingClassifier\n";

  const std::string filePath(outputFile);
  baggingClassifier.save(filePath);

  // Free memory
  free((void *)X);
  free((void *)y);
  return 0;
}