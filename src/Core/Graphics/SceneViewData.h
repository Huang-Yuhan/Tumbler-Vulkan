#pragma once
#include <glm/glm.hpp>

/**
 * @brief 场景视图数据 (Scene View Data)
 * * 【架构核心思想】
 * 本结构体代表了渲染器“观察世界”的角度和环境参数。它与表示客观存在的物体数据 (RenderPacket) 
 * 进行了极其严格的物理隔离。这一设计是现代游戏引擎（如 Unreal Engine 的 FSceneView）的基石。
 * * 【为什么不把 RenderPacket (物体) 包含在视图里？】
 * 因为在现代渲染管线中，客观场景 (Scene) 与 观察者视图 (View) 是“一对多”的关系：
 * * 1. 双人分屏 / VR渲染：世界中只有 10000 棵树 (RenderPacket)，但有 2 个玩家的相机 
 * (SceneViewData) 在同时看它们。保持分离可以避免将 10000 棵树的数据复制两遍。
 * 2. 阴影映射 (Shadow Mapping)：在图形学中，“光源”也是一个隐藏的相机。为了生成深度阴影图，
 * 我们需要用“光源的视角”创建一个特殊的 SceneViewData 去渲染世界。
 * * 【渲染器的本质】
 * 渲染器 (Renderer) 拿到代表观察者的 SceneViewData，和代表客观世界的 RenderPacket 数组后，
 * 将客观世界投影到当前观察者的屏幕上。
 */
struct SceneViewData {

    // ==========================================
    // 观察者空间 (Observer Space)
    // 描述“从哪里看，怎么看”
    // ==========================================
    glm::mat4 ViewMatrix;       // 视图矩阵 (相机的位姿逆变换)
    glm::mat4 ProjectionMatrix; // 投影矩阵 (FOV、屏幕比例、远近裁剪面)
    glm::vec3 CameraPosition;   // 相机世界坐标 (用于 PBR 的视角向量 V 的计算)
    
    // ==========================================
    // 环境光照状态 (Environment Lighting)
    // 描述“当前观察者感受到什么样的光照环境”
    // (注：未来扩展多光源时，这里可以抽象为 std::vector<LightData>)
    // ==========================================
    glm::vec3 LightPosition;    // 主光源世界坐标 (用于计算光照方向 L)
    glm::vec3 LightColor;       // 主光源颜色
    float LightIntensity;       // 主光源能量强度
};