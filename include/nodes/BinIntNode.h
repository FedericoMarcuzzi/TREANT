//
// Created by dg on 05/11/19.
//

#ifndef TREEANT_BININTNODE_H
#define TREEANT_BININTNODE_H

#include <stdexcept>
#include "nodes/INode.h"

/**
 * Binary integer node (partition in two parts depending on an integer value)
 */
class BinIntNode : public INode {
public:
  explicit BinIntNode(index_t featureIndex, int_feature_t v, INode *leftChild,
                      INode *rightChild);

  ~BinIntNode() override;

  [[nodiscard]] std::vector<INode *> getChildren() const override;
  void setChild(std::size_t index, INode *newNodePtr) override;
  void setFeatureIndex(std::size_t index) override;

  // This function is useful for building a decision tree.
  [[nodiscard]] std::vector<std::vector<index_t>>
  split(const std::vector<index_t> &validIndexes,
        const feature_vector_t &featureVector) override {
    const auto intVectorPtr = std::get_if<int_vector_t>(&featureVector);
    if (!intVectorPtr) {
      throw std::runtime_error(
          "BinIntNode::split() called on a non-int_vector_t");
    }
    // Set v as the average of the vector
    v = 0;
    for (const auto& i : validIndexes) {
      v += (*intVectorPtr)[i];
    }
    v /= validIndexes.size();
    // Create the 2 partitions
    std::vector<std::vector<index_t>> p(2);
    for (const auto& i : validIndexes) {
      if ((*intVectorPtr)[i] > v) {
        p[1].push_back(i);
      } else {
        p[0].push_back(i);
      }
    }
    //
    return p;
  }

  // This function is useful to use the decision tree. It should call
  // recursively predict on the next child.
  [[nodiscard]] label_t predict(const record_t &record) const override {
    const auto intFeaturePtr = std::get_if<int_feature_t>(&record[featureIndex_]);
    if (!intFeaturePtr) {
      throw std::runtime_error("BinIntNode::predict() called on a non-int_feature_t");
    }
    else if (*intFeaturePtr > v) {
      return rightChild->predict(record);
    } else {
      return leftChild->predict(record);
    }
  }

private:
  index_t featureIndex_;
  int_feature_t v;
  INode *leftChild = nullptr;
  INode *rightChild = nullptr;
};

#endif // TREEANT_BININTNODE_H
