# Nanite Vertex Compression Encoding (Phase 9)

UE5 source: `Developer/NaniteBuilder/Private/Encode/NaniteEncode.cpp`, `NaniteEncodeGeometryData.cpp`, `Shaders/Private/Nanite/NaniteTranscode.usf`, `NaniteAttributeDecode.ush`, `Shared/NaniteDefinitions.h`

## 1. Architecture Overview

Nanite compresses vertex data in a multi-stage pipeline. Positions, normals, tangents, UVs, and colors are all quantized to variable bit-widths chosen per-cluster. The compression is aggressive: typical vertex data is reduced by 4-8x.

The pipeline has three levels of encoding:
1. **Cluster-level quantization** — positions relative to cluster bounds, normals via octahedron
2. **Triangle strip encoding** — indices encoded as restart + delta references to minimize data
3. **Page-level bit-packing** — attributes packed contiguously at variable bit widths, aligned to byte boundaries

Key constants:
- `NANITE_MAX_POSITION_QUANTIZATION_BITS = 21` (per component, 63 bits total for 3 components)
- `NANITE_MAX_NORMAL_QUANTIZATION_BITS = 15` (2 components = 30 bits via octahedron)
- `NANITE_MAX_TANGENT_QUANTIZATION_BITS = 12` (+1 sign bit = 13 bits)
- `NANITE_MAX_COLOR_QUANTIZATION_BITS = 8` (8 bits per channel = 32 bits)
- `NANITE_UV_FLOAT_NUM_EXPONENT_BITS = 5`, `NANITE_UV_FLOAT_MAX_MANTISSA_BITS = 14` (max 20 bits per component)
- `NANITE_MAX_CLUSTER_VERTICES = 256`, `NANITE_MAX_CLUSTER_TRIANGLES = 128`

## 2. Position Quantization

### 2.1 Per-Mesh Precision Selection

The position precision (bits of sub-texel resolution) is determined per-mesh:

```pseudocode
if PositionPrecision == AUTO:
    // Heuristic: denser meshes need higher precision
    for each leaf cluster:
        TotalLogSize += log2(Cluster.Bounds.GetExtent().Size())
    AvgLogSize = TotalLogSize / num_leaf_clusters
    PositionPrecision = 7 - round(AvgLogSize)
    PositionPrecision = max(PositionPrecision, 4)  // 1/16 cm minimum

QuantizationScale = 2^PositionPrecision  // World-space units per quantized step
```

### 2.2 Per-Cluster Quantization

For each cluster:

```pseudocode
// Step 1: Quantize all positions to integer grid
for each vertex:
    IntPos = round(Position * QuantizationScale)

// Step 2: Find per-cluster min/max
IntClusterMin = min(all IntPos)
IntClusterMax = max(all IntPos)

// Step 3: Compute required bit widths
NumBitsX = ceil(log2(IntClusterMax.X - IntClusterMin.X + 1))
NumBitsY = ceil(log2(IntClusterMax.Y - IntClusterMin.Y + 1))
NumBitsZ = ceil(log2(IntClusterMax.Z - IntClusterMin.Z + 1))
// Each <= 21 bits

// Step 4: Store as delta from min (relative encoding)
for each vertex:
    QuantizedPos[i] = IntPos[i] - IntClusterMin[i]

// Step 5: Recompute float positions from quantized data (lossy)
Position[i] = QuantizedPos[i] * (1.0 / QuantizationScale)

// Step 6: Store metadata
Cluster.QuantizedPosStart  = IntClusterMin
Cluster.QuantizedPosBits   = (NumBitsX, NumBitsY, NumBitsZ)
Cluster.QuantizedPosPrecision = PositionPrecision
```

The position `PosScale` and `PosRcpScale` in the GPU-side `FCluster` are:
- `PosScale = 2^(-PositionPrecision)` = world-space distance per quantized unit
- `PosRcpScale = 2^(PositionPrecision)` = quantized units per world-space unit

### 2.3 GPU-Side Decode

Position decode (in `NaniteDataDecode.ush`):
```pseudocode
// Read variable-bit-length position components from packed data
IntPosX = ReadBits(PageBuffer, PositionOffset, NumBitsX)
IntPosY = ReadBits(PageBuffer, PositionOffset + NumBitsX, NumBitsY)
IntPosZ = ReadBits(PageBuffer, PositionOffset + NumBitsX + NumBitsY, NumBitsZ)
WorldPos = (PosStart + int3(IntPosX, IntPosY, IntPosZ)) * PosScale
```

### 2.4 Error Bounds Verification

The encoder validates that positions fit in the 21-bit range. If any cluster exceeds this, `QuantizationScale` is halved (precision reduced by 1 bit) and all positions are re-quantized. This loop ensures robustness against extreme cases.

## 3. Normal Encoding (Octahedron)

### 3.1 Octahedron Mapping

Normals are projected onto an octahedron and mapped to 2D coordinates:

```pseudocode
OctahedronEncode(N):
    N /= abs(N.x) + abs(N.y) + abs(N.z)  // Project to octahedron
    if N.z < 0:  // Fold lower hemisphere
        N.xy = (N.x >= 0 ? 1-abs(N.y) : abs(N.y)-1,
                N.y >= 0 ? 1-abs(N.x) : abs(N.x)-1)
    return (N.x, N.y)  // Range: [-1, 1]

OctahedronDecode(Packed, Bits):
    Mask = (1 << Bits) - 1
    f.xy = extract(Packed, low, high) * (2.0 / Mask) - 1.0
    f.z = 1.0 - abs(f.x) - abs(f.y)
    t = saturate(-f.z)
    f.xy += select(f.xy >= 0, -t, t)  // Hemioct correction
    return normalize(f.xyz)
```

### 3.2 Precise Encoding with Sub-Quantization Correction

The basic octahedron encode is extended with a **sub-quantization correction** (`OctahedronEncodePreciseSIMD`):

```pseudocode
// Test all 4 rounding combinations (floor vs ceil) for both X and Y
BaseX = floor(Coord.X * Scale + Bias)
BaseY = floor(Coord.Y * Scale + Bias)
for OffsetX in {0, 1}:
    for OffsetY in {0, 1}:
        TestX = BaseX + OffsetX
        TestY = BaseY + OffsetY
        DecodedNormal = OctahedronDecode(TestX, TestY, Bits)
        Error = abs(1.0 - dot(DecodedNormal, OriginalNormal))
        Keep the pair with minimum error
```

The SIMD version evaluates all 4 candidates in vector registers, computing the dot product of each decoded normal against the original and picking the winner. This reduces worst-case angular error by up to 4x compared to simple rounding.

### 3.3 Bit Allocation

Normal precision is configurable (typically 10-12 bits). With 10 bits: `(2*10=20 bits)` per normal. The encoded value is `X | (Y << Bits)`.

## 4. Tangent Encoding

Tangents are encoded as an angle around the normal:

```pseudocode
PackTangent(TangentX, TangentZ, NumBits):
    // Step 1: Reduce to 2D problem
    // Build orthonormal basis from TangentZ
    RefX = normalize(cross(TangentZ, up))
    RefY = cross(TangentZ, RefX)
    // Project TangentX into this 2D basis
    x = dot(TangentX, RefX)
    y = dot(TangentX, RefY)
    // Step 2: Quantize the angle
    Angle = atan2(y, x)  // Range: [-PI, PI]
    if Angle < 0: Angle += 2*PI
    QuantizedAngle = round(Angle / (2*PI) * (1 << NumBits))
```

The sign of the tangent frame handedness (bitangent sign) is encoded as a separate 1-bit value. Total: `1 + TangentPrecision` bits.

## 5. UV Encoding (Custom Float)

UVs use a custom floating-point format with configurable mantissa bits:

```pseudocode
// Encode float to custom format
DecodeUVFloat(Encoded, NumMantissaBits):
    Mask = BitFieldMask(5 + NumMantissaBits, 0)  // 5 exponent bits
    bNeg = (Encoded <= Mask)  // Sign-magnitude: all 1s with no set bits above mask = negative
    Value = (bNeg ? ~Encoded : Encoded) & Mask
    Result = asfloat(0x3F000000u + (Value << (23 - NumMantissaBits)))
    // 0x3F000000 = 0.5 in float (exponent = 126, mantissa = 0)
    Result = min(Result * 2.0 - 1.0, Result)  // Stretch [0.5, 1.0] to [0.0, 1.0]
    return bNeg ? -Result : Result
```

This format supports:
- Signed values (via sign-magnitude encoding)
- Denormalized range [0, 1] smoothly (no gap at zero)
- Up to 14 mantissa bits (~4 decimal digits of precision)
- Symmetric encoding (negative and positive values use the same encoding, just sign bit inverted)

### UV Range Optimization

Per-UV-channel, the minimum value is stored as a `UVHeader.Min` and subtracted before encoding. This shifts the encoded range to `[0, Max-Min]` instead of potentially wasting bits on negative values. The number of bits per component is also stored per-channel: `UVHeader.NumBits`.

## 6. Color Encoding

Colors are quantized to 8 bits per channel (RGBA) with per-cluster color min:

```pseudocode
ColorMin = min(per component across all vertices)
ColorBits = bits needed to represent range (typically 8 per channel)

EncodedColor.R = round((VertexColor.R - ColorMin.R) * 255)
// ... similar for G, B, A
```

The GPU decode applies `ColorMin + EncodedColor / 255 * ColorMax`. When all vertices have the same color, `ColorMode = CONSTANT` and zero bits are stored.

## 7. Triangle Strip Index Encoding

### 7.1 Generalized Triangle Strip

Nanite encodes triangle indices as a **generalized triangle strip** with two types of triangles:

- **Start triangles (S)**: Define a new triangle with 0-3 reference vertices (indices from previously emitted vertices) and 3-0 new vertices.
- **Continuation triangles (L/R)**: Left or right continuation. Reference 1-2 existing vertices, add 0-1 new vertices.

Each triangle is described by 3 bitmasks per 32-bit dword:
1. **SMask** (`StripBitmasks.x`): 1 = start triangle, 0 = continuation
2. **LMask** (`StripBitmasks.y`): 1 = left continuation, 0 = right continuation (only meaningful when SMask=0)
3. **WMask** (`StripBitmasks.z`): 1 = vertex is a reference (index from existing vertices), 0 = new vertex

This allows an arbitrary triangle strip to be encoded:
- S with 3 refs: reuse 3 existing vertices (degenerate index buffer compression)
- S with 0 refs: 3 new vertices (first triangle)
- L with 1 ref, 1 new: common strip continuation
- etc.

### 7.2 Reference Encoding

Reference vertices use 5-bit offsets from the base vertex (`NumPrevNewVertices - 1`):
```pseudocode
RefVertexIndex = BaseVertex - (RefData & 31)
```
This limits references to the last 32 emitted vertices, which is sufficient because the cluster builder ensures vertex ordering locality.

### 7.3 Pre-computed Prefix Sums

`NumPrevRefVerticesBeforeDwords` and `NumPrevNewVerticesBeforeDwords` encode prefix sums of reference/new vertex counts for each 32-bit dword. This allows O(1) random access to any triangle without decoding the entire strip:
```pseudocode
TotalRefsBefore = NumPrevRefVerticesBeforeDword[DwordIndex] + localRefsBeforeBit
TotalNewsBefore = NumPrevNewVerticesBeforeDword[DwordIndex] + localNewsBeforeBit
```

### 7.4 GPU-Side Decode

`UnpackStripIndices()` in `NaniteTranscode.usf` implements the full decode logic (~100 lines of shader code). The key insight: for start triangles, the number of reference vertices determines how many 5-bit ref indices are read. For continuation triangles, the third vertex is found by searching backwards for the matching left/right partner.

## 8. Page-Level Encoding

### 8.1 Page Format

GPU pages contain packed cluster data:

```
[PageGPUHeader: 16 bytes]
[Cluster headers: N * sizeof(FPackedCluster)]
[Vertex position data (variable bit-width per cluster)]
[Index data (variable bit-width strip encoding)]
[Vertex attribute data (normals, tangents, UVs, colors, packed at BitsPerAttribute)]
[Decode info (UV headers, bone influence headers)]
[Brick data for voxel clusters]
```

### 8.2 Transcoding

When streaming brings a new page to GPU memory, `NaniteTranscode.usf` decompresses from the disk format to the GPU-consumable format. The transcode process:

1. **Read page disk header** — `NumClusters`, `NumRawFloat4s`, offsets
2. **Read cluster disk header** — byte offsets for each data section
3. **Decompress positions** — decode variable-bit zigzag deltas, reconstruct per-vertex positions
4. **Decompress indices** — decode triangle strip into flat index buffer
5. **Decompress/transcode attributes** — read variable-bit attribute data, transcode to format expected by current GPU
6. **Write cluster headers** — populate `FPackedCluster` structs at fixed offsets for fast GPU read

### 8.3 Variable-Byte Deltas

Position/index data on disk uses **variable-byte delta encoding**. Values are split into low/mid/high bytes based on the bit-width needed per value:

```pseudocode
// Encode: if BytesPerDelta=2, write low byte to LowBytes stream, high byte to MidBytes stream
// Decode (GPU):
int4 values = 0
if BytesPerDelta >= 1: values |= ReadBytes<4>(LowBytesOffset, index)
if BytesPerDelta >= 2: values |= ReadBytes<4>(MidBytesOffset, index) << 8
if BytesPerDelta >= 3: values |= ReadBytes<4>(HighBytesOffset, index) << 16
// Zigzag decode + inclusive prefix sum
for each component:
    delta = DecodeZigZag(values[i])  // (-1,1,-2,2...) -> (0,1,0,2...)
    value[i] = WaveInclusivePrefixSum(delta) + PrevLastValue
PrevLastValue = WaveReadLaneLast(value)
```

This uses wave-level prefix sums for efficient parallel decompression within a workgroup.

## 9. Design Decisions / Tradeoffs

### 9.1 Per-Cluster Variable Bit Width
- Each cluster independently selects optimal bit widths for positions (based on bounds spread)
- Heterogeneous clusters: a flat wall cluster might use 4+4+1 bits; a detailed hero prop might use 16+15+12
- Tradeoff: more header data (PosStart, PosBits per cluster) vs better compression

### 9.2 Octahedron Normal Encoding
- 2-component encoding (vs 3 for spherical) saves 33% bits
- The sub-quantization correction (4-candidate evaluation) significantly reduces worst-case error
- The SIMD version evaluates all 4 candidates in parallel using vector operations
- GPU decode is also 2-component: `UnpackNormal()` matches the CPU decoder exactly

### 9.3 Tangent Angle Encoding
- Only an angle around the normal is stored (1D), not 2 components
- Tangent X can be derived from Normal + Angle, Tangent Y from cross product
- 12 bits for angle (~0.09 degree precision) + 1 sign bit = 13 bits total
- The XZ swap trick handles the degenerate case when the normal points mostly along Z

### 9.4 Custom UV Float Format
- Better than fixed-point: handles both tiny UV ranges (lightmaps) and large UV ranges (tiling textures)
- 5 exponent bits + configurable mantissa = adaptive precision
- The sign-magnitude trick avoids separate sign bits
- Denormal range smoothness avoids precision issues near zero

### 9.5 Triangle Strip with Random Access
- Generalized strip (not just sequential triangles) allows efficient random access
- Prefix sums enable O(1) seek to any triangle
- 5-bit reference offsets limit the window but work with vertex locality
- The cluster builder ensures vertex ordering to satisfy the 32-vertex window constraint

### 9.6 Page Transcoding vs Direct Encoding
- Disk format is optimized for size (variable-byte, zigzag deltas)
- GPU format is optimized for read speed (fixed offsets, aligned data)
- Transcode happens once when a page is streamed in (amortized over many frames)
- The relative encoding chain is limited to 6 levels to prevent long decode dependency chains

## 10. What Tumbler Phase 9 Should Adopt

### Adopt:
- **Position quantization relative to cluster bounds** — variable bit-width per cluster component
- **Octahedron normal encoding** — proven 2-component format, well-documented
- **Generalized triangle strip** with random access prefix sums
- **UV float format** — or a simpler fixed-point alternative if UV ranges are small
- **Per-cluster variable bit widths** for all attributes

### Simplify for Phase 9:
- **Fixed precision** instead of per-cluster variable — simpler, good enough initially
- **Skip sub-quantization correction** for normals — simple rounding is adequate for first pass
- **Skip tangent angle encoding** — or use simpler spherical coordinates
- **No page transcoding** — stream directly in GPU-consumable format (Phase 9 is streaming; see Topic 7)
- **Skip color compression** if not needed
- **Fixed UV bits** (e.g., 16-bit fixed point) instead of custom float format

### Implementation Notes:
1. Position quantization is the highest-impact compression — adopt first
2. Octahedron normal encoding is straightforward to implement on GPU
3. The generalized strip encoding is complex — start with a flat index buffer for Phase 4, add compression in Phase 9
4. Ensure GPU decode math exactly matches CPU encode math (verification via round-trip testing)
5. Bit widths should be multiples of 8 for byte-aligned reads on GPU
