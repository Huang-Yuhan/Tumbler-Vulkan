# 基于图像的光照 (Image-Based Lighting - IBL)

## 目录
1. [什么是 IBL？为什么需要它？](#1-什么是-ibl为什么需要它)
2. [IBL 的核心概念](#2-ibl-的核心概念)
3. [IBL 的实现步骤](#3-ibl-的实现步骤)
4. [在 Tumbler 引擎中的实现](#4-在-tumbler-引擎中的实现)

---

## 1. 什么是 IBL？为什么需要它？

### 问题：只有一个点光源不够！

想象一下你在一个漂亮的大厅里：
- 墙上有大窗户，阳光照进来
- 天花板上有吊灯
- 地板是抛光的大理石，反射着周围的一切
- 墙上有镜子，反射着整个房间

如果我们只用之前的**点光源**（Point Light），会有什么问题？

1. **只能模拟直接光照**：光线直接从光源到物体
2. **没有环境反射**：物体不会反射周围的环境
3. **不真实**：在现实世界中，物体不仅被直接光照亮，还被整个环境照亮

### 解决方案：IBL！

**基于图像的光照 (Image-Based Lighting)** 是一种技术，它使用一张**环境贴图**（Environment Map）来模拟整个环境的光照。

**简单来说**：
- 我们拍一张 360° 的全景照片（HDR 环境贴图）
- 用这张照片来照亮场景中的物体
- 物体也会反射这张照片

---

## 2. IBL 的核心概念

### 2.1 HDR 环境贴图

**HDR** = High Dynamic Range（高动态范围）

普通的照片（LDR）：
- 颜色值范围：0.0 ~ 1.0
- 太阳和灯泡的亮度一样！
- 不够真实

HDR 照片：
- 颜色值可以超过 1.0
- 太阳可以是 10000 倍亮！
- 更接近真实世界

在 IBL 中，我们使用 **HDR 全景图**（通常是 .hdr 格式）来表示环境。

---

### 2.2 环境贴图的两种用法

IBL 把环境光照分成两部分：

#### A. 漫反射环境光 (Diffuse IBL)
- **作用**：给物体提供均匀的环境光
- **就像**：阴天时，整个天空均匀地照亮万物
- **怎么算**：把环境贴图模糊成一张很小的图（Irradiance Map）

#### B. 镜面反射环境光 (Specular IBL)
- **作用**：让物体反射周围的环境
- **就像**：你在擦亮的金属球上看到周围的倒影
- **怎么算**：根据物体的粗糙度，用不同模糊程度的环境贴图（Prefilter Map）

---

### 2.3 需要预计算的三张图

为了让 IBL 实时运行（60fps），我们需要预先计算三张图：

| 贴图名称 | 用途 | 大小 |
|---------|------|------|
| **Irradiance Map** | 漫反射环境光 | 32x32 或 64x64 |
| **Prefilter Map** | 镜面反射环境光 | 多层 Mipmap（1024→512→256...） |
| **BRDF LUT** | BRDF 查找表 | 512x512 |

---

## 3. IBL 的实现步骤

### 步骤 1：加载 HDR 环境贴图

首先，我们需要一张 HDR 环境贴图（.hdr 文件）。

它通常是**等距柱状投影**（Equirectangular Projection），看起来像世界地图。

### 步骤 2：预计算 Irradiance Map（漫反射）

**什么是 Irradiance Map？**
- 把环境贴图在每个方向上平均
- 结果是一张模糊的贴图
- 用于漫反射

**数学原理**（看不懂没关系）：
```
我们对半球上的所有入射光线积分：
 irradiance = ∫ L_i(ω_i) * cosθ dω_i
```

**简单理解**：
对于表面上的每个点，我们想知道「从这个点的法线方向看出去，整个半球有多亮」。

### 步骤 3：预计算 Prefilter Map（镜面反射）

**什么是 Prefilter Map？**
- 根据不同的粗糙度，预过滤环境贴图
- 粗糙度越低，反射越清晰
- 粗糙度越高，反射越模糊

**怎么做？**
- 创建 Mipmap（多级渐远纹理）
- 每个 Mip 级别对应一个粗糙度值
- Mip 0（最清晰）→ 粗糙度 0.0
- Mip N（最模糊）→ 粗糙度 1.0

### 步骤 4：预计算 BRDF LUT

**什么是 BRDF LUT？**
- LUT = Look-Up Table（查找表）
- 存储 BRDF 的一部分计算结果
- 让实时计算更快

**为什么需要？**
BRDF 计算很复杂，如果每帧每个像素都算完整的 BRDF，会很慢！所以我们预先算好存在一张图里。

### 步骤 5：实时渲染时使用

在 Fragment Shader 中：
1. 采样 Irradiance Map → 得到漫反射环境光
2. 采样 Prefilter Map → 得到镜面反射环境光
3. 采样 BRDF LUT → 得到 BRDF 系数
4. 把它们加起来！

---

## 4. 在 Tumbler 引擎中的实现

### 4.1 材质系统更新

我们需要在材质系统中添加这些新的纹理绑定：

| 绑定 | 名称 | 用途 |
|------|------|------|
| Set 1, Binding 0 | BaseColorMap | 基础颜色贴图 |
| Set 1, Binding 1 | NormalMap | 法线贴图 |
| Set 1, Binding 2 | Material UBO | 材质参数 |
| Set 1, Binding 3 | IrradianceMap | 漫反射环境光 |
| Set 1, Binding 4 | PrefilterMap | 镜面反射环境光 |
| Set 1, Binding 5 | BRDFLUT | BRDF 查找表 |

### 4.2 Shader 更新

#### 顶点 Shader (pbr.vert)
不需要大改，保持原样即可。

#### 片段 Shader (pbr.frag) 的新部分：

```glsl
// 采样 Irradiance Map（漫反射）
vec3 irradiance = texture(IrradianceMap, N).rgb;
vec3 diffuse = irradiance * albedo;

// 采样 Prefilter Map（镜面反射）
vec3 R = reflect(-V, N);
float lod = roughness * MAX_REFLECTION_LOD;
vec3 prefilteredColor = textureLod(PrefilterMap, R, lod).rgb;

// 采样 BRDF LUT
vec2 brdf = texture(BRDFLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);

// 最终颜色
vec3 ambient = (kD * diffuse + specular) * ao;
```

### 4.3 使用示例

```cpp
// 加载 HDR 环境贴图和预计算的贴图
auto irradianceMap = assetMgr->GetOrLoadTexture("IrradianceMap", "assets/env/irradiance.hdr");
auto prefilterMap = assetMgr->GetOrLoadTexture("PrefilterMap", "assets/env/prefilter.hdr");
auto brdfLUT = assetMgr->GetOrLoadTexture("BRDFLUT", "assets/env/brdf_lut.png");

// 设置到材质
matInstance->SetTexture("IrradianceMap", irradianceMap);
matInstance->SetTexture("PrefilterMap", prefilterMap);
matInstance->SetTexture("BRDFLUT", brdfLUT);
matInstance->ApplyChanges();
```

---

## 5. 总结

IBL 让你的渲染更真实！它：
1. ✅ 用环境贴图代替无数个点光源
2. ✅ 让物体反射周围环境
3. ✅ 提供更自然的环境光

关键是：
- **预计算**： offline 做复杂计算，runtime 只需要查表
- **三张图**：Irradiance, Prefilter, BRDF LUT
- **两部分**：漫反射 + 镜面反射

现在，你的引擎可以渲染出像 3A 游戏一样真实的画面了！🎉
