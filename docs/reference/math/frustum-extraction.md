# 数学工具参考

## Gribb/Hartmann 视锥体平面提取

### 动机

GPU-Driven 渲染需要做 Frustum Culling — 判断每个物体是否在相机视野内。这需要视锥体的 6 个平面（左、右、下、上、近、远），每个平面表达为 `vec4(nx, ny, nz, d)`，其中 `dot(n, P) + d >= 0` 表示点 P 在平面的"内部"。

传统方法先求 View 和 Proj 的逆矩阵，再算出视锥体的 8 个角点，最后从角点构造平面 — 需要矩阵求逆 + 大量向量运算。

### 原理

ViewProj 矩阵 `M` 将世界坐标变换到裁剪空间：

```
clipPos = M * worldPos
```

视锥体在裁剪空间中是 6 个简单平面：

```
左:   x + w = 0     右:  -x + w = 0
下:   y + w = 0     上:  -y + w = 0
近:    z = 0        远:  -z + w = 0
```

每个平面满足 `dot(plane_clip, clipPos) = 0`。代入 `clipPos = M * worldPos`：

```
dot(plane_clip, M * worldPos) = 0
  → dot(M^T * plane_clip, worldPos) = 0
```

因此世界空间的平面系数 = `M^T * plane_clip`。展开就是 M 的行向量加减：

| 平面 | clip 空间平面 | M^T * plane | 代码 |
|------|-------------|-------------|------|
| 左 | `( 1,  0, 0, 1)` | Row0 + Row3 | `row3 + row0` |
| 右 | `(-1,  0, 0, 1)` | -Row0 + Row3 | `row3 - row0` |
| 下 | `( 0,  1, 0, 1)` | Row1 + Row3 | `row3 + row1` |
| 上 | `( 0, -1, 0, 1)` | -Row1 + Row3 | `row3 - row1` |
| 近 | `( 0,  0, 1, 0)` | Row2 | `row2` |
| 远 | `( 0,  0,-1, 1)` | -Row2 + Row3 | `row3 - row2` |

### 实现

```cpp
// src/Core/Utils/Math.h
std::array<glm::vec4, 6> ExtractFrustumPlanes(const glm::mat4& viewProj) {
    std::array<glm::vec4, 6> planes;

    planes[0] = glm::vec4(  // 左
        viewProj[0][3] + viewProj[0][0],
        viewProj[1][3] + viewProj[1][0],
        viewProj[2][3] + viewProj[2][0],
        viewProj[3][3] + viewProj[3][0]);

    planes[1] = glm::vec4(  // 右
        viewProj[0][3] - viewProj[0][0],
        viewProj[1][3] - viewProj[1][0],
        viewProj[2][3] - viewProj[2][0],
        viewProj[3][3] - viewProj[3][0]);

    planes[2] = glm::vec4(  // 下
        viewProj[0][3] + viewProj[0][1],
        viewProj[1][3] + viewProj[1][1],
        viewProj[2][3] + viewProj[2][1],
        viewProj[3][3] + viewProj[3][1]);

    planes[3] = glm::vec4(  // 上
        viewProj[0][3] - viewProj[0][1],
        viewProj[1][3] - viewProj[1][1],
        viewProj[2][3] - viewProj[2][1],
        viewProj[3][3] - viewProj[3][1]);

    planes[4] = glm::vec4(  // 近
        viewProj[0][2], viewProj[1][2],
        viewProj[2][2], viewProj[3][2]);

    planes[5] = glm::vec4(  // 远
        viewProj[0][3] - viewProj[0][2],
        viewProj[1][3] - viewProj[1][2],
        viewProj[2][3] - viewProj[2][2],
        viewProj[3][3] - viewProj[3][2]);

    for (auto& p : planes) {
        float len = glm::length(glm::vec3(p));
        p /= len;
    }

    return planes;
}
```

### 归一化

提取出的平面向量长度不一定是 1，需要归一化。归一化后 `dot(normal, P) + d` 就是点 P 到平面的有符号距离，可以直接做 Sphere-Frustum 测试：

```glsl
// GPU 端 frustum_cull.comp
float dist = dot(vec4(worldCenter, 1.0), frustumPlanes[p]);
if (dist < -worldRadius) { /* 不可见 */ }
```

### 参考

Gil Gribb, Klaus Hartmann. *Fast Extraction of Viewing Frustum Planes from the World-View-Projection Matrix* (2001).
