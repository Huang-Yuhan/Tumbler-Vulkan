# Nanite GPU Culling Pipeline (Phase 4C + 8)

UE5 source: `Runtime/Renderer/Private/Nanite/NaniteCullRaster.cpp`, `Shaders/Private/Nanite/NaniteClusterCulling.usf`, `NaniteInstanceCulling.usf`, `NaniteCulling.ush`, `NaniteHZBCull.ush`, `NaniteCullingCommon.ush`, `Shared/NaniteDefinitions.h`

## 1. Architecture Overview

Nanite's GPU culling is a multi-stage pipeline that progressively refines the set of visible geometry:

```
InstanceCull -> NodeCull (BVH Traversal) -> ClusterCull -> RasterBin
     |               |                           |
  Per-instance    Per-BVH-node              Per-cluster
  frustum+HZB     frustum+HZB              LOD selection
                                     + SW/HW raster binning
```

Each stage is a compute shader. The pipeline is organized into passes by **culling pass type**:

| Pass | Constant | Description |
|------|----------|-------------|
| No Occlusion | `CULLING_PASS_NO_OCCLUSION` (0) | No HZB available (first frame, shadow maps without HZB) |
| Occlusion Main | `CULLING_PASS_OCCLUSION_MAIN` (1) | Full frustum + HZB test; writes occluded instances to a list |
| Occlusion Post | `CULLING_PASS_OCCLUSION_POST` (2) | Re-tests previously occluded instances (using previous frame's HZB) |
| Explicit List | `CULLING_PASS_EXPLICIT_LIST` (3) | Explicit per-primitive draws (editor selection, etc.) |

The two-pass occlusion system works because the HZB from the previous frame is available early; occluded objects from the main pass get a second chance in the post pass against the current frame's HZB.

## 2. Key Data Structures

### 2.1 FCandidateNode

```cpp
struct FCandidateNode {
    uint Flags;                  // NANITE_CULLING_FLAG_TEST_LOD, USE_HW, CACHE_AS_STATIC, etc.
    uint ViewId;                 // Which view (for multi-view stereo/shadows)
    uint InstanceId;             // GPU scene instance index
    uint NodeIndex;              // BVH root node index for this instance
    uint AssemblyTransformIndex; // Transform for assembly parts (NANITE_MAX_ASSEMBLY_TRANSFORMS if unused)
    uint EnabledBitmask;         // NANITE_BVH_NODE_ENABLE_MASK (all 4 children enabled)
};
```

Packed into 2-4 uint32s (variable size: 2 for main pass without assembly, 3 for post pass, 4 with assembly data). `0xFFFFFFFF` is reserved as invalid sentinel (avoided by leaving 1 unused bit in each field).

### 2.2 FFrustumCullData

```cpp
struct FFrustumCullData {
    float3 RectMin;             // NDC rectangle min (xy) and depth (z)
    float3 RectMax;             // NDC rectangle max
    bool   bCrossesFarPlane;    // Cluster crosses the far plane
    bool   bCrossesNearPlane;   // Cluster crosses the near plane (requires special handling)
    bool   bFrustumSideCulled;  // Culled by frustum side planes
    bool   bIsVisible;          // Final visibility after frustum test
};
```

### 2.3 FScreenRect

```cpp
struct FScreenRect {
    int4  Pixels;               // Integer pixel rect [min.xy, max.zw] inclusive on both ends
    bool  bOverlapsPixelCenter; // False if rect falls between pixel centers
    int4  HZBTexels;            // HZB texel coordinates at selected mip level
    int   HZBLevel;             // Selected HZB mip level
    float Depth;                // Maximum depth of the object (for depth test: Rect.Depth >= HZBDepth)
};
```

### 2.4 FQueueState / FQueuePassState

(Described in Topic 2 — the same structures are used across culling and traversal.)

## 3. Culling Algorithm Detail

### 3.1 Instance Culling (NaniteInstanceCulling.usf)

**Workgroup**: 64 threads per workgroup. Each thread processes one instance.

**Step 1: Load instance data.** Each thread loads `FInstanceSceneData` (local bounds, transform, primitive ID, Nanite resource ID) and `FPrimitiveSceneData` (visibility flags, material flags).

**Step 2: Visibility pre-filtering.** Check:
- `InstanceFlags & HIDDEN` — immediately skip
- `NaniteRuntimeResourceID == 0xFFFFFFFF` — skip non-Nanite instances
- `IsPrimitiveShown()` — editor/game visibility, shadow casting, scene capture, lighting channel, owner visibility

**Step 3: Distance culling.** Compute screen-space bounds from instance `LocalBoundsCenter/LocalBoundsExtent` and apply:
- `Cull.Distance()` — draw distance / cull distance (per-primitive settings)
- `Cull.GlobalClipPlane()` — global clip plane test

**Step 4: Frustum + HZB occlusion test.** `Cull.FrustumHZB()` performs the combined test. The frustum test is a **box-frustum** test (AABB in clip space) using `BoxCullFrustum()` or `BoxCullFrustumPerspective()`. The HZB test uses `GetScreenRect()` + `IsVisibleHZB()`.

**Step 5: Output.** If visible AND not occluded:
- Atomically increment `NodeWriteOffset` in the queue state
- Store a `FCandidateNode` with NodeIndex=0 (root) into the CandidateNodes buffer
- If occluded (main pass only): write to `OutOccludedInstances` for retesting in post pass

**Step 6: Imposter handling.** If instance has an imposter and screen rect is smaller than `ImposterMaxPixels`, draw imposter directly (write to VisBuffer) and skip culling the cluster hierarchy.

### 3.2 Box-Frustum Test (SphereCullFrustum / BoxCullFrustum)

Nanite uses two frustum test formulations depending on the projection:

**Perspective projection** (`BoxCullFrustumPerspective`):
- Transforms the 8 AABB corners to clip space
- Uses an ISOLATE-based evaluation: 4 passes of 2 corners each (to reduce VGPR pressure on GPU)
- Each pass: compute `PlanesMin = min(PlanesMin, float4(PC.xy, -PC.xy) - PC.w)` for the 6 frustum planes
- Side culling: `any(PlanesMin > 0)` = all corners outside at least one plane
- Near/far: `MinW <= MaxZ` (crosses near), `MaxW > MinZ` (behind near), `0 < MaxZ` (in front of far)
- If `MinW <= 0 && MaxW > 0` (crosses the eye plane): rect expands to full screen `[-1, 1]`

**Orthographic projection** (`BoxCullFrustumOrtho`):
- Simpler: transform center, compute delta = `abs(extent * axis_in_clip)`, rect = `[center - delta, center + delta]`
- 6 plane side culling: `any(rectMax.xy < -1 || rectMin.xy > 1)`

**Sphere-based frustum test** (`SphereCullFrustum`) — used for cluster LOD bounds:
- Uses the [Mara & Morgan 2013] analytic sphere projection to get precise 2D screen-space bounds of a sphere
- `ProjectSphere(x, z, r)` — analytically computes the min/max x-coordinate of a sphere projected to clip space
- Near-clip case uses `ProjectSphereNearClip()` for spheres crossing the near plane
- The result is a `float4(MinX, MinY, MaxX, MaxY)` used to build the screen-space rect for HZB

### 3.3 HZB Occlusion Test (NaniteHZBCull.ush)

The HZB test has three major steps:

**Step 1: Compute screen-space rectangle.**
`GetScreenRect()` maps NDC bounds to integer pixel coordinates:
```pseudocode
RectUV = saturate(CullRectMinMax.xy * (0.5, -0.5) + 0.5)
Pixels = RectUV * ViewSize + ViewRect + (0.5, 0.5, -0.5, -0.5)
// Clamp to viewport
Pixels.xy = max(Pixels.xy, ViewRect.xy)
Pixels.zw = min(Pixels.zw, ViewRect.zw - 1)
bOverlapsPixelCenter = all(Pixels.zw >= Pixels.xy)
```

**Step 2: Select HZB mip level.**
`MipLevelForRect()` uses `firstbithigh(RectPixels.zw - RectPixels.xy)` (a fast integer log2 approximation) to select the coarsest mip where the rect covers at most `DesiredFootprintPixels` (typically 4). The mip is incremented by 1 if the rect's quantized footprint at that level exceeds the threshold.

**Step 3: Sample HZB and test depth.**
`IsVisibleHZB()`:
- Gather 4 texels from the selected HZB mip (2x2 footprint) using `GatherLODRed` if available, or 4-point `SampleLevel` if not
- For 4x4 sampling: gather 4 groups of 4 texels = 16 samples, take the minimum depth
- Mask off texels outside the rect footprint (if rect is 1 pixel wide or tall)
- Test: `Rect.Depth >= MinHZBDepth` (inverted-Z buffer: object's maximum depth must be >= the minimum depth in the HZB tile)

**Plane-based HZB variant** (`IsVisibleHZB(FScreenRect, float3 PlaneHZB)`):
- For AABB geometry, the depth within the rect is a linear function (a plane in screen space)
- `GetFacePlaneHZB()` computes the dominant face plane of the AABB and transforms it to HZB space
- The plane test is: `min(PlaneDepth, Rect.Depth) >= HZBDepth` for each texel
- This is more accurate than using a single depth value for the entire AABB

### 3.4 Node Culling (BVH Traversal)

See Topic 2 for the persistent-thread traversal algorithm. The visibility test for each BVH node child uses:

1. **Frustum test**: The node's `LODBounds` sphere is tested against frustum planes via `SphereCullFrustum()`.
2. **HZB occlusion**: The resulting screen rect is tested against the HZB via `GetScreenRect()` + `IsVisibleHZB()`.
3. **LOD selection**: The projected screen-space radius of `LODBounds` is compared against a threshold.

The `ShouldVisitChild()` callback in the traversal shader orchestrates these tests:
```pseudocode
ShouldVisitChild(HierarchyNodeSlice, bVisible):
    if (!bVisible) return false
    if (!bEnabled) return false
    // Decode LOD error
    LODError = max(Slice.MinLODError, Slice.MaxParentLODError)
    // Compute projected error
    if LODError == 0:
        ProjectedError = 0  // leaf
    else:
        // compare screen-space projected radius to threshold
        if projected_radius < pixel_threshold:
            return false  // LOD sufficient, prune children
    return true
```

### 3.5 Cluster Culling

After BVH traversal produces a list of visible leaf clusters, the cluster culling pass:
1. Loads `FPackedCluster` data (8 float4s = 128 bytes)
2. Decodes cluster bounds and material info
3. Performs final frustum/HZB test at cluster granularity (tighter bounds than nodes)
4. Selects rasterization mode (SW vs HW) based on projected triangle edge length:
   - Small clusters (edge <= MaxPixelsPerEdge): Software rasterization (more efficient for small triangles)
   - Large clusters: Hardware rasterization
5. Writes visible clusters to `OutVisibleClustersSWHW` with raster bin assignments
6. Optionally writes streaming requests for pages not currently resident

### 3.6 Two-Pass Occlusion System

The main pass processes all instances with frustum + HZB (using the **previous frame's HZB**). Occluded instances are recorded. The post pass re-tests those occluded instances against the **current frame's HZB** (which includes visible geometry from the main pass). This two-pass approach ensures conservative culling when the HZB might be stale.

In the post pass:
- Frustum culling is skipped (`bSkipCullFrustum = true`) since instances already passed frustum in the main pass
- HZB test uses the updated HZB from the current frame
- Visible instances produce candidate nodes appended to the same queue (but at `QueueStateIndex = 1`)

## 4. CPU Dispatch Orchestration

`NaniteCullRaster.cpp` orchestrates the GPU culling passes via RDG (Render Dependency Graph):

```
1. InstanceHierarchyDriver.DispatchCullingPass()  // Scene-level instance pre-filter
   -> BuildInstanceWorkGroups()                   // Group instances by primitive/view
   
2. InstanceCull_CS (per-instance frustum + HZB)
   -> Output: CandidateNodes buffer (BVH roots)
   
3. InitNodeCullArgs (initialize indirect args for BVH traversal)
   -> Reads QueueState.NodeWriteOffset

4. NodeAndClusterCull_CS (persistent threads: BVH traversal + cluster culling)
   OR
   NodeCull_CS (per-level) + ClusterCull_CS (final pass)
   -> Output: VisibleClustersSWHW buffer
```

The **persistent thread** path (enabled by `r.Nanite.PersistentThreadsCulling`) is the preferred mode. When disabled, it falls back to per-level node culling with ping-pong indirect args.

## 5. Design Decisions / Tradeoffs

### 5.1 Sphere vs AABB Frustum Test
- **Spheres** are used for BVH nodes (4 floats per child, fast dot-product test, conservative)
- **AABBs** are used for instances and clusters (tighter bounds, requires box-to-NDC projection)
- The sphere test uses the analytic [Mara & Morgan 2013] method for precise projected bounds

### 5.2 Conservative Near-Clip Handling
- When a sphere crosses the near plane, its screen-space projection becomes complex (wraps around infinity)
- UE5 uses a near-clip variant (`SphereToScreenRectNearClip`) that clips the sphere analytically against the near plane, producing a finite screen rect
- This avoids the common bug of objects expanding to full screen when near the camera

### 5.3 Two-Pass HZB for Temporal Coherence
- Using the previous frame's HZB is critical for performance (no need to build HZB before starting culling)
- The post pass catches up any objects that became visible due to changes between frames
- This effectively tolerates one frame of occlusion latency for occluded objects

### 5.4 Imposter Integration within Culling
- Imposters are checked at instance culling time, not later
- If an instance's screen-space footprint is small enough, an imposter is drawn directly
- This short-circuits the entire BVH traversal for distant objects
- The imposter rasterizes directly into the VisBuffer at the instance culling stage

### 5.5 VGPR Management in Frustum Tests
- The perspective box-frustum test uses `ISOLATE` blocks to force the compiler to not keep all 8 corners live simultaneously
- This significantly reduces register pressure (each corner is 4 floats, so 8 corners = 32 floats = 32 VGPRs)
- The `EVAL_POINTS` macro evaluates 2 corners at a time, accumulating results

### 5.6 Workgroup-to-Instance Mapping
- Instance culling uses pre-built work group data from the hierarchical instance culling system (`FInstanceCullingGroupWork`)
- This groups instances with the same primitive ID and view mask into workgroups of 64
- Run-Length Encoding (RLE) compresses consecutive instance IDs
- Work items are dispatched in reverse order for non-static geometry (dynamic instances processed first to maximize early overlap)

## 6. What Tumbler Phase 4 Should Adopt

### Phase 4C (Immediate — Frustum Culling):
- **Sphere-frustum test** for clusters (simpler than box-frustum, adequate for Phase 4)
- **Single-pass instance+cluster culling** — single compute shader:
  1. Each workgroup processes N instances
  2. Instance frustum test → if visible, test all its clusters
  3. Output visible cluster list with raster bin selection
- **Skip HZB entirely** for Phase 4 — use only frustum culling
- **Skip imposter system** — not needed for Phase 4
- **Skip multi-view** — single view only
- **Skip two-pass occlusion** — no HZB means no occlusion testing

### Phase 4C (Simplify):
- **Instance culling**: test instance AABB against 6 frustum planes in clip space using the simpler orthographic method (or sphere test)
- **Cluster culling**: test each cluster's `SphereBounds` against frustum planes (`dot(plane, center) + radius < 0` = culled)
- **Use coherent (non-atomic) output writes** — workgroup-local scratch buffer, then wave-level compact

### Phase 8 (HZB Occlusion):
- **Adopt the two-pass occlusion system** with previous-frame HZB
- **Adopt `GetScreenRect()` + `MipLevelForRect()`** — the `firstbithigh` trick for mip selection is clever and performant
- **Adopt `IsVisibleHZB()`** with 4x4 sampling for tight occlusion tests
- **Adopt the plane-based HZB test** for AABB geometry (more accurate than single-depth test)
- **Consider using a depth pyramid instead of full HZB mip chain** — simpler, similar results for Phase 8

### Key simplifications for Phase 4:
1. **No HZB** — frustum-only culling
2. **No two-pass occlusion** — single pass
3. **No persistent threads** — simple indirect dispatch per culling stage
4. **No BVH traversal** — linear cluster iteration (all clusters are leaf clusters in Phase 4)
5. **No imposter integration** — all geometry goes through the cluster pipeline
6. **Structured buffer output** (packed cluster indices) instead of byte-addressable scatter
