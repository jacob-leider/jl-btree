# jl-btree
b-trees! 

# Use
This is currently under development. There are several makefiles throughout that build a few different testing targets. There are also a collection of python bindings underway that will hopefully make testing easier.

The entire b-tree implementation lives in src/core. Everything else is for testing, debugging, etc.

# Purpose
This project exists mostly for my own educational benifit. For the most part, I have not, and plan not to read any information about B-trees or their implementations beyond this [wikipedia page](https://en.wikipedia.org/wiki/B-tree).


# Technical Details

## Insertion

## Deletion

# Contribution
Things I'm Actively Working On:
- Support custom key types
  - [x] Consolidate key comparison logic to ease this transition
  - [x] Replace integer keys with data-slice structs
  - [ ] Support user-defined operations on keys
    - [ ] comparators
    - [ ] string representation
    - [ ] serializers (serializors?)
  - [ ] Create python bindings for custom key types
  - [ ] Replace functionality that assumes data-slices as integers
    - [ ] Serialization/deserialization
- Support callers choosing between various insertion/deletion algorithms
  - [x] lazy top-down insertion/deletion
  - [ ] lazy bottom-up insertion/deletion
  - [ ] non-lazy top-down insertion/deletion (split/merge nodes that are full/at min capacity)

Things I Plan to Implement:
- subtree-level locking scheme in order to provide a safe framework for writing multithreaded b-tree operations
- platform-independent paging scheme so b-trees can interact more directly with persistent memory
