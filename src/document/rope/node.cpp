//
// Copyright Will Roever 2016 - 2017.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#include "node.hpp"
#include <limits>

namespace proj
{
  using std::make_unique;
  using std::pair;
  using handle = rope_node::handle;
  
  // Define out-of-bounds error constant
  std::invalid_argument ERROR_OOB_NODE = std::invalid_argument("Error: string index out of bounds");
  
  // Construct internal node by concatenating the given nodes
  rope_node::rope_node(handle l, handle r)
  {
    this->left_ = std::move(l);
    this->right_ = std::move(r);
    this->weight_ = this->left_->getLength();
  }

  // Construct leaf node from the given string
  rope_node::rope_node(const std::u32string& str)
    : weight_(str.length()), left_(nullptr), right_(nullptr), fragment_(str)
  {
      rebuildLinebreaks();
  }
  
  // Construct leaf node from the given string
  rope_node::rope_node(std::u32string&& str)
    : weight_(str.length()), left_(nullptr), right_(nullptr), fragment_(std::move(str))
  {
      rebuildLinebreaks();
  }

  // Copy constructor
  rope_node::rope_node(const rope_node& aNode)
   : weight_(aNode.weight_), fragment_(aNode.fragment_)
  {
    rope_node * tmpLeft = aNode.left_.get();
    rope_node * tmpRight = aNode.right_.get();
    if(tmpLeft == nullptr) this->left_ = nullptr;
    else this->left_ = make_unique<rope_node>(*tmpLeft);
    if(tmpRight == nullptr) this->right_ = nullptr;
    else this->right_ = make_unique<rope_node>(*tmpRight);
    if (isLeaf()) {
      rebuildLinebreaks();
    }
  }
  
  // Determine whether a node is a leaf
  bool rope_node::isLeaf(void) const {
    return this->left_ == nullptr && this->right_ == nullptr;
  }
  
  // Get string length by adding the weight of the root and all nodes in
  //   path to rightmost child
  size_t rope_node::getLength() const {
    if(this->isLeaf())
      return this->weight_;
    size_t tmp = (this->right_ == nullptr) ? 0 : this->right_->getLength();
    return this->weight_ + tmp;
  }
  
  // Get the character at the given index
  char32_t rope_node::getCharByIndex(size_t index) const {
    size_t w = this->weight_;
    // if node is a leaf, return the character at the specified index
    if (this->isLeaf()) {
      if (index >= this->weight_) {
        throw ERROR_OOB_NODE;
      } else {
        return this->fragment_[index];
      }
    // else search the appropriate child node
    } else {
      if (index < w) {
        return this->left_->getCharByIndex(index);
      } else {
        return this->right_->getCharByIndex(index - w);
      }
    }
  }

  // Get the substring of (len) chars beginning at index (start)
  u32string rope_node::getSubstring(size_t start, size_t len) const {
    size_t w = this->weight_;
    if (this->isLeaf()) {
      if(len < w) {
        return this->fragment_.substr(start,len);
      } else {
        return this->fragment_;
      }
    } else {
      // check if start index in left subtree
      if (start < w) {
        u32string lResult = (this->left_ == nullptr) ? u32string {} : this->left_->getSubstring(start,len);
        if ((start + len) > w) {
          // get number of characters in left subtree
          size_t tmp =  w - start;
          u32string rResult = (this->right_ == nullptr) ? u32string {} : this->right_->getSubstring(w,len-tmp);
          return lResult.append(std::move(rResult));
        } else {
          return lResult;
        }
      // if start index is in the right subtree...
      } else {
        return (this->right_ == nullptr) ? u32string {} : this->right_->getSubstring(start-w,len);
      }
    }
  }
  
  // Get string contained in current node and its children
  u32string rope_node::treeToString(void) const {
    if(this->isLeaf()) {
      return this->fragment_;
    }
    u32string lResult = (this->left_ == nullptr) ? u32string {} : this->left_->treeToString();
    u32string rResult = (this->right_ == nullptr) ? u32string {} : this->right_->treeToString();
    return lResult.append(std::move(rResult));
  }

  std::vector<rope_node::linebreak> rope_node::linebreaks() const {
    std::vector<linebreak> breaks;
    bool hasEndCr = false;
    retrieveLineBreaks(0, breaks, hasEndCr);
    if (hasEndCr) {
      // need to recheck if there are any CR+LF sequences
      auto it = breaks.begin();
      while (it != breaks.end()) {
        if (it->second == 0x000A && it != breaks.begin()) {
          if ((it - 1)->second == 0x000D) {
            // remove prev, the ++it below will skip current
            it = breaks.erase(it - 1);
          }
        }
        ++it;
      }
    }
    return breaks;
  }

  std::vector<rope_node::linebreak> rope_node::lastLinebreaks() const {
    if(this->isLeaf()) {
      return this->linebreaks_;
    }
    if (this->right_ != nullptr) {
      return this->right_->lastLinebreaks();
    }
    return {};
  }

  size_t rope_node::retrieveLineBreaks(size_t adjust, std::vector<linebreak>& breaks, bool& hasEndCr) const {
    if(this->isLeaf()) {
      if(!this->linebreaks_.empty()) {
        const auto end = breaks.size();
        breaks.resize(end + this->linebreaks_.size());
        std::transform(this->linebreaks_.cbegin(), this->linebreaks_.cend(), breaks.begin() + end, [adjust](auto b) {
          return std::make_pair(b.first + adjust, b.second);
        });
        if (!hasEndCr && breaks.back().second == 0x000D) {
          hasEndCr = true;
        }
      }
      return adjust + this->fragment_.size();
    }
    if (this->left_ != nullptr) {
      adjust += this->left_->retrieveLineBreaks(adjust, breaks, hasEndCr);
    }
    if (this->right_ != nullptr) {
      adjust += this->right_->retrieveLineBreaks(adjust, breaks, hasEndCr);
    }
    return adjust;
  }

  // Split the represented string at the specified index
  pair<handle, handle> splitAt(handle node, size_t index)
  {
    size_t w = node->weight_;
    // if the given node is a leaf, split the leaf
    if(node->isLeaf()) {
      return pair<handle,handle>{
        make_unique<rope_node>(node->fragment_.substr(0,index)),
        make_unique<rope_node>(node->fragment_.substr(index,w-index))
      };
    }

    // if the given node is a concat (internal) node, compare index to weight and handle
    //   accordingly
    handle oldRight = std::move(node->right_);
    if (index < w) {
      node->right_ = nullptr;
      node->weight_ = index;
      std::pair<handle, handle> splitLeftResult = splitAt(std::move(node->left_), index);
      node->left_ = std::move(splitLeftResult.first);
      return pair<handle,handle>{
        std::move(node),
        make_unique<rope_node>(std::move(splitLeftResult.second), std::move(oldRight))
      };
    } else if (w < index) {
      pair<handle, handle> splitRightResult = splitAt(std::move(oldRight),index-w);
      node->right_ = std::move(splitRightResult.first);
      return pair<handle,handle>{
        std::move(node),
        std::move(splitRightResult.second)
      };
    } else {
      return pair<handle,handle>{
        std::move(node->left_), std::move(oldRight)
      };
    }
  }

  // Get the maximum depth of the rope, where the depth of a leaf is 0 and the
  //   depth of an internal node is 1 plus the max depth of its children
  size_t rope_node::getDepth(void) const {
    if(this->isLeaf()) return 0;
    size_t lResult = (this->left_ == nullptr) ? 0 : this->left_->getDepth();
    size_t rResult = (this->right_ == nullptr) ? 0 : this->right_->getDepth();
    return std::max(++lResult,++rResult);
  }

  // Store all leaves in the given vector
  void rope_node::getLeaves(std::vector<rope_node *>& v) {
    if(this->isLeaf()) {
      v.push_back(this);
    } else {
      rope_node * tmpLeft = this->left_.get();
      if (tmpLeft != nullptr) tmpLeft->getLeaves(v);
      rope_node * tmpRight = this->right_.get();
      if (tmpRight != nullptr) tmpRight->getLeaves(v);
    }
  }

  void rope_node::rebuildLinebreaks() {
    this->linebreaks_.clear();
    if (!this->isLeaf()) {
      return;
    }
    size_t lastCr = std::numeric_limits<size_t>::max();
    for (size_t idx = 0; idx < this->weight_; ++idx) {
      const auto ch = this->fragment_[idx];
      if (spurv::isLineBreak(ch)) {
        switch (ch) {
        case 0x000D:
          this->linebreaks_.push_back(std::make_pair(idx, ch));
          lastCr = idx;
          break;
        case 0x000A:
          if (idx > 0 && idx - 1 == lastCr) {
            this->linebreaks_.back().first += 1;
          } else {
            this->linebreaks_.push_back(std::make_pair(idx, ch));
          }
          break;
        default:
          this->linebreaks_.push_back(std::make_pair(idx, ch));
          break;
        }
      }
    }
  }
  
} // namespace proj
