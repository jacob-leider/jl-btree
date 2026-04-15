# jl-btree
b-trees! 

# Use
This is currently under development. There are several makefiles throughout that build a few different testing targets. There are also a collection of python bindings underway that will hopefully make testing easier.

# Purpose
This project exists mostly for my own educational benifit. For the most part, I have not, and plan not to read any information about B-trees or their implementations beyond this [wikipedia page](https://en.wikipedia.org/wiki/B-tree).

# Contribution
Things I'm Actively Working On:
- Allowing for custom key types
- Letting callers choose between various insertion/deletion algorithms
- Efficient b-tree serialization/deserialization

Things I Plan to Implement:
- subtree-level locking scheme in order to provide a safe framework for writing multithreaded b-tree operations
- platform-independent paging scheme so b-trees can interact more directly with persistent memory
