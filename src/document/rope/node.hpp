//
// Copyright Will Roever 2016 - 2017.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <Unicode.h>

namespace proj
{
  using std::u16string;

  // A rope_node represents a string as a binary tree of string fragments
  //
  // A rope_node consists of:
  //   - a non-negative integer weight
  //   - a pointer to a left child rope_node
  //   - a pointer to a right child rope_node
  //   - a string fragment
  //
  // INVARIANTS:
  //   - a leaf is represented as a rope_node with null child pointers
  //   - a leaf node's weight is equal to the length of the string fragment it contains
  //   - an internal node is represented as a rope_node with non-null children and
  //     an empty string fragment
  //   - an internal node's weight is equal to the length of the string fragment
  //     contained in (the leaf nodes of) its left subtree

  class rope_node {

  public:
    using handle = std::unique_ptr<rope_node>;
    using linebreak = spurv::Linebreak;

    // CONSTRUCTORS
    // Construct internal node by concatenating the given nodes
    rope_node(handle l, handle r);
    // Construct leaf node from the given string
    rope_node(const u16string& str);
    // Construct leaf node from the given string
    rope_node(u16string&& str);
    // Copy constructor
    rope_node(const rope_node&);

    // ACCESSORS
    size_t getLength(void) const;
    char32_t getCharByIndex(size_t) const;
    // Get the substring of (len) chars beginning at index (start)
    u16string getSubstring(size_t start, size_t len) const;
    // Get string contained in current node and its children
    u16string treeToString(void) const;

    std::vector<linebreak> linebreaks() const;
    std::vector<linebreak> lastLinebreaks() const;

    // MUTATORS
    // Split the represented string at the specified index
    friend std::pair<handle, handle> splitAt(handle, size_t);

    // HELPERS
    // Functions used in balancing
    size_t getDepth(void) const;
    void getLeaves(std::vector<rope_node *>& v);

    // Determine whether a node is a leaf
    bool isLeaf(void) const;

  private:
    void rebuildLinebreaks();
    size_t retrieveLineBreaks(size_t adjust, std::vector<linebreak>& breaks, bool& hasEndCr) const;

  private:

    size_t weight_;
    handle left_;
    handle right_;
    u16string fragment_;
    std::vector<linebreak> linebreaks_;
    
  }; // class rope_node
  
} // namespace proj
