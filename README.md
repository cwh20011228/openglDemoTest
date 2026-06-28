# openglDemoTest

## 1. MsaaDemo.cpp是关于多重采样抗锯齿的demo，开启多重采样后，边缘会变得平滑，减少锯齿现象。

## 2. Pixel Bead Art — OpenGL 拼豆图片转换器

## 3. glHint.cpp 是关于OpenGL提示的demo，使用glHint函数可以告诉OpenGL在某些情况下应该使用哪种算法来处理特定的操作，以优化性能或提高质量。
### 左边是使用GL_FASTEST提示的结果，右边是使用GL_NICEST提示的结果。可以看到，左边图片较为粗糙，而右边图片更加平滑。
![alt text](.\res_image\glHint_study.png)

## 4. ComputeShaderDemo.cpp 是关于计算着色器的demo

这个 demo 使用 OpenGL 4.3 计算着色器把结果写入 `GL_TEXTURE_2D`，然后用普通的全屏三角形 fragment shader 把纹理画到窗口上。

### 运行方式

在 Visual Studio 打开 CMake 工程后，选择并运行 `Compute_Shader_Demo` 目标。

### 操作

- `Space`: 暂停/继续传给 compute shader 的时间变量。
- `M`: 切换不同的 compute shader 图案模式。
- 鼠标移动: 影响波纹中心和 Mandelbrot 偏移。
- `Esc`: 退出。

### 建议调试点

- `glDispatchCompute(groupsX, groupsY, 1)`: 用 F10 越过，查看 CPU 侧提交的 work group 数量；它不能用 F11 进入 GLSL。
- `glBindImageTexture(0, outputTexture, ...)`: 看 C++ 的 binding 和 GLSL `layout(binding = 0)` 如何对应。
- `glMemoryBarrier(...)`: 去掉或改错这里，可以观察 compute 写入和后续采样之间的同步问题。
- `ReadBrightPixelCount(statsBuffer)`: 这是 SSBO 从 GPU 读回 CPU 的例子，方便理解 `atomicAdd` 的结果。
- `ReadDebugProbe(probeBuffer)`: 在函数内 `return value` 处断下，展开局部变量 `value`，查看鼠标所指像素在 GPU 中的数据流。

`DebugProbe::intermediate` 随模式变化：waves 为 `distance/wave/sweep/glow`，Mandelbrot 为 `c.x/c.y/iteration/|z|^2`，field 为 `q.x/q.y/value/|q|`。
