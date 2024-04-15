//
// Copyright Will Roever 2016 - 2017.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include <algorithm>
#include <string>
#include "node.hpp"

namespace proj
{
  using std::u32string;

  // A rope represents a string as a binary tree wherein the leaves contain fragments of the
  //   string. More accurately, a rope consists of a pointer to a root rope_node, which
  //   describes a binary tree of string fragments.
  
  // Examples:
  //
  //        X        |  null root pointer, represents empty string
  //
  //      "txt"      |
  //     /     \     |  root is a leaf node containing a string fragment
  //    X       X    |
  //
  //        O        |
  //       / \       |
  //  "some" "text"  |  root is an internal node formed by the concatenation of two distinct
  //    /\     /\    |  ropes containing the strings "some" and "text"
  //   X  X   X  X   |
  
  class rope {
    
  public:
    
    using node_handle = std::unique_ptr<rope_node>;
    
    // CONSTRUCTORS
    // Default constructor - produces a rope representing the empty string
    rope(void);
    // Construct a rope from the given string
    rope(const u32string&);
    // Construct a rope from the given string
    rope(u32string&&);
    // Copy constructor
    rope(const rope&);
    // Move constructor
    rope(rope&&);
    // Move construct a rope from a node
    rope(node_handle&&);

    // Get the string stored in the rope
    u32string toString(void) const;
    // Get the length of the stored string
    size_t length(void) const;
    // Get the character at the given position in the represented string
    char32_t at(size_t index) const;
    // Return the substring of length (len) beginning at the specified index
    u32string substring(size_t start, size_t len) const;
    // Determine if rope is balanced
    bool isBalanced(void) const;
    // Balance the rope
    void balance(void);
    
    // MUTATORS
    // Insert the given string/rope/node into the rope, beginning at the specified index (i)
    void insert(size_t i, const u32string& str);
    void insert(size_t i, const rope& r);
    void insert(size_t i, node_handle&& r);
    // Concatenate the existing string/rope/node with the argument
    void append(const u32string&);
    void append(const rope&);
    void append(node_handle&&);
    // Remove the substring of (len) characters beginning at index (start)
    node_handle remove(size_t start, size_t len);
    
    // OPERATORS
    rope& operator=(const rope& rhs);
    rope& operator=(rope&& rhs);
    rope& operator=(node_handle&& rhs);
    bool operator==(const rope& rhs) const;
    bool operator!=(const rope& rhs) const;

  private:
    
    // Pointer to the root of the rope tree
    node_handle root_;
    
  }; // class rope
  
  size_t fib(size_t n);
  std::vector<size_t> buildFibList(size_t len);
  
} // namespace proj
