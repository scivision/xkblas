# RegionAccessTree

## The problem
A region is a K-dimension hyper-rectangle.

An access mode is whether *write* or *read*.

An access is a couple *(R, M)* with *R* a region and *M* an access mode.

Given a sequence of accesses *(Xi)* with *Xi = (Ri, Mi)* - we define the *dependence graph G(X)* so that
- Xi are nodes
- there exists a directed path between Xi and Xj if and only if
  - i < j
  - Ri and Rj intersects
  - Mi or Mj is *write*

## Remarks
Extending the problem to support *concurent write* is straightforward, adding more condition to path existence.

In practice, the same task would perform multiple accesses - we can build a *task dependence graph* from an *access dependence graph* by merging nodes of the same task and removing their edges.

## The repo
The `src/impl` folder contains various implementations of the `src/history.hpp` interfaces, that can solve the problem
