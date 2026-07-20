# Nanite Cluster Building (Phase 4B)

UE5 source: `Engine/Source/Developer/NaniteBuilder/Private/Cluster.h`, `Cluster.cpp`

## 1. Architecture Overview

Nanite's mesh preprocessing pipeline partitions static mesh triangles into **clusters** of exactly 128 triangles each. Clusters are the fundamental GPU work unit: one cluster = one wave/subgroup worth of culling + one compute workgroup for rasterization. Clusters form the leaves of the DAG hierarchy.

The build pipeline operates on CPU-side intermediate data structures before encoding into `FPackedCluster` (the GPU-consumable format). The central CPU class is `FCluster`.

## 2. Key Data Structures

### 2.1 FVertexFormat

```cpp
struct FVertexFormat {
    uint8 NumTexCoords      = 0;
    uint8 NumBoneInfluences = 0;
    bool  bHasTangents      : 1 = false;
    bool  bHasColors        : 1 = false;
};
```

Describes which vertex attributes are present. The inline functions (`GetColorOffset`, `GetUVOffset`, `GetBoneInfluenceOffset`, `GetVertSize`) compute attribute byte offsets from the format flags. Layout:
- `float3 Position` (12 bytes, always)
- `float3 Normal` (12 bytes, always)
- `float3 TangentX` (12 bytes, if `bHasTangents`)
- `float TangentYSign` (4 bytes, if `bHasTangents`)
- `float4 Color` (16 bytes, if `bHasColors`)
- `float2 UV[N]` (8N bytes)
- `float2 BoneInfluences[M]` (8M bytes, pairs of (boneIndex, boneWeight))

### 2.2 FVertexArray

A flat `TArray<float>` storing interleaved vertex attributes. Indexed by `VertIndex * VertSize + offset`. Provides typed accessors (`GetPosition`, `GetNormal`, `GetUVs`, etc.) via template `Fetch<>`. Key operations:
- `Add()` / `AddUninitialized()` — append vertices
- `RemoveAt()` — delete vertex range
- `CopyAttributes()` / `LerpAttributes()` — attribute transfer between formats
- `FindOrAddHash()` — deduplication via spatial hash
- `Sanitize()` — clamp positions/normals/colors/UVs to valid ranges, NaN-safe

### 2.3 FCluster (CPU Build Intermediate)

```cpp
static const uint32 ClusterSize = 128;  // Exactly 128 triangles per cluster

uint32       NumTris;                     // Current triangle count (0 for voxel clusters)
FVertexArray Verts;                      // Vertex data (interleaved floats)
TArray<uint32> Indexes;                  // Triangle indices (NumTris * 3)
TArray<int32>  MaterialIndexes;          // Per-triangle material IDs

TArray<int8>   ExternalEdges;            // Per-edge adjacency count to other clusters
uint32         NumExternalEdges;         // Number of edges on cluster boundary

TBitArray<>    VoxelTriangle;            // Whether each triangle is a voxel proxy
bool           bHasVoxelTriangles;

struct FBrick {
    uint64       VoxelMask;              // 64-bit bitmask for a 4x4x4 brick
    FIntVector3  Position;               // Brick world position (voxel grid)
    uint32       VertOffset;             // Start index into Verts array for this brick
};
TArray<FBrick> Bricks;                   // Voxel bricks (only for NumTris == 0 clusters)

FBounds3f     Bounds;                    // AABB of all vertices
uint64        GUID;                      // Hash for stable identification
int32         MipLevel;                  // DAG depth level (0 = leaf)
FIntVector    QuantizedPosStart;         // Position quantization anchor
int32         QuantizedPosPrecision;     // Quantization bit count
FIntVector    QuantizedPosBits;          // Bits per axis
float         EdgeLength;                // Maximum triangle edge length in cluster
float         LODError;                  // Simplification error bound
float         SurfaceArea;               // Total triangle surface area
FSphere3f     SphereBounds;              // Tight bounding sphere (for culling)
FSphere3f     LODBounds;                 // Conservative LOD bounding sphere (includes simplification error)

uint32        GroupIndex;                // Group assignment for streaming
uint32        GeneratingGroupIndex;      // Which group produced this cluster
uint32        PageIndex;                 // Streaming page index (MAX_uint32 if unassigned)

TArray<FMaterialRange, TInlineAllocator<4>> MaterialRanges;  // Sorted material triangle ranges
TArray<FIntVector> QuantizedPositions;                        // GPU-side quantized positions
FStripDesc    StripDesc;                 // Triangle strip encoding descriptor
TArray<uint8> StripIndexData;            // Encoded triangle strip data
```

### 2.4 FClusterRef

Lightweight reference to a cluster within the DAG:
```cpp
uint32 InstanceIndex;  // MAX_uint32 = not instanced, else index into AssemblyInstanceData
uint32 ClusterIndex;   // Index into DAG.Clusters array
```

### 2.5 FMaterialRange

Describes a contiguous range of triangles sharing the same material within a cluster:
```cpp
uint32 RangeStart;     // First triangle index in the sorted range
uint32 RangeLength;    // Number of triangles
uint32 MaterialIndex;  // Material ID
TArray<uint8> BatchTriCounts;  // Sub-batch sizes for GPU execution
```

## 3. Algorithm Flow

### 3.1 Construction from Source Mesh

`FCluster(const FConstMeshBuildVertexView& InVerts, ...)` — constructs leaf clusters from raw mesh data.

**Step 1: Gather triangles.** The builder passes a contiguous sorted range `[Begin, End)` of up to 128 triangles (after graph partitioning). For each triangle in the range:
- Look up each vertex in a `TMap<uint32, uint32> OldToNewIndex` to deduplicate shared vertices
- Copy position, normal, tangents, colors, UVs, bone influences from source mesh

**Step 2: Compute external edges.** For each triangle edge, count adjacent triangles that fall **outside** the current cluster range. These are cluster boundary edges — critical for simplification (locked edges prevent cracking). The adjacency is precomputed via `FAdjacency`.

**Step 3: Sanitize vertices.** Run `CorrectAttributes()` on every vertex to normalize normals, orthogonalize tangents, clamp colors. Then call `Verts.Sanitize()` for NaN-safe clamping.

**Step 4: Compute bounds.** `Bound()` iterates all vertices to compute:
- `Bounds` — AABB enclosing all positions
- `SphereBounds` — optimal bounding sphere (fits all vertex positions)
- `SurfaceArea` — sum of triangle areas
- `EdgeLength` — maximum edge length (used for LOD error metric)

### 3.2 Cluster Splitting

`FCluster(FCluster& SrcCluster, uint32 Begin, uint32 End, ...)` — splits an existing cluster by extracting a triangle sub-range. Used during recursive graph partitioning. Copies vertices, indexes, and external edges from the source cluster for the specified triangle range. Clamped to `ClusterSize` (128).

### 3.3 Merging Children (DAG Parent Construction)

`FCluster(const FClusterDAG& DAG, TArrayView<const FClusterRef> Children)` — merges child clusters to form a DAG parent node.

**Step 1: Determine vertex format.** Iterate all children to find the union of all vertex formats (max UV count, max bone influence count, OR of bHasTangents/bHasColors). This ensures the parent can represent all children.

**Step 2: Merge vertices.** For each child:
- If **matching format**: `FindOrAddHash()` deduplicates vertices by exact attribute match using spatial hashing
- If **mismatched format** or **instanced**: `AddVertMismatched()` creates temporary vertex, copies/transforms attributes, then hash-deduplicates
- Instanced children apply `Transform.TransformPosition()` and `NormalTransform.TransformVector()`

**Step 3: Merge bounds.** Bounds and surface area are accumulated (union AABB, sum surface area). Instanced bounds are transformed by the instance matrix.

**Step 4: Recompute adjacency.** External edges are recalculated — interior edges between merged children now become internal edges (adjacency count decreases).

**Step 5: Voxel children.** If `bAllowVoxels && Child.NumTris == 0`, the child contains voxel brick data instead of triangles. Each brick becomes a proxy triangle (random orientation at the brick position). The triangle scale is `LODError * sqrt(4 * NumVoxels)` to maintain screen-space footprint.

### 3.4 Simplification

`Simplify(DAG, TargetNumTris, TargetError)` — reduces triangle count using quadric error metric (QEM) simplification.

**Step 1: Floating-point precision scaling.** To avoid loss of precision during edge collapse, positions are scaled so the float exponent matches a desired value (~0.25). This is a **lossless** operation that only changes the float exponent field.

**Step 2: Attribute weights.** Configures QEM weights:
- Normal: weight=1.0 (highest priority)
- Tangents: weight=0.0625
- TangentYSign: weight=0.5
- Colors: weight=0.0625
- UVs: weight computed from UV area ratio (adaptive); weight=0 if `!bLerpUVs` (forces dominant-corner copy)
- Bone influences: weight=0 (always copy from nearest original vertex)

**Step 3: Lock external edges.** All boundary edges (adjacency count > 0) are locked to prevent cracks between clusters. Both endpoint positions are locked via `Simplifier.LockPosition()`.

**Step 4: Run QEM simplification.** `MeshSimplifier.Simplify()` performs iterative edge collapses targeting `TargetNumTris` triangles with error bound `TargetError^2`. Maximum edge length is constrained by `MaxEdgeLengthFactor`.

**Step 5: Post-processing.** `Simplifier.PreserveSurfaceArea()` or `ShrinkVoxelTriangles()` adjusts triangle positions to compensate for area loss. `Simplifier.Compact()` writes final data back to the vertex/index arrays.

**Step 6: Restore scale.** Vertices multiplied by inverse scale. Error is converted back: `sqrt(MaxErrorSqr) * InvScale`.

### 3.5 Graph Partitioning (Split)

`Split(FGraphPartitioner& Partitioner, const FAdjacency& Adjacency)` — partitions oversized clusters into sub-clusters of at most 128 triangles.

**Step 1: Build disjoint sets.** Connected components via shared edges (union-find on triangle pairs).

**Step 2: Build locality links.** The partitioner uses `BuildLocalityLinks()` to create spatial connections between triangles (not just topological adjacency). This encourages spatially coherent partitions.

**Step 3: Build partition graph.** Each triangle is a node. Edges connect adjacent triangles (weight=260 for shared edges) and locality-linked triangles (weight=1). The graph is then partitioned into balanced subgraphs using a Metis-like multilevel algorithm via `PartitionStrict()`.

### 3.6 Voxelization

`Voxelize(FClusterDAG& DAG, TArrayView<const FClusterRef> Children, float VoxelSize)` — converts distant geometry into a sparse voxel representation.

**Step 1: Triangle rasterization to voxels.** Each child triangle is rasterized into a voxel grid using `VoxelizeTri26()` (26-connectivity). Position is scaled by `1/VoxelSize`. Each voxel stores a `TMap<FIntVector3, uint32>` entry.

**Step 2: Ray tracing.** For each voxel, cast rays against the full mesh (via `FRayTracingScene`) to determine coverage and normal distribution function (NDF):
- Multiple stratified ray directions (configurable, default 32 rays)
- GPU-style intersections using `Intersect16`
- Coverage = HitCount / RayCount
- NDF = average of hit normals (for orientation-dependent opacity)

**Step 3: Coverage filtering.** Voxels with zero coverage are discarded (but remembered in `ExtraVoxels` for higher LOD levels). For high ray counts, a coverage redistribution pass moves excess coverage to neighboring voxels weighted by the NDF projected area — this ensures energy conservation.

**Step 4: Attribute lerp.** For surviving voxels, vertex attributes are interpolated from the hit triangle at the hit point's barycentric coordinates. Color channel A optionally stores NDF anisotropy; channel B optionally stores opacity.

**Step 5: Voxels-to-bricks.** `VoxelsToBricks()` packs voxels into 4x4x4 bricks (64-bit mask). Bricks sharing the same material are grouped into a single `FBrick` entry.

### 3.7 Material Range Building

`BuildMaterialRanges()` — sorts triangles within a cluster by material ID to create contiguous ranges for efficient GPU dispatch.

**Algorithm:**
1. Tally triangles per material index
2. Sort triangles: primary key = descending range size, secondary = ascending material index
3. Group consecutive triangles with the same material into `FMaterialRange`
4. Reorder vertex arrays/indexes to match the new triangle order

The sort order (largest range first) is critical for GPU: the first range must have more than 1 triangle to enable a "minus one" encoding optimization in the material evaluation state machine.

## 4. Bounds Computation

`Bound()` computes three levels of spatial bounds:
- **`Bounds`** (AABB): union of all vertex positions — tight fit
- **`SphereBounds`**: minimal bounding sphere fitting all vertex positions — used for fast sphere-plane frustum tests on GPU
- **`LODBounds`**: initially equals `SphereBounds` but later expanded to account for simplification error (LODError)
- **`SurfaceArea`** and **`EdgeLength`**: geometric statistics used for error metrics and parent cluster construction

## 5. Design Decisions / Tradeoffs

### 5.1 Fixed Cluster Size (128)
- Matches typical GPU wave/subgroup size for efficient wave-level operations
- Simplifies workgroup sizing: one workgroup per cluster
- 128 triangles is a sweet spot between culling granularity and draw overhead
- The partitioner enforces this strictly: oversized clusters are recursively split

### 5.2 External Edge Locking
- Boundary edges are locked during simplification to prevent LOD seams between adjacent clusters
- This is the key invariant that allows independent per-cluster simplification without cracking
- Cost: boundary vertices cannot be simplified, limiting the maximum LOD reduction for clusters with many boundary edges

### 5.3 Float Exponent Scaling
- QEM simplification uses floating-point math that degrades precision with large coordinate values
- Scaling positions so the exponent matches ~0.25 before simplification, then undoing the scale after, is a clever lossless trick
- Avoids the need for double precision in the simplifier

### 5.4 Voxel Representation
- At coarse LOD levels, geometry is replaced with sparse voxels (4x4x4 bricks)
- Voxels sample the original geometry's opacity and normal distribution via stochastic ray casting
- Enables level-of-detail for highly complex shapes that cannot be simplified with traditional edge collapse
- Tradeoff: voxels lose geometric precision but preserve silhouette and coverage at extreme distances
- ExtraVoxels queue remembers voxels rejected at this level for consideration at coarser levels

### 5.5 Material Range Sorting
- Largest material ranges first enables an efficient GPU state machine
- The minus-one encoding works because the first range always has >= 2 triangles
- Sorting once on CPU avoids per-pixel material lookups on GPU

## 6. What Tumbler Phase 4 Should Adopt

### Phase 4A (Immediate):
- **Fixed cluster size of 128 triangles** — adopt directly, it maps well to GPU workgroups
- **AABB + bounding sphere computation** — essential for frustum culling
- **External edge tracking** — critical for preventing LOD seams
- **Material range sorting** — enables efficient per-cluster material dispatch
- **Simple vertex deduplication** via hash table (no mismatched format merging needed yet)

### Phase 4A (Simplify):
- **Skip QEM simplification entirely** — Phase 4 uses full-detail clusters only (no LOD)
- **Skip voxelization** — Phase 4 works at LOD 0 only, too close for voxels to be needed
- **Skip instancing** — Phase 4 targets static meshes without assembly transforms
- **Skip bone influences** — skeletal meshes are out of scope for Phase 4
- **Skip float exponent scaling trick** — not needed without simplification
- **Use a simpler graph partitioner** — only need spatial clustering, not the full Metis-like approach. A simple Morton-code sort + linear chunking into 128-tri groups is sufficient for Phase 4

### Key simplifications for Phase 4 topology:
1. Cluster = Morton-code sorted triangles grouped into 128-tri chunks
2. No LOD, no simplification, no voxels
3. Bounds = tight AABB + bounding sphere (use Ritter's algorithm for sphere)
4. Material ranges = sort triangles by material, build contiguous ranges
5. External edges = mark edges shared with triangles outside the cluster

### Save for later phases:
- QEM simplification (Phase 6 — LOD hierarchy)
- Voxelization pipeline (Phase 7+)
- Graph partitioner with locality links (Phase 6)
- Assembly/instancing support (Phase 9)
- Skeletal mesh vertex formats (out of scope)
