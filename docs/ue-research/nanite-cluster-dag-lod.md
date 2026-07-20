# Nanite Cluster DAG + LOD Hierarchy (Phase 6)

UE5 source: `Developer/NaniteBuilder/Private/ClusterDAG.cpp`, `Shaders/Private/Nanite/NaniteHierarchyTraversal.ush`, `NaniteHierarchyTraversalCommon.ush`, `Shared/NaniteDefinitions.h`, `Runtime/Engine/Public/Rendering/NaniteResources.h`

## 1. Architecture Overview

Nanite represents LOD as a **Directed Acyclic Graph (DAG)** rather than a tree. The DAG is a BVH with fanout 4 where interior nodes represent merged (simplified) geometry and leaves represent full-detail clusters. The DAG is built bottom-up from leaf clusters (128 triangles each), through successive levels of simplification and merging.

On the GPU, a **persistent-thread BVH traversal** walks the hierarchy, testing nodes against frustum and HZB, and emits visible clusters into a candidate list for rasterization.

Key constants (from `NaniteDefinitions.h`):
- `NANITE_MAX_BVH_NODE_FANOUT = 4` (quad-tree BVH, `NANITE_MAX_BVH_NODE_FANOUT_BITS = 2`)
- `NANITE_HIERARCHY_NODE_SLICE_SIZE_DWORDS = 56` (per-node GPU memory)
- `NANITE_MAX_BVH_NODES_PER_GROUP = 16` (64 threads / 4 children)
- `NANITE_MAX_CLUSTER_HIERARCHY_DEPTH = 32` (max DAG depth)
- `NANITE_MAX_CLUSTERS_PER_GROUP_TARGET = 128` (grouping target)
- `NANITE_MAX_CLUSTERS_PER_PAGE_MASK` — clusters per streaming page
- `NANITE_PERSISTENT_CLUSTER_CULLING_GROUP_SIZE = 64`

## 2. Key Data Structures

### 2.1 FPackedHierarchyNode (GPU-side)

```cpp
struct FPackedHierarchyNode
{
    FVector4f LODBounds[NANITE_MAX_BVH_NODE_FANOUT];  // [4] sphere: xyz=center, w=radius

    struct { // Misc0[4]
        FVector3f BoxBoundsCenter;                    // AABB center for tighter box test
        uint32    MinLODError_MaxParentLODError;      // Packed: 16 bits min LOD error, 16 bits max parent LOD error
    } Misc0[4];

    struct { // Misc1[4]
        FVector3f BoxBoundsExtent;                    // AABB half-extent
        uint32    ChildStartReference;                // Page-local cluster index OR child node index
    } Misc1[4];

    struct { // Misc2[4]
        uint32    ResourcePageRangeKey;               // For streaming: which pages this node references
        uint32    GroupPartSize_AssemblyPartIndex;    // Packed group info
    } Misc2[4];
};
// Total: 4 * (4 + 4 + 4 + 2) = 56 DWORDs = 224 bytes per node
```

**Field semantics per child slot:**
- `LODBounds`: bounding sphere in world space, `w` component = radius. Used for fast sphere-plane frustum test. The sphere includes simplification error, so it's a **conservative** bound.
- `BoxBoundsCenter+Extent`: AABB for tighter occlusion and distance tests, computed from the original (unsimplified) vertex positions.
- `MinLODError_MaxParentLODError`: packed `float16` pair. MinLODError is the simplification error of this child. MaxParentLODError is the error inherited from ancestors (for monotonic nesting). The effective LOD error used for screen-size selection is `max(MinLODError, MaxParentLODError)`.
- `ChildStartReference`: if **leaf**: contains the page-local cluster index (offset within the GPU page). If **interior**: contains the node index for the next level.
- `ResourcePageRangeKey`: indices into the streaming page range lookup table. Tells the GPU which pages need to be resident for this child's clusters.
- `GroupPartSize_AssemblyPartIndex`: for multi-part assemblies, identifies which assembly transform applies.

### 2.2 FClusterGroup (CPU Build)

```cpp
struct FClusterGroup {
    TArray<FClusterRef> Children;    // Child clusters/nodes in this group
    FSphere3f Bounds;               // Tight bounding sphere of all children
    FSphere3f LODBounds;            // Conservative LOD sphere (includes parent error)
    float     ParentLODError;       // Maximum child LODError (monotonic)
    int32     MipLevel;             // DAG depth level
    uint32    MeshIndex;            // Owning mesh index
    bool      bRoot;                // Is this the root group?
};
```

Groups typically contain 8-32 children (target 128 at leaf level) and are the unit of parallel reduction.

### 2.3 FQueueState (GPU Traversal State)

```cpp
struct FQueuePassState {
    uint ClusterBatchReadOffset;   // Batch-granularity read pointer for cluster queue
    uint ClusterWriteOffset;       // Individual cluster write pointer
    uint NodeReadOffset;           // DWORD-granularity node read pointer
    uint NodeWriteOffset;          // DWORD-granularity node write pointer
    int  NodeCount;                // Conservative count of remaining nodes (can be transiently > actual)
};

struct FQueueState {
    uint            TotalClusters;
    uint            AssemblyTransformsWriteOffset;
    FQueuePassState PassState[2];  // Double-buffered: one for current pass, one for next
};
```

## 3. DAG Construction Algorithm (CPU)

### 3.1 Entry Point: ReduceMesh

`FClusterDAG::ReduceMesh()` builds the DAG bottom-up in a loop:

```
while (LevelClusters.Num() > 1)
{
    // 1. Separate triangle clusters from voxel clusters
    TriClusters   = partition(LevelClusters, NumTris > 0)
    VoxelClusters = partition(LevelClusters, NumTris == 0)

    // 2. Group spatially-coherent clusters together
    GroupTriangleClusters(TriClusters)   // Graph partitioning
    GroupVoxelClusters(VoxelClusters)    // Morton sort + merge

    // 3. Reduce each group to parent clusters
    for each Group:
        ReduceGroup(Group)  // Simplify or Voxelize, then split

    // 4. Sort parents by GUID for determinism
    LevelClusters = sorted parent clusters
}
```

**Termination conditions:**
- Single cluster remaining (root)
- Single voxel cluster with `MaterialIndexes.Num() <= 32` (rare)
- Single triangle cluster with data fully on root pages (no streaming needed)

### 3.2 GroupTriangleClusters

Groups triangle clusters using graph partitioning:

1. **Find adjacent clusters** (`FindAdjacentClusters`): For each cluster's external edges, look up the reverse edge direction in a concurrent hash table of all external edges. This builds a bidirectional adjacency graph where edge weight = number of shared edges.

2. **Build disjoint sets**: Connect clusters sharing edges via union-find.

3. **Build locality links**: Additional spatial links added via the graph partitioner for non-adjacent but nearby clusters (using cluster bounding sphere centers).

4. **Partition the graph**: `FGraphPartitioner.PartitionStrict()` produces balanced groups of 8-32 clusters each. Edge weights are `NumSharedEdges * (bSiblings ? 12 : 16) + 4` where "sibling" means same GeneratingGroupIndex (clusters from the same parent group are weighted higher for grouping).

### 3.3 GroupVoxelClusters

Voxel clusters use a different grouping strategy since they don't have mesh adjacency:

1. **Sort by GeneratingGroupIndex, then InstanceIndex** — keeps clusters from the same source together.

2. **Split into groups of up to 128**: Each run of same (InstanceIndex, GroupIndex) forms a group.

3. **Morton-code spatial sort**: Compute 30-bit Morton code from normalized center position within `TotalBounds`.

4. **Iterative nearest-neighbor merge**: Merge adjacent groups (search radius = 16 in sorted order) based on bounding sphere radius cost. Groups smaller than MinGroupSize preferentially merge with neighbors.

### 3.4 ReduceGroup

This is the core reduction pass. For each group:

**Step 1: Compute group bounds.** Accumulate child bounds (AABB, bounding sphere, LOD bounds). For instanced children, transform bounds by the instance matrix. Track `ParentLODError = max(child.LODError)` for monotonic nesting.

**Step 2: Attempt simplification.** If all children are triangle clusters:
- `Merged = FCluster(*this, Group.Children)` — merges all children into a single oversized cluster
- `Merged.Simplify(TargetNumTris)` — QEM simplification to `NumParents * TargetClusterSize` triangles
- Returns `SimplifyError`

**Step 3: Attempt voxelization.** If voxels are allowed (`!bAllTriangles || ShapePreservation == Voxelize`):
- Compute `VoxelSize = sqrt(GroupArea / TargetNumVoxels) * 0.75`
- Clamp `VoxelSize >= Group.ParentLODError` (voxels must be larger than simplification error)
- Iteratively increase voxel size by 1.1x until voxel count fits target
- `Voxelize()` ray-traces the child geometry into sparse voxels

**Step 4: Choose the better representation.** If `VoxelSize < SimplifyError`, use voxels; otherwise use simplified triangles.

**Step 5: Split oversized result.** `SplitCluster()` partitions the merged/simplified/voxelized cluster into `NumParents` sub-clusters of at most 128 triangles each. If splitting fails (graph partitioner produces too many parts), retry with a smaller target cluster size (`TargetClusterSize -= 2`).

**Step 6: Assign LOD data.** All sibling parent clusters from the same group share the same `LODBounds`, `LODError`, and `GeneratingGroupIndex`. This ensures they all switch LOD at the same screen size.

### 3.5 FindCut (Streaming LOD Selection)

Selects which DAG nodes to render for a given triangle/error budget:

```
Heap<FClusterRef> selected = {root children}
while (true):
    worst = HeapTop  // highest LODError
    if worst.MipLevel == 0: break  // already at leaves
    if NumTris > TargetNumTris && MinError < TargetError: break  // budget satisfied
    HeapPop(worst)
    NumTris -= worst.NumTris
    HeapPush(worst's children)  // replace with finer-LOD children
    NumTris += sum(child.NumTris)
```

This is a greedy cut-finding algorithm: replacing a coarse node with its children reduces error at the cost of more triangles.

## 4. GPU Hierarchy Traversal

### 4.1 Persistent Thread Architecture

The traversal shader (`NaniteHierarchyTraversal.ush`) implements three modes controlled by the compile-time define `NANITE_HIERARCHY_TRAVERSAL_TYPE`:

| Mode | Value | Description |
|------|-------|-------------|
| `NANITE_CULLING_TYPE_NODES` | 0 | Separate node-only passes per level |
| `NANITE_CULLING_TYPE_CLUSTERS` | 1 | Separate cluster-only pass |
| `NANITE_CULLING_TYPE_PERSISTENT_NODES_AND_CLUSTERS` | 2 | Combined persistent-thread traversal |

Mode 2 is the primary path. Persistent threads run until the job queue is empty, dynamically switching between processing BVH nodes and culling clusters.

**Design rationale** (from comments in the shader): Mapping tree traversal to GPU is awkward because the number of leaf nodes is dynamic (zero to hundreds of thousands). Persistent threads with a job queue avoid the underutilization of 1:1 thread-to-leaf mapping and the serial bottleneck of 1:1 thread-to-tree mapping. Workers favor node processing to keep the critical path progressing, but fill idle time by processing clusters.

### 4.2 Persistent Node and Cluster Cull

```pseudocode
PersistentNodeAndClusterCull(GroupIndex):
    bProcessNodes = true
    while (true):
        // Phase 1: Try to grab and process BVH nodes
        if bProcessNodes:
            if no current node batch:
                atomically claim next batch of NANITE_MAX_BVH_NODES_PER_GROUP nodes
            check which nodes in batch are "ready" (fully written by producers)
            if nodes ready:
                ProcessNodeBatch(batchSize, GroupIndex)
                    for each node's 4 children:
                        if child visible (frustum/HZB test):
                            if interior: atomically write child node to output queue
                            if leaf: atomically write clusters to output queue,
                                     update cluster batch counters
                continue  // loop back, prioritize nodes

        // Phase 2: No nodes ready — process clusters instead
        if no current cluster batch:
            atomically claim next cluster batch index
        if cluster batch has work:
            ProcessClusterBatch(batchIndex, batchSize, GroupIndex)
                cull each cluster (frustum/HZB test)
                bin visible clusters for SW/HW rasterization based on screen size

        // Check termination
        if all nodes processed AND no more cluster batches:
            break
```

### 4.3 Node Processing

`ProcessNodeBatch()` processes a batch of BVH nodes:

1. Each thread handles one child slot (fanout=4, so 16-node batch needs 64 threads)
2. `GetHierarchyNodeSlice()` decodes the packed node data from the hierarchy buffer
3. `ShouldVisitChild()` performs visibility tests (frustum sphere-plane, HZB occlusion)
4. If visible AND interior: wave-interlocked increment on `GroupNumCandidateNodes`, atomically append to node output queue
5. If visible AND leaf: atomically append cluster indices to cluster output queue, update cluster batch counters for subsequent cluster culling

Key observation: **Node pass and cluster pass share the same persistent threads**, eliminating dispatch overhead and idle threads.

### 4.4 Cluster Processing

`ProcessClusterBatch()` processes already-visible clusters:

1. Load `PackedCluster` data (8 float4s = 128 bytes per cluster)
2. Decode cluster bounds, LOD error
3. Perform additional culling tests (e.g., distance-based LOD selection)
4. Bin visible clusters: small clusters -> SW rasterization; large clusters -> HW rasterization
5. Clear the batch slot for reuse by the next pass

### 4.5 Double-Buffered Queue

The `FQueueState` uses two `FQueuePassState` slots (PassState[0] and PassState[1]) for double-buffered traversal across two GPU passes:

- **Pass 0**: Instance culling produces initial nodes -> feeds Pass 1
- **Pass 1**: Hierarchy traversal -> produces clusters for rasterization

The `ClusterBatchReadOffset` and `NodeReadOffset` track consumption progress; `ClusterWriteOffset` and `NodeWriteOffset` track production. `NodeCount` is decremented when a node batch is fully processed (conservative counting prevents premature termination).

### 4.6 Group Shared Memory Management

Two levels of work distribution:
- **Across workgroups**: Nodes and cluster batches are distributed via global atomic counters
- **Within a workgroup**: `groupshared` arrays (`GroupNodeData`) hold the current node batch; atomic wave operations synchronize. After a node batch, the group shares results through `QueueState` buffers.

## 5. LOD Error Metric

The LOD error is a **world-space distance** representing the maximum geometric deviation caused by simplification. It is used to decide when to switch LOD levels:

- **Cluster.LODError**: worst-case distance from any simplified vertex to the original surface
- **Cluster.LODBounds**: bounding sphere expanded by LODError (conservative: encloses the original surface)
- **Monotonic nesting**: `ParentLODError >= max(child.LODError)` for all children; enforced at each reduction step

On GPU, the screen-space projected radius of `LODBounds` is compared against a threshold (typically 1 pixel). If the projected error is below the threshold, the current node is sufficient; otherwise, children are traversed.

The `FindCut` algorithm on CPU pre-selects which LOD levels to include in streaming pages, ensuring that even without full data residency, a valid cut of the DAG is always available.

## 6. Design Decisions / Tradeoffs

### 6.1 DAG vs Tree
- **DAG** allows a child cluster to have multiple parents (shared subtrees), reducing memory. In practice, children from different groups can contribute to a single parent merge, but true DAG sharing of complete subtrees is limited.

### 6.2 Quad-Fanout BVH
- Fanout=4 balances traversal cost vs. memory. Larger fanout would reduce tree depth but increase node size (56 DWORDs already substantial).
- Each child is an independent visibility test target, enabling SIMD-style evaluation across the 4 children.

### 6.3 Persistent Threads
- Avoids multiple dispatch overhead of per-level compute passes
- Workers dynamically balance node vs cluster work
- Requires careful double-buffering and conservative counting to avoid deadlocks

### 6.4 Conservative NodeCount
- `NodeCount` is decremented only when an entire batch finishes, not per-node
- This means the count is transiently higher than actual remaining nodes
- Threads may spin briefly waiting for the count to catch up, but this is bounded by batch size

### 6.5 Triangle vs Voxel Duality
- Both triangle simplification and voxelization coexist in the same DAG
- At each level, the algorithm picks whichever gives lower error
- Voxel levels dominate at coarse LODs; triangle simplification at fine LODs

## 7. What Tumbler Should Adopt vs Simplify

### Phase 4A (Immediate):
- **Skip the DAG entirely** — Phase 4 only has leaf clusters (LOD 0), no hierarchy needed
- **Store bounds per cluster** (AABB + bounding sphere + LOD error = 0)
- **Simple linear cluster list** — no BVH traversal, iterate all clusters

### Phase 6 (LOD Hierarchy):
- **Adopt quad-fanout BVH** (fanout=4, `FPackedHierarchyNode` layout)
- **Adopt bottom-up merge + simplify** pipeline, but simplify:
  - Use a simpler grouping strategy: Morton sort clusters by center, merge adjacent groups greedily
  - Skip voxelization initially (support only triangle simplification)
  - Skip the full Metis-like graph partitioner for grouping; Morton-sort-based grouping is adequate
- **Adopt the persistent-thread traversal model** — it's the right architecture for GPU tree traversal
- **Adopt the double-buffered queue** pattern for multi-pass traversal
- **Adopt LOD error monotonic nesting** for correct screen-size selection

### Phase 6 (Simplify):
- **Skip voxel representation** until distant geometry is a bottleneck
- **Skip assembly parts / instancing** in the DAG — single static mesh only
- **Skip the overshoot mechanism in FindCut** — simpler single-pass cut selection
- **Use a fixed-DAG-depth BVH** (e.g., 4 levels deep instead of adaptive) for simpler GPU traversal
- **Single-pass traversal** instead of persistent threads initially: one workgroup per BVH root, recursive descent in LDS

### Key simplifications for Phase 6:
1. Morton-sort clusters, build BVH with fanout=4 recursively
2. Merge siblings → simplify → split (no voxel path)
3. Store `FPackedHierarchyNode` with sphere bounds + child references
4. GPU: per-workgroup BVH traversal with group-shared stack
5. No streaming integration yet (all data in one buffer)
