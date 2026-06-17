# Nanite VisBuffer Decode + G-Buffer Export (Phase 4E/F)

UE5 source: `Shaders/Private/Nanite/NaniteExportGBuffer.usf`, `NaniteDepthExport.usf`, `NaniteAttributeDecode.ush`, `NaniteDataDecode.ush`, `NaniteShadeBinning.usf`, `NaniteWritePixel.ush`, `NaniteShading.cpp`

## 1. Architecture Overview

Nanite uses a **deferred material shading** pipeline. The key innovation is the **Visibility Buffer (VisBuffer)**, a 64-bit per-pixel buffer that stores geometry identity (which triangle, which cluster) and depth, rather than G-Buffer material properties. Material evaluation is deferred until after depth resolution.

The pipeline has four stages after rasterization:

```
VisBuffer (64-bit) → Depth Export (HTile + SceneDepth) → Shade Binning → Material Shading (G-Buffer)
```

**Stage 1: VisBuffer** — written by SW/HW rasterizer. Encodes cluster identity + triangle index + depth per pixel via atomic max.

**Stage 2: Depth Export** — compute shader reads VisBuffer, computes min/max depth per HTile, writes SceneDepth texture. HTile enables coarse depth culling for subsequent passes.

**Stage 3: Shade Binning** — compute shader reads VisBuffer, determines material ID per pixel, sorts pixels into bins by material ID. This enables coalesced material evaluation (same material = same shader = efficient wave utilization).

**Stage 4: Material Shading** — deferred per-material compute passes read the shade bins, decode vertex attributes from the cluster page data, interpolate at pixel barycentrics, evaluate materials, and write G-Buffer targets.

## 2. Key Data Structures

### 2.1 VisBuffer64 Pixel Format

```cpp
// 64-bit VisBuffer entry (R64_UINT format)
uint2 Packed = uint2(PixelValue, DepthInt);

// PixelValue encoding:
// Bits [6:0]   = Triangle Index (0-127 within cluster)
// Bits [30:7]  = VisibleClusterIndex + 1 (0 = empty pixel)
// Bit  [31]    = IsImposter flag (imposter pixels bypass normal decode)

// DepthInt: uint32 reinterpretation of float depth (inverted-Z)
```

`UnpackVisPixel()` extracts:
```pseudocode
VisibleClusterIndex = (Packed.x >> 7) - 1;  // 0xFFFFFFFF = empty pixel
TriIndex            = Packed.x & 0x7F;
DepthInt            = Packed.y;
```

The `+1` encoding ensures zero is never a valid cluster index, allowing 0 to represent "no geometry" without needing a separate clear.

### 2.2 FVisibleCluster

```cpp
struct FVisibleCluster {
    uint  Flags;                    // Culling flags (WPO enabled, fallback raster, cache as static, etc.)
    uint  ViewId;                   // Multi-view index
    uint  InstanceId;               // GPU scene instance index
    uint  PageIndex;                // Streaming page index
    uint  ClusterIndex;             // Cluster index within page
    uint  AssemblyTransformIndex;   // Assembly/instance transform
    uint  DepthBucket;              // Depth bucket for sorting
    uint2 vPage;                    // Virtual page (VSM)
    uint2 vPageEnd;                 // End virtual page (inclusive)
};
```

### 2.3 FCluster (GPU-side decoded)

```cpp
struct FCluster {
    uint  PageBaseAddress;          // Byte offset into the page data buffer
    uint  NumVerts;                 // Number of vertices (max 256)
    uint  PositionOffset;           // Byte offset to position data
    uint  NumTris;                  // Number of triangles (max 128)
    uint  IndexOffset;              // Byte offset to index data
    int3  PosStart;                 // Quantized position base
    uint  BitsPerIndex;             // Bits per index entry
    int   PosPrecision;             // Position quantization precision
    uint3 PosBits;                  // Bits per position component
    uint  NormalPrecision;          // Normal quantization bits
    uint  TangentPrecision;         // Tangent quantization bits
    float PosScale;                 // Dequantization scale
    float PosRcpScale;
    float4 LODBounds;               // Bounding sphere (xyz=center, w=radius)
    float3 BoxBoundsCenter;         // AABB center
    float  LODError;                // Simplification error
    float  EdgeLength;
    float3 BoxBoundsExtent;         // AABB half-extent
    uint   AttributeOffset;         // Byte offset to attribute data
    uint   BitsPerAttribute;        // Bits per attribute entry
    uint   DecodeInfoOffset;        // Offset to decode info (UV headers, bone influences)
    uint   NumUVs;                  // Number of UV channels
    // Material info (fast path: up to 3 materials, slow path: material table)
    uint   Material0Length;         // Triangles in material 0
    uint   Material0Index;          // Material ID of material 0
    uint   Material1Length;
    uint   Material1Index;
    uint   Material2Index;
    uint   MaterialTotalLength;
    // ... more fields
};
```

### 2.4 Shading Mask

```cpp
// Packed shading mask per pixel (output by DepthExport, consumed by ShadeBinning)
// Bits [3:0]   = ShadingBin (material bin index)
// Bits [6:4]   = LightingChannels
// Bit  [18]    = bIsDecalReceiver
// Bit  [19]    = bHasRayTracingRepresentation
// Bits [23:20] = ShadingRate (VRS)
uint PackShadingMask(ShadingBin, ShadingRate, bIsDecalReceiver, bHasRayTracingRepresentation, LightingChannels);
```

## 3. Algorithm Flow

### 3.1 VisBuffer Write (during rasterization)

`FVisBufferPixel.Write()` performs the atomic write:

```pseudocode
Write():
    Depth = saturate(Depth)
    DepthInt = asuint(Depth)
    // 64-bit atomic max: higher depth (closer) wins
    Pixel = PackUlongType(uint2(PixelValue, DepthInt))
    ImageInterlockedMaxUInt64(OutVisBuffer64, PixelPos, Pixel)
```

The **inverted-Z** convention (near=1.0, far=0.0) enables `InterlockedMax` to resolve closer depth. No separate depth buffer write needed — the VisBuffer is the sole output of rasterization.

### 3.2 Depth Export (NaniteDepthExport.usf)

This compute shader runs per 8x8 pixel tile, exporting SceneDepth and updating HTile metadata:

**Step 1: Read VisBuffer.** Each thread reads its pixel's VisBuffer64 entry and unpacks it.

**Step 2: Depth resolve.** For each pixel:
```pseudocode
NaniteDepthValue = asfloat(DepthInt)
NanitePixelVisible = NaniteDepthValue >= SceneDepthValue  // Inverted-Z: higher = closer
if NanitePixelVisible:
    // Decode cluster identity
    VisibleCluster = GetVisibleCluster(VisibleClusterIndex)
    InstanceData = GetInstanceSceneData(VisibleCluster)
    PrimitiveData = GetPrimitiveData(InstanceData.PrimitiveId)
    Cluster = GetCluster(VisibleCluster.PageIndex, VisibleCluster.ClusterIndex)
    // Determine material bin
    ShadingBin = GetMaterialShadingBin(Cluster, PrimitiveId, MeshPassIndex, TriIndex)
    // Write depth + stencil
    SceneDepthValue = NaniteDepthValue
    // Export shading mask (for shade binning)
    ShadingMask = PackShadingMask(ShadingBin, ...)
    // Export velocity (if WPO not enabled)
    if !bWPOEnabled:
        Velocity = CalculateNaniteVelocity(...)
```

**Step 3: Per-tile reduction.** Using wave operations, compute min/max depth across all 64 pixels in the 8x8 tile:
```pseudocode
TileMinDepth = WaveActiveMin(SceneDepthValue)  // Far (inverted-Z)
TileMaxDepth = WaveActiveMax(SceneDepthValue)  // Near (inverted-Z)
if WaveIsFirstLane:
    SceneHTile[TileIndex] = EncodeTileMinMaxDepth(TileMinDepth, TileMaxDepth)
```

**Step 4: Scalarized write.** Depth and stencil values are written per-pixel using scalar memory operations (optimized for GPU memory hierarchy).

The HTile metadata enables subsequent passes (like lighting) to skip tiles with no geometry.

### 3.3 Shade Binning (NaniteShadeBinning.usf)

This multi-pass compute pipeline sorts pixels by material ID into shade bins:

**Pass 1: Count pixels per bin.**
Each thread reads VisBuffer, unpacks pixel, determines material bin. Uses wave-level atomic increments on per-bin counters: `InterlockedAdd(OutShadingBinScatterCounters[Bin].LooseElementCount, WavePixelCount)`.

**Pass 2: Reserve space.** Allocates output space for each bin based on counts. Computes prefix sums to determine where each bin's data starts.

**Pass 3: Scatter.** Each thread writes its pixel data (position, depth, cluster ID, triangle index) to the bin's allocated region in the shade bin buffer.

**Pass 4: Validate.** Verifies binning correctness (debug only).

Key algorithm detail -- **BinScalarization**: Since a single pixel can belong to only one material bin, but different threads may have different bins, the shader "scalarizes" by having all threads vote on a common bin and process matching threads together. This avoids thread divergence:

```pseudocode
while WaveActiveAnyTrue(IsValidBin(MaxBin)):
    VotedBin = WaveReadLaneFirst(MaxBin)  // All threads agree on one bin
    for i in 0..3:  // 4 pixels per thread (quad mode)
        if ShadingBins[i] == VotedBin:
            WritePixelToBin(ShadeBinData, VotedBin, pixelData)
            ShadingBins[i] = INVALID  // Mark as processed
    MaxBin = max(ShadingBins[0..3])
```

### 3.4 Material Shading / G-Buffer Export

The deferred material shading computes per-pixel material properties from the VisBuffer identity and writes to G-Buffer targets. This happens in a compute shader (not the `NaniteExportGBuffer.usf` pixel shader which handles depth/stencil export only).

**Step 1: Parse shade bin.** Each workgroup processes one shade bin (all pixels with the same material). Each thread processes one quad (2x2 pixels).

**Step 2: Decode VisBuffer per pixel.**
```pseudocode
VisibleClusterIndex = ShadePixelData.ClusterIndex
TriIndex = ShadePixelData.TriIndex
VisibleCluster = GetVisibleCluster(VisibleClusterIndex)
```

**Step 3: Load cluster and instance data.**
```pseudocode
Cluster = GetCluster(VisibleCluster.PageIndex, VisibleCluster.ClusterIndex)
InstanceData = GetInstanceSceneData(VisibleCluster)
PrimitiveData = GetPrimitiveData(InstanceData.PrimitiveId)
```

**Step 4: Decode vertex indices.**
```pseudocode
VertIndexes = DecodeTriangleIndices(Cluster, TriIndex)
// Handles compressed strip encoding (see Topic 6)
```

**Step 5: Compute barycentrics.** Need to reconstruct barycentric coordinates from the pixel position. Since Nanite doesn't store barycentrics per pixel, they are computed from the depth using the triangle's depth plane:
```pseudocode
// From the VisBuffer pixel, we have:
// - Depth (from VisBuffer)
// - Pixel position
// - Triangle vertex positions (from decoded cluster data)

// Reconstruct using the rasterization edge functions in reverse:
// C0, C1, C2 = edge function values at this pixel (from triangle setup)
// Barycentrics = GetPerspectiveCorrectBarycentrics(C, Tri.InvW)
```

In practice, the G-Buffer export uses the `CalculateBarycentrics()` function from `NaniteRasterizationCommon.ush`:
```pseudocode
// Re-derive edge function values from pixel position vs triangle edges
float3 C = { Edge12.eval(pixel), Edge20.eval(pixel), Edge01.eval(pixel) }
float3 UVW = GetPerspectiveCorrectBarycentrics(C, Tri.InvW)
// UVW = perspective-correct barycentric coordinates
```

**Step 6: Decode vertex attributes.** For each vertex, decode compressed attributes:
- **Position**: Decode quantized position from `PosStart + encoded * PosScale`
- **Normal**: Octahedron decode from quantized bits
- **Tangents**: Quantized decode + tangent frame reconstruction
- **UVs**: Decode from quantized float format (`DecodeUVFloat`)
- **Color**: Decode from quantized 8-bit per channel

**Step 7: Interpolate attributes.** Using the barycentric coordinates:
```pseudocode
InterpolatedNormal = normalize(Verts[0].Normal * UVW.x + Verts[1].Normal * UVW.y + Verts[2].Normal * UVW.z)
InterpolatedUV = Verts[0].UV * UVW.x + Verts[1].UV * UVW.y + Verts[2].UV * UVW.z
// ... etc for all attributes
```

**Step 8: Evaluate material.** Run the material shader (standard UE5 material graph) to produce:
- BaseColor, Metallic, Specular, Roughness (PBR parameters)
- Normal (tangent-space or world-space)
- Emissive
- etc.

**Step 9: Write G-Buffer.** Output to MRT targets:
- GBufferA: WorldNormal + PerObjectGBufferData
- GBufferB: Metallic + Specular + Roughness + ShadingModelID
- GBufferC: BaseColor
- GBufferD/E: Custom data, PrecomputedShadowFactors, etc.

## 4. Key Decode Functions

### 4.1 Normal Decode (Octahedron)

`UnpackNormal(uint Packed, uint Bits)`:
```pseudocode
Mask = (1 << Bits) - 1
F.xy = (extract(Packed, Bits, 0), extract(Packed, Bits, Bits)) * (2.0 / Mask) - 1.0
N.xy = F.xy
N.z = 1.0 - abs(F.x) - abs(F.y)
// Hemioct correction (reflect points outside the octahedron)
T = saturate(-N.z)
N.xy += select(N.xy >= 0, -T, T)
return normalize(N)
```

### 4.2 UV Decode (Custom Float)

`DecodeUVFloat(uint EncodedValue, uint NumMantissaBits)`:
```pseudocode
// Compact float encoding with configurable mantissa bits
// Supports both normalized [0,1] and signed range
ExponentAndMantissaMask = BitFieldMask(NANITE_UV_FLOAT_NUM_EXPONENT_BITS + NumMantissaBits, 0)
bNeg = (EncodedValue <= ExponentAndMantissaMask)
ExponentAndMantissa = (bNeg ? ~EncodedValue : EncodedValue) & ExponentAndMantissaMask
Result = asfloat(0x3F000000u + (ExponentAndMantissa << (23 - NumMantissaBits)))
Result = min(Result * 2.0 - 1.0, Result)  // Stretch denormals to [0,1]
return bNeg ? -Result : Result
```

### 4.3 Material Range Decode

`DecodeMaterialRange(uint EncodedRange, out TriStart, out TriLength, out MaterialIndex)`:
```pseudocode
TriStart      = BitFieldExtract(EncodedRange, 8, 0)   // max 128 triangles
TriLength     = BitFieldExtract(EncodedRange, 8, 8)   // max 128 triangles  
MaterialIndex = BitFieldExtract(EncodedRange, 6, 16)  // max 64 materials per cluster
```

For the **fast path** (1-3 materials): the cluster directly encodes `Material0Index`, `Material0Length`, etc. This covers the common case of clusters with few materials without the indirection of the material table.

## 5. Design Decisions / Tradeoffs

### 5.1 VisBuffer vs Traditional G-Buffer
- **Pro**: Single atomic 64-bit write per pixel during rasterization (vs 5+ MRT writes)
- **Pro**: No overdraw cost for material evaluation (shaded exactly once after depth resolve)
- **Pro**: Material coalescing (sort by material ID before shading) improves wave utilization
- **Con**: Requires an extra pass to decode materials
- **Con**: Needs per-pixel barycentric reconstruction (floating-point precision must match rasterizer)

### 5.2 HTile Depth Export
- Using hardware HTile metadata accelerates subsequent depth tests (tile-level rejection)
- The 8x8 tile reduction uses wave operations for efficient min/max across 64 threads
- Nanite pixels that pass the depth test overwrite HTile metadata
- Non-Nanite (raster) depth is preserved when no Nanite pixels overwrite a tile

### 5.3 Shade Binning Scalarization
- The scalarization pattern (all threads agree on one bin) is critical for SIMD efficiency
- Without it, threads would diverge on material evaluation, destroying wave utilization
- Works because materials are grouped: nearby pixels on the same object typically share materials
- The quad-based dispatch (2x2 pixels per thread) maps naturally to texture sampling quads

### 5.4 Inverted-Z Convention
- Near = 1.0, Far = 0.0
- Enables `InterlockedMax` for depth testing (closer = larger value)
- Consistent with modern GPU depth buffer conventions
- Requires `Depth = saturate(Depth)` to clamp near-clipped vertices

### 5.5 Fast vs Slow Material Path
- **Fast path**: Cluster has 1-3 materials, encoded directly in cluster header. Single triangle range lookup.
- **Slow path**: Material table with per-triangle material indices (supports up to 64 materials). Uses indirection via `MaterialTableOffset`.
- 99%+ of clusters use fast path (clusters are small and spatially coherent)

## 6. What Tumbler Phase 4E/F Should Adopt

### Phase 4E (Depth Export):
- **64-bit VisBuffer** — adopt the exact format: `(VisibleClusterIndex+1) << 7 | TriIndex` in high 32 bits, depth in low 32 bits
- **Compute-based depth export** — per 8x8 tile, compute min/max depth via wave ops
- **Atomic Max for depth resolve** — `InterlockedMax` on uint64
- **SceneDepth write** — simple float buffer, no HTile metadata needed for Phase 4

### Phase 4F (Shade Binning + G-Buffer):
- **Shade binning** — count + scatter pass for material coalescing
- **Deferred material evaluation** — compute shader reads VisBuffer, decodes attributes, evaluates material, writes G-Buffer
- **Barycentric reconstruction** — from pixel position + triangle edges (re-derive C values)
- **Attribute interpolation** — using reconstructed barycentrics
- **G-Buffer layout** — standard deferred shading GBuffer (Albedo, Normal+Roughness, Metal+AO+Specular)

### Phase 4E/F (Simplify):
- **Single material per cluster** for Phase 4 — skip shade binning entirely (just process all visible clusters in one pass)
- **Skip HTile** — not needed for initial implementation
- **Skip velocity export** — add in later phase
- **Skip VSM page translation** — standard render targets only
- **Skip stencil / decal support** — Phase 4 doesn't need it
- **Skip multi-material fast/slow path** — single material per cluster, direct per-triangle index
- **Skip VRS** — all pixels at 1x1 rate
- **Simplified attribute decode** — fewer UV channels, no bone influences, no vertex color

### Key observations for Phase 4:
1. The VisBuffer approach is the core architectural decision — adopt it fully
2. Depth export can be a simple per-pixel compute pass (no tile optimization needed)
3. Shade binning is optional for single-material-per-cluster; direct compute dispatch works
4. Barycentric reconstruction from the VisBuffer requires using the same edge function math as the rasterizer
5. The most challenging part is matching barycentric precision between rasterizer and material shader — use identical math
