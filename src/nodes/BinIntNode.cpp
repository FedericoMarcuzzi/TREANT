//
// Created by dg on 05/11/19.
//

#include "nodes/BinIntNode.h"
#include <stdexcept>

BinIntNode::BinIntNode(index_t featureIndex, int_feature_t v, INode *leftChild,
                       INode *rightChild)
    : featureIndex_(featureIndex), v(v), leftChild(leftChild),
      rightChild(rightChild) {}

BinIntNode::~BinIntNode() {
  delete leftChild;
  leftChild = nullptr;
  delete rightChild;
  rightChild = nullptr;
}

std::vector<INode *> BinIntNode::getChildren() const {
  return {leftChild, rightChild};
}

void BinIntNode::setChild(std::size_t index, INode *newNodePtr) {
  switch (index)
  {
  case 0:
    leftChild = newNodePtr;
    break;
  case 1:
    rightChild = newNodePtr;
    break;
  default:
    throw std::runtime_error(
        "Invalid usage of setChild in BinDoubleNode, index is invalid");
  }
}

void BinIntNode::setFeatureIndex(std::size_t index) {
  featureIndex_ = index;
}
