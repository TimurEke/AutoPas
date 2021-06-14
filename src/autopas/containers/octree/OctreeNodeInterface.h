/**
 * @file OctreeNodeInterface.h
 *
 * @author Johannes Spies
 * @date 15.04.2021
 */
#pragma once

#include <array>
#include <vector>

#include "autopas/cells/FullParticleCell.h"
#include "autopas/containers/octree/OctreeDirection.h"
#include "autopas/utils/inBox.h"

namespace autopas {
template <typename Particle>
class OctreeLeafNode;
template <typename Particle>
class OctreeInnerNode;

/**
 * The base class that provides the necessary function definitions that can be applied to an octree.
 *
 * @tparam Particle
 */
template <class Particle>
class OctreeNodeInterface {
 public:
  /**
   * Create an octree node interface by initializing the given fields.
   * @param boxMin The minimum coordinate of the enclosing box
   * @param boxMax The maximum coordinate of the enclosing box
   * @param parent A pointer to the parent of this node, may be nullptr for the root.
   */
  OctreeNodeInterface(std::array<double, 3> boxMin, std::array<double, 3> boxMax, OctreeNodeInterface<Particle> *parent)
      : _boxMin(boxMin), _boxMax(boxMax), _parent(parent) {}

  /** To make clang happy. */
  virtual ~OctreeNodeInterface() = default;

  /**
   * Insert a particle into the octree.
   * @param ref A pointer reference to the location at which a possible new child can point to
   * @param p The particle to insert
   */
  virtual void insert(std::unique_ptr<OctreeNodeInterface<Particle>> &ref, Particle p) = 0;

  /**
   * Put all particles that are below this node into the vector.
   * @param ps A reference to the vector that should contain the particles after the operation
   */
  virtual void appendAllParticles(std::vector<Particle> &ps) = 0;

  /**
   * Put the min/max corner coordinates of every leaf into the vector.
   * @param boxes A reference to the vector that should contain pairs of the min/max corner coordinates
   */
  virtual void appendAllLeafBoxes(std::vector<std::pair<std::array<double, 3>, std::array<double, 3>>> &boxes) = 0;

  /**
   * Put all leaves below this subtree into a given list.
   * @param leaves A reference to the vector that should contain pointers to the leaves
   */
  virtual void appendAllLeaves(std::vector<OctreeLeafNode<Particle> *> &leaves) = 0;

  /**
   * Delete the entire tree below this node.
   */
  virtual void clearChildren(std::unique_ptr<OctreeNodeInterface<Particle>> &ref) = 0;

  /**
   * @copydoc CellBasedParticleContainer::getNumParticles()
   */
  virtual unsigned int getNumParticles() = 0;

  /**
   * Get a child node of this node (if there are children) given a specific octant using the spacial structure of the
   * stored children.
   * @param O The octant
   * @return A pointer to a child node
   */
  virtual OctreeNodeInterface<Particle> *SON(Octant O) = 0;

  /**
   * Check if the node is a leaf or an inner node. The function exists for debugging.
   * @return true iff the node is a leaf, false otherwise.
   */
  virtual bool hasChildren() = 0;

  /**
   * Get a child by its index from the node.
   * @param index The index of the child. Must be between 0 and 7 inclusive.
   * @return A pointer to the child.
   */
  virtual OctreeNodeInterface<Particle> *getChild(int index) = 0;

#if 0
  virtual std::optional<OctreeNodeInterface<Particle> *> getGreaterParentAlongAxis(
      int axis, int dir, OctreeNodeInterface<Particle> *embedded) = 0;

  virtual std::vector<OctreeNodeInterface<Particle> *> findTouchingLeaves(int axis, int dir,
                                                                          OctreeNodeInterface<Particle> *embedded) = 0;

  bool isPositive(int dir) {
    if (dir == 0) {
      throw std::runtime_error("[OctreeNodeInterface.h] Unable to obtain direction from 0");
    }
    return dir > 0;
  }

  bool isNegative(int dir) {
    if (dir == 0) {
      throw std::runtime_error("[OctreeNodeInterface.h] Unable to obtain direction from 0");
    }
    return dir < 0;
  }

  static int oppositeDirection(int dir) {
    switch (dir) {
      case 1:
        return -1;
      case -1:
        return 1;
      default:
        throw std::runtime_error("[OctreeNodeInterface.h] dir must be -1 or 1.");
    }
  }

  std::vector<OctreeNodeInterface<Particle> *> getNeighborsAlongAxis(int axis, int dir) {
    std::vector<OctreeNodeInterface<Particle> *> result = {};
    if (auto greaterParentOptional = getGreaterParentAlongAxis(axis, dir, this)) {
      OctreeNodeInterface<Particle> *greaterParent = *greaterParentOptional;
      if (!greaterParent->hasChildren()) {
        throw std::runtime_error("[OctreeNodeInterface.h] The greater parent must not be a leaf if it exists.");
      }

      for (int i = 0; i < 8; ++i) {
        auto *child = greaterParent->getChild(i);
        if (enclosesVolumeWithOtherOnAxis(axis, child)) {
          int downDir = oppositeDirection(dir);
          result = child->findTouchingLeaves(axis, downDir, this);
          break;
        }
      }
    }
    return result;
  }

  std::vector<OctreeNodeInterface<Particle> *> getNeighbors() {
    std::vector<OctreeNodeInterface<Particle> *> result = {};
    for (int axis = 0; axis < 3; ++axis) {
      for (int dir = -1; dir <= 1; dir += 2) {
        auto neighborsAlongAxis = getNeighborsAlongAxis(axis, dir);
        result.insert(result.end(), neighborsAlongAxis.begin(), neighborsAlongAxis.end());
      }
    }
    return result;
  }
#endif

  /**
   * Check if a 3d point is inside the node's axis aligned bounding box. (Set by the boxMin and boxMax fields.)
   * @param point The node to test
   * @return true if the point is inside the node's bounding box and false otherwise
   */
  bool isInside(std::array<double, 3> point) {
    using namespace autopas::utils;
    return inBox(point, _boxMin, _boxMax);
  }

  /**
   * Check if the volume enclosed by two boxes a and b is nonzero on a specific axis.
   *
   * @param axis An axis index (0, 1 or 2)
   * @param aMin The minimum coordinate of a's volume
   * @param aMax The maximum coordinate of a's volume
   * @param bMin The minimum coordinate of b's volume
   * @param bMax The maximum coordinate of b's volume
   * @return true iff the enclosed volume is greater than zero, false if the enclosed volume is equal to zero.
   */
  static bool volumeExistsOnAxis(int axis, std::array<double, 3> aMin, std::array<double, 3> aMax,
                                 std::array<double, 3> bMin, std::array<double, 3> bMax) {
    bool o1 = aMin[axis] < bMax[axis];
    bool o2 = bMin[axis] < aMax[axis];
    return o1 && o2;
  }

  /**
   * Check if an octree node's box encloses volume with another octree node's box on a specific axis.
   *
   * @param axis An axis index (0, 1 or 2)
   * @param other The octree node to check against.
   * @return true iff the enclosed volume is greater than zero, false if the enclosed volume is equal to zero.
   */
  bool enclosesVolumeWithOtherOnAxis(int axis, OctreeNodeInterface<Particle> *other) {
    return volumeExistsOnAxis(axis, this->getBoxMin(), this->getBoxMax(), other->getBoxMin(), other->getBoxMax());
  }

  /**
   * Check if the node's axis aligned bounding box overlaps with the given axis aligned bounding box.
   * @param otherMin The minimum coordinate of the other box
   * @param otherMax The maximum coordinate of the other box
   * @return true iff the overlapping volume is non-negative
   */
  bool overlapsBox(std::array<double, 3> otherMin, std::array<double, 3> otherMax) {
    bool result = true;
    for (auto d = 0; d < 3; ++d) {
      result &= (this->_boxMin[d] <= otherMax[d]) && (this->_boxMax[d] >= otherMin[d]);
    }
    return result;
  }

  /**
   * Find a node (via the pointer structure) that is of equal size of the current node's bounding box, according to the
   * Samet paper. This function searches along a face.
   * @param I The face in which direction the search should find a node
   * @return An octree node
   */
  OctreeNodeInterface<Particle> *EQ_FACE_NEIGHBOR(Face I) {
    OctreeNodeInterface<Particle> *param, *P = this;
    if (ADJ(I, SONTYPE(P))) {
      param = FATHER(P)->EQ_FACE_NEIGHBOR(I);
    } else {
      param = FATHER(P);
    }
    return param->SON(REFLECT(I, SONTYPE(P)));
  }

  /**
   * Find a node (via the pointer structure) that is of equal size of the current node's bounding box, according to the
   * Samet paper. This function searches along an edge.
   * @param I The edge in which direction the search should find a node
   * @return An octree node
   */
  OctreeNodeInterface<Particle> *EQ_EDGE_NEIGHBOR(Edge I) {
    OctreeNodeInterface<Particle> *param, *P = this;
    if (ADJ(I, SONTYPE(P))) {
      param = FATHER(P)->EQ_EDGE_NEIGHBOR(I);
    } else if (COMMON_FACE(I, SONTYPE(P)) != O) {
      param = FATHER(P)->EQ_FACE_NEIGHBOR(COMMON_FACE(I, SONTYPE(P)));
    } else {
      param = FATHER(P);
    }
    return param->SON(REFLECT(I, SONTYPE(P)));
  }

  /**
   * Find a node (via the pointer structure) that is of equal size of the current node's bounding box, according to the
   * Samet paper. This function searches along a vertex.
   * @param I The face in which direction the search should find a node
   * @return An octree node
   */
  OctreeNodeInterface<Particle> *EQ_VERTEX_NEIGHBOR(Vertex I) {
    OctreeNodeInterface<Particle> *param, *P = this;
    if (ADJ(I, SONTYPE(P))) {
      param = FATHER(P)->EQ_VERTEX_NEIGHBOR(I);
    } else if (COMMON_EDGE(I, SONTYPE(P)) != OO) {
      param = FATHER(P)->EQ_EDGE_NEIGHBOR(COMMON_EDGE(I, SONTYPE(P)));
    } else if (COMMON_FACE(I, SONTYPE(P)) != O) {
      param = FATHER(P)->EQ_FACE_NEIGHBOR(COMMON_FACE(I, SONTYPE(P)));
    } else {
      param = FATHER(P);
    }
    return param->SON(REFLECT(I, SONTYPE(P)));
  }

  /**
   * Find a node (via the pointer structure) that is of greater than or equal to the size of the current node's bounding
   * box, according to the Samet paper. This function searches along a face.
   * @param I The face in which direction the search should find a node
   * @return An octree node
   */
  OctreeNodeInterface<Particle> *GTEQ_FACE_NEIGHBOR(Face I);

  /**
   * Find a node (via the pointer structure) that is of greater than or equal to the size of the current node's bounding
   * box, according to the Samet paper. This function searches along an edge.
   * @param I The edge in which direction the search should find a node
   * @return An octree node
   */
  OctreeNodeInterface<Particle> *GTEQ_EDGE_NEIGHBOR(Edge I);

  /**
   * Find a node (via the pointer structure) that is of greater than or equal to the size of the current node's bounding
   * box, according to the Samet paper. This function searches along a vertex.
   * @param I The vertex in which direction the search should find a node
   * @return An octree node
   */
  OctreeNodeInterface<Particle> *GTEQ_VERTEX_NEIGHBOR(Vertex I);

  /**
   * Find all leaf nodes along a list of given directions.
   * @param directions A list of allowed directions for traversal.
   * @return A list of leaf nodes
   */
  virtual std::vector<OctreeNodeInterface<Particle> *> getLeavesFromDirections(std::vector<Vertex> directions) = 0;

  /**
   * This function combines all required functions when traversing down a subtree of the octree and finding all leaves.
   * @param direction The "original" direction. The leaves will be found along the opposite direction.
   * @return A list of leaf nodes
   */
  std::vector<OctreeNodeInterface<Particle> *> getNeighborLeaves(Any direction) {
    auto opposite = getOppositeDirection(direction);
    auto directions = getAllowedDirections(opposite);
    auto neighborLeaves = getLeavesFromDirections(directions);
    return neighborLeaves;
  }

  std::vector<OctreeNodeInterface<Particle> *> getNeighborLeaves() {
    std::vector<OctreeNodeInterface<Particle> *> result;
    return result;
  }

  /**
   * Set the minimum coordinate of the enclosing box.
   * @param boxMin A point in 3D space
   */
  void setBoxMin(std::array<double, 3> boxMin) { _boxMin = boxMin; }

  /**
   * Set the maximum coordinate of the enclosing box.
   * @param boxMin A point in 3D space
   */
  void setBoxMax(std::array<double, 3> boxMax) { _boxMax = boxMax; }

  /**
   * Get the minimum coordinate of the enclosing box.
   * @return A point in 3D space
   */
  std::array<double, 3> getBoxMin() { return _boxMin; }

  /**
   * Get the maximum coordinate of the enclosing box.
   * @return A point in 3D space
   */
  std::array<double, 3> getBoxMax() { return _boxMax; }

  /**
   * Get the parent node of this node.
   * @return A pointer to the parent, can be nullptr.
   */
  OctreeNodeInterface<Particle> *getParent() { return _parent; }

 protected:
  /**
   * Check if this is not the root node.
   * @return true iff there does not exist a parent for this node.
   */
  bool hasParent() { return _parent != nullptr; }

  /**
   * A pointer to the parent node. Can be nullptr (iff this is the root node).
   */
  OctreeNodeInterface<Particle> *_parent;

  /**
   * The min coordinate of the enclosed volume.
   */
  std::array<double, 3> _boxMin;

  /**
   * The max coordinate of the enclosed volume.
   */
  std::array<double, 3> _boxMax;
};

/**
 * Check if a node is an inner node.
 * @tparam Particle The particle type used in this container
 * @param node A pointer to a node
 * @return true if the node has children, false otherwise.
 */
template <class Particle>
inline bool GRAY(OctreeNodeInterface<Particle> *node) {
  // According to Samet: "All non-leaf nodes are said to be GRAY"
  return node->hasChildren();
}

/**
 * Get the parent node of an arbitrary octree node.
 *
 * @tparam Particle
 * @param node
 * @return The parent of the given node if the node is not the root node, otherwise nullptr.
 */
template <class Particle>
inline OctreeNodeInterface<Particle> *FATHER(OctreeNodeInterface<Particle> *node) {
  return node->getParent();
}

/**
 * Get the octant in which a given node can be found in the parent.
 *
 * @tparam Particle
 * @param node The node to check
 * @return The octant in which the node is in the parent, OOO if the node either does not have a parent or could not be
 * found in the parent.
 */
template <class Particle>
static Octant SONTYPE(OctreeNodeInterface<Particle> *node) {
  Octant result = OOO;
  if (FATHER(node)) {
    for (Vertex *test = VERTICES(); *test != OOO; ++test) {
      if (FATHER(node)->SON(*test) == node) {
        result = *test;
        break;
      }
    }
    assert(result != OOO);
  }
  return result;
}

template <class Particle>
OctreeNodeInterface<Particle> *OctreeNodeInterface<Particle>::GTEQ_FACE_NEIGHBOR(Face I) {
  // Check precondition
  if (!contains(getFaces(), O, I)) {
    throw std::runtime_error("[OctreeNodeInterface.h] Received invalid face.");
  }

  auto null = [](OctreeNodeInterface<Particle> *T) { return T == nullptr; };

  // Find a common ancestor
  OctreeNodeInterface<Particle> *Q, *P = this;
  if ((not null(FATHER(P))) and ADJ(I, SONTYPE(P))) {
    Q = FATHER(P)->GTEQ_FACE_NEIGHBOR(I);
  } else {
    Q = FATHER(P);
  }

  if ((not null(Q)) and GRAY(Q)) {
    // Follow the reflected path to locate the neighbor
    return Q->SON(REFLECT(I, SONTYPE(P)));
  } else {
    return Q;
  }
}

template <class Particle>
OctreeNodeInterface<Particle> *OctreeNodeInterface<Particle>::GTEQ_EDGE_NEIGHBOR(Edge I) {
  // Check precondition
  if (!contains(getEdges(), OO, I)) {
    throw std::runtime_error("[OctreeNodeInterface.h] Received invalid edge.");
  }

  auto null = [](OctreeNodeInterface<Particle> *T) { return T == nullptr; };

  // Find a common ancestor
  OctreeNodeInterface<Particle> *Q, *P = this;
  if (null(FATHER(P))) {
    Q = nullptr;
  } else if (ADJ(I, SONTYPE(P))) {
    Q = FATHER(P)->GTEQ_EDGE_NEIGHBOR(I);
  } else if (Face common = COMMON_FACE(I, SONTYPE(P)); common != O) {
    Q = FATHER(P)->GTEQ_FACE_NEIGHBOR(common);
  } else {
    Q = FATHER(P);
  }

  if ((not null(Q)) and GRAY(Q)) {
    // Follow opposite path to locate the neighbor
    return Q->SON(REFLECT(I, SONTYPE(P)));
  } else {
    return Q;
  }
}

template <class Particle>
OctreeNodeInterface<Particle> *OctreeNodeInterface<Particle>::GTEQ_VERTEX_NEIGHBOR(Vertex I) {
  // Check precondition
  if (!contains(VERTICES(), OOO, I)) {
    throw std::runtime_error("[OctreeNodeInterface.h] Received invalid vertex.");
  }

  auto null = [](OctreeNodeInterface<Particle> *T) { return T == nullptr; };

  // Find a common ancestor
  OctreeNodeInterface<Particle> *Q, *P = this;
  if (null(FATHER(P))) {
    Q = nullptr;
  } else if (ADJ(I, SONTYPE(P))) {
    Q = FATHER(P)->GTEQ_VERTEX_NEIGHBOR(I);
  } else if (Edge commonEdge = COMMON_EDGE(I, SONTYPE(P)); commonEdge != OO) {
    Q = FATHER(P)->GTEQ_EDGE_NEIGHBOR(commonEdge);
  } else if (Face commonFace = COMMON_FACE(I, SONTYPE(P)); commonFace != O) {
    Q = FATHER(P)->GTEQ_FACE_NEIGHBOR(commonFace);
  } else {
    Q = FATHER(P);
  }

  if ((not null(Q)) and GRAY(Q)) {
    // Follow opposite path to locate the neighbor
    return Q->SON(REFLECT(I, SONTYPE(P)));
  } else {
    return Q;
  }
}
}  // namespace autopas