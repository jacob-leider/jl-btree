# jl-btree
b-trees! 

# Use
This is currently under development. There are several makefiles throughout that build a few different testing targets. There are also a collection of python bindings underway that will hopefully make testing easier.

The entire b-tree implementation lives in src/core. Everything else is for testing, debugging, etc.

# Purpose
This project exists mostly for my own educational benifit. For the most part, I have not, and plan not to read any information about B-trees or their implementations beyond this [wikipedia page](https://en.wikipedia.org/wiki/B-tree).


# Technical Details

## Insertion

### Topdown (lazy)

### Topdown (non-lazy)

### Bottom up

## Deletion

### Topdown (lazy)

### Topdown (non-lazy)

### Bottom up

# Contribution

## Things I'm Actively Working On:

- Support custom key types
  - [x] Consolidate key comparison logic to ease this transition
  - [x] Replace integer keys with data-slice structs
  - [ ] Support user-defined operations on keys
    - [ ] Comparators
    - [ ] String representation
    - [ ] Serializers (serializors?)
  - Create python bindings for custom key types
    - [ ] Create a key python object
  - [ ] Replace functionality that assumes data-slices as integers
    - [ ] Serialization/deserialization
- Support callers choosing between various insertion/deletion algorithms
  - Insertion
    - [x] Write stubs for each algorithm
    - [ ] Implement lazy topdown insertion
    - [ ] Implement nonlazy topdown insertion
    - [ ] Implement bottom-up insertion
  - Deletion
    - [ ] Write stubs for each algorithm
    - [x] Implement lazy topdown deletion
    - [ ] Implement nonlazy topdown deletion
    - [ ] Implement bottom-up deletion
-  Documentation
  - [ ] Write documentation for insertion algorithms
    - [ ] Lazy topdown
    - [ ] Non-lazy topdown
    - [ ] Bottom up
  - [ ] Write documentation for deletion algorithms
    - [ ] Lazy topdown
    - [ ] Non-lazy topdown
    - [ ] Bottom up

## Things I Plan to Implement:

- subtree-level locking scheme in order to provide a safe framework for writing multithreaded b-tree operations
- platform-independent paging scheme so b-trees can interact more directly with persistent memory
