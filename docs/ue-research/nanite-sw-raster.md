# Nanite Software Rasterization (Phase 4D)

UE5 source: `Shaders/Private/Nanite/NaniteRasterizer.usf`, `NaniteRasterizer.ush`, `NaniteRasterizationCommon.ush`

## 1. Architecture Overview

Nanite uses a compute-based software rasterizer for small triangles and a hardware rasterizer for large triangles. The primary motivation is that hardware rasterization efficiency degrades dramatically for triangles covering only a few pixels -- the fixed-function pipeline is optimized for triangles covering many pixels. In extreme cases (sub-pixel triangles), hardware rasterization wastes massive amounts of quad/wavefront capacity.

The software rasterizer runs as a compute shader that processes one **cluster** (up to 128 triangles) per workgroup. Each workgroup transforms all vertices, then iterates over triangles and rasterizes them using edge-function tests in compute.

Key constants:
- `NANITE_SUBPIXEL_BITS = 8` (256 subpixel samples per pixel)
- `NANITE_SUBPIXEL_SAMPLES = 256` (8-bit subpixel precision)
- Workgroup size: 64 threads (standard), 32 for voxel/vert-reuse paths
- Max triangle rect size: 63x63 pixels (hard limit to keep register pressure low)
- Subpixel coordinate format: 16.8 fixed point for vertices, 4.8 fixed point for edges

## 2. Key Data Structures

### 2.1 FRasterTri

```cpp
struct FRasterTri {
    int2   MinPixel;        // Pixel-space bounding rect min (inclusive)
    int2   MaxPixel;        // Pixel-space bounding rect max (inclusive)
    float2 Edge01;          // Edge from V0 to V1 (4.8 fixed point)
    float2 Edge12;          // Edge from V1 to V2
    float2 Edge20;          // Edge from V2 to V0
    float  C0;              // Edge constant for Edge12 at MinPixel
    float  C1;              // Edge constant for Edge20 at MinPixel
    float  C2;              // Edge constant for Edge01 at MinPixel
    float3 DepthPlane;      // Planar depth equation (at subpixel origin)
    float3 InvW;            // Reciprocal w for perspective-correct interpolation
    float3 Barycentrics_dx; // Screen-space derivative of barycentrics (x-direction)
    float3 Barycentrics_dy; // Screen-space derivative of barycentrics (y-direction)
    bool   bIsValid;        // False if culled or zero area
    bool   bBackFace;       // Detected backface
};
```

### 2.2 FRaster

```cpp
struct FRaster {
    float2 ViewportScale;   // NDC-to-pixel scale (subpixel: * 256)
    float2 ViewportBias;    // NDC-to-pixel bias
    int4   ScissorRect;     // Viewport scissor rect in pixels
    // VSM-related fields (virtual shadow map page translation)
};
```

### 2.3 FCachedVertex

A 160+ byte struct holding transformed vertex data for pixel programmable shading:
- `PointSubpixelClip`: clip-space position in subpixel coordinates
- `TransformedVert`: full transformed vertex (position, normal, tangent, UVs, color)
- Stored in groupshared arrays of 64 elements (rolling window cache)

### 2.4 TNaniteWritePixel

A callable struct that encapsulates writing to the visibility buffer:
```cpp
template<typename FSoftwareShader, typename FPageTranslation>
struct TNaniteWritePixel {
    FRaster           Raster;
    FSoftwareShader   Shader;       // Material shader for pixel evaluation
    uint              PixelValue;   // Packed: (VisibleIndex+1) << 7 | TriIndex
    uint2             VisualizeValues;
    FPageTranslation  PageTranslation;

    void operator()(uint2 PixelPos, float3 C, FRasterTri Tri) {
        // 1. Compute depth from plane equation
        float DeviceZ = Tri.DepthPlane.x + Tri.DepthPlane.y * C.y + Tri.DepthPlane.z * C.z;
        // 2. Create VisBuffer pixel
        FVisBufferPixel Pixel = CreateVisBufferPixel(PixelPos, PixelValue, DeviceZ);
        // 3. Apply VSM page translation (if needed)
        if (!PageTranslation(Pixel)) return;
        // 4. Atomic depth test + write VisBuffer
        Pixel.WriteOverdraw();
        // 5. If pixel programmable: do material evaluation (alpha test, PDO, etc.)
        if (!Shader.EvaluatePixel(...)) return;
        // 6. Final write (color/depth export)
        Pixel.Write();
    }
};
```

## 3. Algorithm Flow

### 3.1 Triangle Setup (`SetupTriangle`)

Converts clip-space triangle vertices to a subpixel-precision rasterization description:

**Step 1: Backface culling.**
```pseudocode
Edge01 = Vert0.xy - Vert1.xy
Edge12 = Vert1.xy - Vert2.xy
Edge20 = Vert2.xy - Vert0.xy
DetXY = Edge01.y * Edge20.x - Edge01.x * Edge20.y
bBackFace = (DetXY >= 0)  // CW winding in clip space
```
For two-sided materials, backface triangles have their winding order swapped (negate all edges). For one-sided materials, backfaces are culled.

**Step 2: Compute bounding rect.**
```pseudocode
MinSubpixel = min3(Vert0.xy, Vert1.xy, Vert2.xy)
MaxSubpixel = max3(Vert0.xy, Vert1.xy, Vert2.xy)
// Convert from subpixel to pixel, with conservative rounding
MinPixel = floor((MinSubpixel + SubpixelSamples/2 - 1) / SubpixelSamples)
MaxPixel = floor((MaxSubpixel - SubpixelSamples/2 - 1) / SubpixelSamples)
// Clamp to scissor rect
MinPixel = max(MinPixel, ScissorRect.xy)
MaxPixel = min(MaxPixel, ScissorRect.zw - 1)
// Limit to 64 pixels max (63 pixel range)
MaxPixel = min(MaxPixel, MinPixel + 63)
```

The 63-pixel limit is critical: it bounds the register pressure and loop iteration count. Larger triangles go through hardware rasterization.

**Step 3: Rebase vertices.**
All vertex coordinates are rebased to `MinPixel * SubpixelSamples + SubpixelSamples/2` (the subpixel center of the top-left pixel). This improves fixed-point precision for the edge functions.

**Step 4: Compute edge constants.**
```
C0 = Edge12.y * Vert1.x - Edge12.x * Vert1.y  // Edge from V1 to V2, evaluated at V1
C1 = Edge20.y * Vert2.x - Edge20.x * Vert2.y  // Edge from V2 to V0
C2 = Edge01.y * Vert0.x - Edge01.x * Vert0.y  // Edge from V0 to V1
```
The edge function `E(x,y) = (y-y0)*(x1-x0) - (x-x0)*(y1-y0)`. For a pixel to be inside the triangle, all three edge functions must be >= 0.

**Step 5: Fill convention correction.** Apply top-left rule nudging:
```pseudocode
C0 -= saturate(Edge12.y + saturate(1 - Edge12.x))
C1 -= saturate(Edge20.y + saturate(1 - Edge20.x))
C2 -= saturate(Edge01.y + saturate(1 - Edge01.x))
```
This ensures edges shared by adjacent triangles rasterize exactly once (no double hits, no cracks).

**Step 6: Scale constants.** Divide edge constants by `SubpixelSamples` (lossless because it's a power of 2) instead of scaling edges up. This keeps the constants in a numerically stable range.

**Step 7: Compute derivatives.**
```pseudocode
ScaleToUnit = SubpixelSamples / (C0 + C1 + C2)  // Sum of edge constants = triangle area
Barycentrics_dx = float3(-Edge12.y, -Edge20.y, -Edge01.y) * ScaleToUnit
Barycentrics_dy = float3( Edge12.x,  Edge20.x,  Edge01.x) * ScaleToUnit
```
These are the screen-space derivatives of barycentric coordinates, used for attribute interpolation.

**Step 8: Depth plane.**
```pseudocode
DepthPlane.x = Verts[0].z
DepthPlane.yz = (Verts[1].z - Verts[0].z, Verts[2].z - Verts[0].z) * ScaleToUnit
```
Depth is computed as `DepthPlane.x + DepthPlane.y * C.y + DepthPlane.z * C.z`.

### 3.2 Rasterization Strategies

The shader uses three rasterization strategies, selected adaptively:

#### 3.2.1 Rect Rasterization (`RasterizeTri_Rect`)

Used when the triangle is small and non-programmable (fast path):

```pseudocode
CY0=C0, CY1=C1, CY2=C2
for y = MinPixel.y to MaxPixel.y:
    // Test first pixel of the row
    if min3(CY0, CY1, CY2) >= 0:
        WritePixel(x, y, float3(CY0, CY1, CY2), Tri)
    // Walk the rest of the row, incrementally updating edge values
    CX0=CY0-Edge12.y, CX1=CY1-Edge20.y, CX2=CY2-Edge01.y
    for x = MinPixel.x+1 to MaxPixel.x:
        if min3(CX0, CX1, CX2) >= 0:
            WritePixel(x, y, float3(CX0, CX1, CX2), Tri)
        CX0 -= Edge12.y; CX1 -= Edge20.y; CX2 -= Edge01.y
    // Step down one row
    CY0 += Edge12.x; CY1 += Edge20.x; CY2 += Edge01.x
```

This is the classic Pineda rasterizer: edge functions are evaluated incrementally. The Barycentrics (C0, C1, C2) at each pixel are exactly the edge function values (unscaled), so they can be used directly for depth interpolation.

#### 3.2.2 Scanline Rasterization (`RasterizeTri_Scanline`)

Used for wider triangles (or when pixel programmable):

```pseudocode
Edge012 = float3(Edge12.y, Edge20.y, Edge01.y)
bOpenEdge = Edge012 < 0  // edges that open rightward
InvEdge012 = select(Edge012 == 0, 1e8, 1/Edge012)

for y = MinPixel.y to MaxPixel.y:
    // Compute where each edge crosses this scanline
    CrossX = float3(CY0, CY1, CY2) * InvEdge012
    // For opening edges (negative slope): pixel is inside when x >= CrossX
    MinX = select(bOpenEdge, CrossX, 0)
    // For closing edges (positive slope): pixel is inside when x <= CrossX
    MaxX = select(bOpenEdge, MaxPixel.x - MinPixel.x, CrossX)
    // Intersection of all three edge spans
    x0 = ceil(max3(MinX.x, MinX.y, MinX.z)) + MinPixel.x
    x1 = min3(MaxX.x, MaxX.y, MaxX.z) + MinPixel.x
    // Rasterize span
    CX = CY - x0_local * Edge012
    for x = x0 to x1:
        if min3(CX0, CX1, CX2) >= 0:
            WritePixel(x, y, float3(CX0, CX1, CX2), Tri)
        CX -= Edge012
    CY += Edge012.x
```

This computes the exact start and end of each scanline span analytically (intersection of edge lines with the scanline), avoiding the inner loop of rect rasterization for empty portions of the bounding rect. Faster for wide triangles (~5+ pixels wide).

#### 3.2.3 Adaptive Selection (`RasterizeTri_Adaptive`)

```pseudocode
bScanline = NANITE_PIXEL_PROGRAMMABLE || WaveActiveAnyTrue(MaxPixel.x - MinPixel.x > 4)
if bScanline:
    RasterizeTri_Scanline(Tri, WritePixel)
else:
    RasterizeTri_Rect(Tri, WritePixel)
```

Threshold of 4 pixels wide: wider than 4 pixels benefits from scanline span skipping. Pixel programmable forces scanline because the inner-loop overhead is dominated by material evaluation anyway.

### 3.3 Per-Cluster Workgroup Dispatch

The compute shader processes one cluster at a time:

**Step 1: Fetch cluster data.** Each workgroup reads `FVisibleCluster` (cluster index, page index, view ID), `FPrimitiveSceneData`, `FInstanceSceneData`, then decodes `FCluster` (vertex/index data from GPU page buffer).

**Step 2: Set up raster transform.** `CreateRaster()` computes viewport transform with subpixel scaling (multiply by 256) and configures scissor rect.

**Step 3: Transform vertices.** Threads cooperatively transform cluster vertices to clip space and store subpixel coordinates:
- For non-programmable: simple transform to clip space, store `GroupVerts[VertIndex] = clip.xyz`
- For pixel programmable: transform + store full vertex attributes to a **64-entry rolling window cache** (`groupshared` arrays). Vertices are transformed in batches of 32.

**Step 4: Iterate triangles.** Triangles are processed in batches of up to 32 per iteration:
- Decode 3 vertex indices (`DecodeTriangleIndices` — handles compressed strip encoding)
- Load transformed vertices from groupshared cache
- `SetupTriangle()` with 256-subpixel precision
- `RasterizeTri_Adaptive()` to write pixels

**Step 5: Write VisBuffer.** For each covered pixel, `TNaniteWritePixel` atomically writes to the 64-bit visibility buffer. The `PixelValue` encodes:
```
PixelValue = ((VisibleIndex + 1) << 7) | TriIndex
```
`VisibleIndex + 1` ensures zero is never a valid value (0 = empty pixel). The low 7 bits encode the triangle index within the cluster (0-127).

### 3.4 Vertex Deduplication for Tessellation

For tessellation mode, vertices are deduplicated at the workgroup level using `DeduplicateVertIndexes()`:

1. Find smallest active vertex index in the wave
2. Subtract base from all vertex indices (relative indexing)
3. Build a 64-bit mask of used vertices across the wave
4. Compute prefix sums (`MaskedBitCount`) to map each unique vertex to a compact index
5. Use `FindNthSetBit` (binary search) for the reverse mapping

This reduces the number of vertices that need full transformation.

### 3.5 Sliding Window Vertex Cache

For pixel programmable shaders, a 64-entry sliding window cache avoids storing all 256 possible cluster vertices in groupshared memory:

1. Start with `NumCached = 0`
2. For each batch of up to 32 triangles:
   - Check if `MaxVertIndex >= NumCached`
   - If so, transform 32 new vertices (threads cooperate: each thread transforms one vertex)
   - Store to groupshared at `LaneVertIndex & 63` (mod-64 rolling window)
   - `NumCached += 32`
3. The invariant: no triangle can reference a vertex more than 32 indices back from the max-seen vertex index. This holds because clusters are built with at most 128 triangles and vertex indices are assigned sequentially.

### 3.6 Depth Testing and VisBuffer Write

`FVisBufferPixel.WriteOverdraw()` performs an atomic depth test:

```pseudocode
WriteOverdraw():
    DeviceZ_as_uint = asuint(DeviceZ)  // Reinterpret float depth as uint for atomic ops
    // Atomic: if new depth >= existing depth (inverted-Z), store
    InterlockedMax(VisBuffer[PixelPos], pack(PixelValue, DeviceZ_as_uint))
```

This is an **interlocked max** on the 64-bit VisBuffer entry (upper 32 bits = PixelValue, lower 32 bits = depth). The inverted-Z convention (far = 0.0, near = 1.0) means "closer" = larger depth value = wins the `InterlockedMax`.

If pixel programmable, after the depth test passes, `EvaluatePixel()` runs the material shader for alpha test, pixel depth offset, etc. If the pixel fails alpha test, the VisBuffer write is skipped.

## 4. Design Decisions / Tradeoffs

### 4.1 Subpixel Precision
- 8 bits of subpixel precision (256 samples per pixel) provides smooth sub-pixel positioning
- The 16.8 fixed-point format for vertices, 4.8 for edges, keeps all arithmetic in a stable range
- The 63-pixel rect limit ensures edge values never overflow: max edge value = 63 * 256 = 16128, which fits comfortably in float16 for the scanline cross-products

### 4.2 Adaptive Rasterization
- Rect rasterization is simpler/faster for small triangles (no scanline intersection math)
- Scanline rasterization skips empty portions of wide triangles, reducing work
- The threshold (width > 4 pixels) is chosen by wave-level vote

### 4.3 Fill Convention
- The top-left rule fill convention correction prevents double-hits on shared edges
- Using `saturate` instead of conditional branches avoids divergence
- Equivalent to: if edge is horizontal (dy == 0) and pointing left (dx > 0), OR edge is pointing down (dy > 0), subtract 1 from the edge constant

### 4.4 Vertex Cache Design
- The 64-entry rolling window cache is a key optimization: it avoids storing all 256 possible vertices while maintaining correctness
- Relies on the cluster builder's vertex ordering constraint (vertex index delta < 32)
- The double-buffer approach (two `CachedTransformedVerts[2]`) allows accessing vertices from both the current and previous batch

### 4.5 SW vs HW Rasterization Split
- Small triangles (edge length <= ~1 pixel): SW raster (compute shader, one workgroup per cluster)
- Large triangles: HW raster (mesh shader or vertex shader)
- The split is determined during cluster culling (`NANITE_CULLING_FLAG_USE_HW`)
- This avoids the fundamental inefficiency of HW raster for small triangles while maintaining HW raster's fill-rate advantage for large triangles

### 4.6 Atomic VisBuffer Write
- 64-bit atomic `InterlockedMax` on a single UAV is the core visibility resolution mechanism
- No depth buffer needed during rasterization — just the VisBuffer
- The packed format (PixelValue | Depth) enables a single atomic to resolve both visibility and identity

## 5. What Tumbler Phase 4D Should Adopt

### Adopt Directly:
- **Edge-function triangle setup** with subpixel precision (NANITE_SUBPIXEL_SAMPLES = 256)
- **Rect rasterizer** (`RasterizeTri_Rect`) for the initial implementation — simpler, correct
- **Top-left fill convention** — essential for watertight rasterization
- **64-bit atomic VisBuffer write** — `InterlockedMax` on `uint64` with depth in low 32 bits
- **PixelValue encoding**: `(VisibleIndex << 7) | TriIndex`
- **Per-cluster workgroup dispatch** — one workgroup per cluster (64 threads, 128 triangles)
- **Sliding window vertex cache** with the vertex ordering constraint
- **Depth plane interpolation** for per-pixel depth

### Simplify for Phase 4D:
- **Skip scanline rasterizer** — rect rasterizer is sufficient for small triangles
- **Skip tessellation** — not needed until displacement mapping is supported
- **Skip HW rasterization path** — all SW raster for Phase 4 (it's simpler)
- **Skip pixel programmable path** — Phase 4 can use fixed-function raster only (no alpha test, no PDO, no WPO)
- **Skip VSM page translation** — standard render targets only
- **Skip WPO/displacement** — basic static geometry only

### Implementation Notes:
1. Use `groupshared` memory for vertex cache (or direct register reads if triangle count is small)
2. The fixed-point vertex rebase trick is important for precision — adopt it
3. Incremental edge function evaluation (Pineda) is optimal for compute shaders — adopt the pattern
4. Consider using subgroup/wave operations for vertex deduplication if available
5. Ensure the cluster builder enforces the vertex ordering invariant (max vertex index delta < 32 within any group of 32 triangles)
