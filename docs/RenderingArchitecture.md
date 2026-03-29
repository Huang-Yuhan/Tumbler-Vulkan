# Tumbler Vulkan: Rendering Architecture

Tumbler has evolved from a monolithic Forward Renderer into a **Polymorphic Dual-Pipeline system** supporting both **Forward** and **Deferred** rendering paths. The architecture leverages modern Vulkan feature-sets—specifically Subpasses and Input Attachments—to maximize Mobile and Desktop bandwidth utilization.

## 1. Dual-Pipeline Strategy Pattern

The engine implements the Strategy Pattern via the `IRenderPipeline` interface. `VulkanRenderer` acts as the **coordinator**, holding a map of all pipeline implementations and delegating command recording at runtime:

```cpp
std::unordered_map<ERenderPath, std::unique_ptr<IRenderPipeline>> Pipelines;
```

At frame time, `SceneViewData::RenderPath` (an `ERenderPath` enum) selects which pipeline receives the `RecordCommands()` call. Both pipelines are initialized on startup and remain hot-swappable.

- **`FForwardPipeline`**: Traditional single-pass architecture. Geometry and Lighting are computed simultaneously within the same fragment shader (`pbr.frag`). Single Subpass, 1 Color + 1 Depth attachment.
- **`FDeferredPipeline`**: Decoupled shading model with a 2-Subpass RenderPass. Geometry is rendered first to a multi-render-target (MRT) G-Buffer (Subpass 0), followed by a full-screen Lighting pass that reads the G-Buffer via Vulkan `subpassInput` (Subpass 1).

## 2. Deferred Rendering: Memory & Bandwidth Optimizations

The Deferred pipeline adopts a highly optimized memory footprint designed specifically to prevent VRAM saturation and memory bandwidth throttling common in standard Position-Normal-Albedo G-Buffers.

### G-Buffer Allocation (Subpass 0: Geometry)
We allocate only **Two** explicit G-Buffer color attachments, packing properties tightly:
1. **Albedo Buffer** (`R8G8B8A8_UNORM`):
   - `R, G, B`: Base Color
   - `A`: Metallic (Packed)
2. **Normal Buffer** (`R16G16B16A16_SFLOAT`):
   - `R, G, B`: World Space Normal Vector
   - `A`: Roughness (Packed)
3. **Depth Buffer** (Reused Swapchain Depth `D32_SFLOAT_S8_UINT`):
   - World Position is **NOT** explicitly stored. Instead, the Lighting phase recovers World Position mathematically via Depth Unprojection using the `InvViewProj` matrix (stored in `SceneDataUBO`).

### Subpass Dependencies & Input Attachments (Subpass 1: Lighting)
Rather than writing G-Buffers out to System VRAM and reading them back in as sampled Textures in a separate RenderPass, `FDeferredPipeline` combines them into a single `VkRenderPass` containing two subpasses.

- The Lighting pass pulls the G-Buffers using Vulkan's `subpassInput` semantics (`VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT`).
- Due to the `VK_DEPENDENCY_BY_REGION_BIT` barrier configuration, Tile-Based Deferred Rendering (TBDR) GPUs (like Apple Silicon / Mobile Processors) can keep the G-Buffer entirely inside the extremely fast On-Chip memory (SRAM), skipping the heavy Main Memory round-trip entirely.
- All G-Buffer attachments use `storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE` — a deliberate choice to signal the driver that these intermediate results need never be flushed to VRAM.

## 3. Dynamic Material Compilation

Because the Shaders must diverge structurally between the two paths, the `FMaterial` engine handles Dual-Compilation transparently:
- A material automatically requests compilation for **both** pipelines if available.
- For Deferred Geometry, Color Blending is automatically disabled, and `colorWriteMask` ensures all packed floats remain intact via `VulkanPipelineBuilder`.
- At render time, `AppLogic` switches between pipelines via `ERenderPath` enumerators embedded in `SceneViewData`, and the correct `VkPipeline` is intrinsically bound from the requested material variant.

## 4. Command Buffer Lifecycle

`VulkanRenderer` is solely responsible for the **full lifecycle** of the main command buffer (`MainCommandBuffer`) each frame:

1. `vkResetCommandBuffer` — reset at the start of each frame.
2. `IRenderPipeline::RecordCommands()` — opens the CB (`vkBeginCommandBuffer`), records all draw commands, and ends the RenderPass (`vkCmdEndRenderPass`). **Does NOT call `vkEndCommandBuffer`.**
3. `onUIRender(cmdBuffer, imageIndex)` — UI rendering callback (ImGui) executes in its own independent `VkRenderPass` (managed by the ImGui Vulkan backend) **after** the geometry/lighting pass ends.
4. `vkEndCommandBuffer` — called **once**, exclusively by `VulkanRenderer`, after all passes including UI are recorded.

This ensures that the command buffer remains in the Recording state throughout the UI pass and is closed exactly once, preventing validation errors like `VUID-vkEndCommandBuffer-commandBuffer-recording`.

## 5. UI Rendering Decoupling

ImGui rendering is fully decoupled from the scene rendering passes. The `Render()` function signature accepts an `onUIRender` callback:

```cpp
void VulkanRenderer::Render(
    const SceneViewData& viewData,
    const std::vector<RenderPacket>& renderPackets,
    std::function<void(VkCommandBuffer, uint32_t)> onUIRender = nullptr
);
```

The callback receives both `VkCommandBuffer` and `imageIndex` (required by the ImGui Vulkan backend to select the correct framebuffer). The UI RenderPass is entirely owned and managed by ImGui — `VulkanRenderer` only provides the recording slot after the scene pipeline finishes.
