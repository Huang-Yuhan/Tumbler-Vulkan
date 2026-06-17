# Nanite Streaming (Phase 9)

UE5 source: `Runtime/Renderer/Private/Nanite/NaniteStreamOut.cpp`, `Shaders/Private/Nanite/NaniteStreaming.usf`, `NaniteStreaming.ush`, `Runtime/Engine/Public/Rendering/NaniteStreamingManager.h`, `Shared/NaniteDefinitions.h`

## 1. Architecture Overview

Nanite's streaming system is a **GPU-driven, demand-paged** virtual geometry system. The CPU prepares data but the GPU decides what to stream via a GPU-to-CPU feedback mechanism. The streaming is integrated directly into the culling/rasterization pipeline: as the GPU traverses the BVH hierarchy and discovers that certain nodes need more detailed geometry (pages not yet resident), it writes streaming requests to a ring buffer that the CPU reads back and processes.

Key design principles:
- **Root pages** always resident (loaded at resource creation time) -- guarantee there is always something to render.
- **Streaming pages** loaded on demand at GPU request with priority derived from projected pixel error.
- **The DAG hierarchy provides inherently streaming-friendly data:** interior nodes are valid LOD representations of children.
- **GPU writes requests, CPU processes them:** a feedback loop where the GPU signals what it needs and the CPU loads it for future frames.

Key constants:
- `NANITE_ROOT_PAGE_GPU_SIZE = 128 KB` (root pages, always loaded)
- `NANITE_STREAMING_PAGE_GPU_SIZE = 512 KB` (streamed pages, loaded on demand)
- `NANITE_ROOT_PAGE_MAX_CLUSTERS = 4096` (max clusters in root pages)
- `NANITE_STREAMING_PAGE_MAX_CLUSTERS = 16384` (max clusters per streamed page)
- `NANITE_MAX_GPU_PAGES = 131072` (17-bit page index)
- `NANITE_MAX_CLUSTER_HIERARCHY_DEPTH = 32` (max DAG depth)
- `NANITE_MAX_PRIORITY_BEFORE_PARENTS = 0xFFFFFFE0` (priority threshold to elevate parent pages)

## 2. Key Data Structures

### 2.1 FPageKey (CPU)

```cpp
struct FPageKey {
    uint32 RuntimeResourceID;   // Unique ID for each Nanite resource
    uint32 PageIndex;           // Page index within that resource
};
```

### 2.2 FStreamingRequest (GPU → CPU)

```cpp
struct FStreamingRequest {
    uint RuntimeResourceID_Magic;   // Resource ID + sanity check magic bits
    uint ResourcePageRangeKey;      // Identifies which page range to stream
    uint Priority_Magic;            // Priority + magic bits
};
```

The `ResourcePageRangeKey` encodes:
- Number of pages/ranges in this request (`NANITE_PAGE_RANGE_KEY_COUNT_MASK`)
- Flag: `NANITE_PAGE_RANGE_KEY_FLAG_HAS_STREAMING_PAGES` — only emit request if pages are streamable
- Flag: `NANITE_PAGE_RANGE_KEY_FLAG_MULTI_RANGE` — request covers multiple page ranges
- Page range index (25 bits): index into `PageRangeLookup` table

### 2.3 FPageRangeKey Lookup

```cpp
// Each entry in PageRangeLookup describes a contiguous range of pages
// that share LOD and can be loaded together.
struct FPageRangeKey {
    uint32 GroupIndex;          // Group that owns this range
    uint32 StartPage;           // First page in range
    uint32 NumPages;            // Number of contiguous pages
    uint32 Flags;               // HAS_STREAMING_PAGES, etc.
};
```

### 2.4 Hierarchy Node Slice (GPU)

Each BVH node child stores a `ResourcePageRangeKey` in `Misc2[NANITE_MAX_BVH_NODE_FANOUT]`. During traversal, if a node is selected for finer LOD but its pages are not resident, the GPU writes a streaming request for the missing pages and uses the coarser (parent) representation for the current frame.

The node also has a `bLoaded` flag that is checked before descending:
```pseudocode
if bVisible && bLoaded && !bLeaf:
    // descend into children
else if bVisible && bLeaf:
    // emit clusters (whether loaded or not — if not loaded, use parent LOD)
```

## 3. Algorithm Flow

### 3.1 Initial Resource Setup

1. Mesh is built offline: DAG hierarchy + pages are generated.
2. **Root pages** (containing the coarsest LOD + the first few levels of the DAG, up to 128KB) are embedded in the `FResources::RootData` array. These are uploaded to GPU immediately when the resource is registered.
3. **Streaming pages** are stored in `FByteBulkData StreamablePages` (cooked data) or in the Derived Data Cache (DDC).

### 3.2 GPU-Side Streaming Request Generation

During culling (in `NaniteClusterCulling.usf`), when a BVH node is processed:

```pseudocode
// In ShouldVisitChild():
if bVisible && !bLeaf:
    LODError = max(MinLODError, MaxParentLODError)
    ProjectedError = compute_screen_space_error(LODBounds, LODError)
    if ProjectedError > pixel_threshold:
        // Need finer LOD
        if !bLoaded:
            // Pages not resident — issue streaming request
            PriorityCategory = ProjectedError_to_category(ProjectedError)
            Priority = asfloat(ProjectedError)
            RequestPageRange(StreamingRequests, RuntimeResourceID, 
                           ResourcePageRangeKey, PriorityCategory, Priority)
        // Use parent representation for this frame
        return false  // Don't descend
    else:
        // LOD is sufficient, use this node
        return true
```

The `RequestPageRange()` function (`NaniteStreaming.ush`):

```pseudocode
RequestPageRange(RequestsBuffer, RuntimeResourceID, ResourcePageRangeKey, PriorityCategory, Priority):
    if !(RenderFlags & OUTPUT_STREAMING_REQUESTS): return
    if NumPagesOrRanges == 0: return
    if !bHasStreamingPages: return  // Root pages are always resident
    
    // Atomically allocate slot in the request buffer
    Index = WaveInterlockedAdd(RequestsBuffer[0].RuntimeResourceID_Magic, 1)  // HACK: count in first entry
    
    if Index < StreamingRequestsBufferSize - 1:
        // Pack priority: category in top 2 bits, float priority in remaining bits
        UIntPriority = clamp((PriorityCategory << 30) | (asuint(Priority) >> 2), 1, MAX_PRIORITY_BEFORE_PARENTS)
        
        Request.RuntimeResourceID = RuntimeResourceID
        Request.ResourcePageRangeKey = ResourcePageRangeKey
        Request.Priority = UIntPriority
        
        // Sanity check: embed magic values to detect stale data
        if SANITY_CHECK:
            Request.RuntimeResourceID |= (0x10 | FrameNibble) << MAGIC_BITS
            Request.Priority |= (0x20 | FrameNibble) << MAGIC_BITS
        
        RequestsBuffer[Index + 1] = Request
```

### 3.3 CPU-Side Readback and Processing

The streaming manager on the CPU side (`FStreamingManager`):

1. **Readback GPU buffer**: After each frame, the GPU-generated streaming requests buffer is copied back to CPU memory via an async readback.

2. **Parse requests**: Each `FStreamingRequest` is validated (magic number check if sanity checks enabled) and translated to `FPageKey` + priority.

3. **Deduplicate and prioritize**: Requests are accumulated in a priority queue. The `FPageKey` + priority are sorted: higher priority (closer to camera, more visible error) requests are processed first.

4. **Page allocation**: Available GPU memory pages are allocated from a pool (`FSpanAllocator`). If memory is full, lowest-priority pages are evicted (LRU with priority weighting).

5. **Data loading**: Page data is loaded from either:
   - The **bulk data** stored with the resource (cooked data)
   - The **Derived Data Cache** (DDC) for editor/preview builds

6. **GPU upload**: The loaded page data is uploaded to the GPU page pool buffer. Since Nanite pages are in a compact intermediate format on disk, they are transcoded to the final GPU-consumable format during upload (via `NaniteTranscode.usf` — see Topic 6).

7. **Hierarchy update**: The `bLoaded` flags in the BVH hierarchy nodes for the affected pages are updated to reflect the new residency state.

### 3.4 Page Residency and Fixup

After streaming, the hierarchy needs to be updated so the GPU knows which pages are now resident:

1. The `HierarchyBuffer` contains `FHierarchyNodeSlice` entries with `bLoaded` flags.
2. When a page is streamed in, the corresponding `ResourcePageRangeKey` entries are marked as loaded.
3. The GPU reads `bLoaded` during traversal:
   - If `bLoaded`: the cluster data for this node is valid in the `ClusterPageData` buffer.
   - If `!bLoaded`: the node is a "streaming leaf" — the GPU treats it as if it were a leaf node in the DAG, using the coarser parent LOD representation.

### 3.5 StreamOut (Geometry Export to CPU)

`NaniteStreamOut.cpp` implements the reverse path: exporting GPU-visible geometry back to CPU memory. This is used for Nanite ray tracing (building a BVH on CPU) and for the legacy renderer fallback.

The stream-out pipeline:

1. **Init queue**: GPU receives a list of resources to stream out, initializes the traversal queue with root nodes.
2. **Count pass** (optional cache): Traverses the DAG, culls against a cut error threshold, counts visible vertices and triangles. Optionally caches the cluster list to avoid re-traversal.
3. **Allocate ranges**: Based on the count, allocates contiguous ranges in the output vertex/index buffers.
4. **StreamOut pass**: Uses the cached cluster list (from count pass) to decode vertex positions, indices, and attributes, writing them to the output buffers. Uses indirect dispatch.

## 4. Priority System

### 4.1 Priority Components

Each streaming request has a 32-bit priority composed of:
- **Bits [31:30]**: Priority category (2 bits):
  - Category 0: Directly visible geometry (highest priority)
  - Category 1-3: Prefetching hints, shadow-casting, etc. (lower priority)
- **Bits [29:0]**: Float priority reinterpreted as uint (projected pixel error, inverted)
  - Lower numeric value = higher actual priority (like a min-heap)
  - The clamping `MAX_PRIORITY_BEFORE_PARENTS = 0xFFFFFFE0` ensures parent pages (which are larger) get high priority

### 4.2 Priority Computed from LOD Error

The priority is derived from the projected screen-space LOD error:
```pseudocode
// Larger projected error = more detail needed = higher streaming priority
ProjectedRadius = project_sphere_radius(LODBounds, ViewToClip)
PixelError = max(0, ProjectedRadius - pixel_threshold)
Priority = 1.0 / (1.0 + PixelError)  // Lower pixel error = higher priority value
```

### 4.3 Parent Page Elevation

A special mechanism ensures parent pages are always resident before too many child pages are requested. If a page request has priority below `MAX_PRIORITY_BEFORE_PARENTS`, only the parent page (not individual child pages) is requested. This prevents a "fog of war" scenario where many small pages are requested but the coarser representation hasn't loaded yet.

## 5. Memory Management

### 5.1 GPU Page Pool

```
Total GPU memory for Nanite pages = NANITE_MAX_GPU_PAGES * NANITE_STREAMING_PAGE_GPU_SIZE
                                  = 131072 * 512 KB
                                  = 64 GB (theoretical maximum)
                                  Practical: 128-512 MB (configurable)
```

Pages use a **span allocator** (`FSpanAllocator`): variable-sized allocations in a fixed-size pool. Pages are contiguous in GPU memory.

### 5.2 Page Eviction

When the page pool is full and a higher-priority page needs to be loaded:
1. Find the lowest-priority resident page
2. If the new page's priority exceeds the existing page's priority, evict the old page
3. Mark evicted pages as not loaded in the hierarchy (their clusters become inaccessible — but parent nodes still provide valid LOD)

### 5.3 Root Pages

Root pages (the coarsest LOD levels) are **never evicted**. They are loaded at resource creation and remain resident for the resource's lifetime. This guarantees there is always something to draw, even if no streaming pages are ready.

The root pages typically contain:
- The top levels of the DAG hierarchy (coarse BVH nodes)
- The coarsest cluster representations (small number of very simplified triangles or large voxels)
- This data fits in 128 KB per resource

## 6. Design Decisions / Tradeoffs

### 6.1 GPU-Driven Request Model
- **Pro**: The GPU knows exactly which pages are visible and at what LOD — no CPU-side prediction or heuristic needed.
- **Pro**: Streaming is naturally latency-tolerant: non-resident pages use coarser LOD (which is available in parent nodes), so missing data never causes holes.
- **Con**: One-frame latency: requests from frame N are only fulfilled by frame N+2 (readback + load + upload).
- **Con**: Requires a GPU readback path, which adds complexity and driver overhead.

### 6.2 Single Request Buffer
- All streaming requests from all views and all resources go into one shared ring buffer.
- The buffer is cleared at the start of each frame (`ClearStreamingRequestCount`).
- The first entry stores the total count (packed into `RuntimeResourceID_Magic` as a hack).
- Buffer size: typically 4096 entries, large enough for visible geometry + some headroom.

### 6.3 LOD as Streaming Proxy
- The DAG hierarchy inherently provides streaming proxies: parent nodes are valid simplified geometry.
- When a page is not resident, the GPU simply stops traversing at that node (treats it as a streaming leaf).
- There is no "pop-in" — the parent's LOD representation is already visible. When the page streams in, finer detail smoothly replaces the coarser version.

### 6.4 Page-Local Cluster References
- Cluster indices within the hierarchy (`ChildStartReference`) are page-local (masked with `NANITE_MAX_CLUSTERS_PER_PAGE_MASK`).
- This means cluster references are small (15-17 bits) and don't change when pages move in memory.
- The page index provides the global offset: `ClusterData[PageBaseAddress + LocalClusterIndex * sizeof(FPackedCluster)]`.

### 6.5 StreamOut Caching
- The optional caching of traversal results avoids traversing the DAG twice (once for counting, once for writing).
- The cluster list from the count pass is reused in the write pass.
- Tradeoff: caching requires additional GPU memory for the cluster list buffer.

### 6.6 Sanity Check Magic Values
- `NANITE_SANITY_CHECK_STREAMING_REQUESTS`: embeds a frame counter nibble in streaming request entries.
- If the readback returns data from a previous frame (stale buffer), the magic values won't match and the request is ignored.
- Protects against the readback racing with the GPU still writing the buffer.

## 7. What Tumbler Phase 9 Should Adopt

### Adopt:
- **GPU-to-CPU streaming requests** — the feedback loop model
- **Root pages always resident** — guarantee renderable geometry
- **LOD-as-streaming-proxy** — DAG hierarchy provides natural LOD fallback
- **Priority system** based on projected pixel error
- **Page pool with LRU eviction** — simple, effective

### Simplify for Phase 9:
- **No readback race protection** — simpler buffer management without magic value checks
- **Fixed-size pages** instead of variable-size via span allocator
- **No StreamOut** — this is for ray tracing fallback, not needed for Phase 9
- **Single-priority-class requests** — 32-bit priority, no category bits
- **No parent elevation heuristic** — simpler priority ordering

### Implementation Notes:
1. The streaming request buffer must be accessible as both UAV (GPU write) and CPU readback
2. One-frame latency is acceptable because coarser LOD is always available
3. Start with a simple page pool: fixed-size slots, mark-and-sweep eviction
4. Root pages can be larger in Tumbler (e.g., 512KB) since we have fewer resources to stream
5. The `RequestPageRange` function is simple enough to port directly
6. Ensure the hierarchy update after streaming is atomic (double-buffered hierarchy buffer)

### Key Streaming Pipeline for Tumbler:
```
Frame N:
  1. GPU culling: write streaming requests to ring buffer
  2. Present frame (with available LODs)
  
Frame N+1:
  3. CPU readback: copy ring buffer to CPU
  4. CPU process: deduplicate requests, sort by priority, load pages
  5. GPU upload: transcode + upload pages to GPU pool
  6. Update hierarchy bLoaded flags

Frame N+2:
  7. New pages are available for rendering
```

This 2-frame latency is fundamental to GPU-readback-based streaming and is acceptable for all but the most extreme camera teleportation scenarios.

## 8. Relationship to Other Subsystems

- **Cluster DAG (Topic 2)**: The hierarchy provides the `ResourcePageRangeKey` and `bLoaded` flags that drive streaming decisions. Without the DAG, streaming would have no LOD proxy.
- **Vertex Encoding (Topic 6)**: Streamed pages use transcoding (disk format → GPU format). The transcoding shader decompresses variable-bit data into the final GPU layout.
- **GPU Culling (Topic 3)**: Streaming requests are generated during culling. The `ShouldVisitChild()` callback in the traversal shader checks `bLoaded` to decide whether to descend or emit a request.
- **StreamOut**: The reverse path (GPU→CPU geometry export) shares the same DAG traversal code but writes vertex/index data instead of rasterizing.
