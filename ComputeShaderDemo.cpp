#include <algorithm>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

static constexpr int kWindowWidth = 1000;
static constexpr int kWindowHeight = 700;
static constexpr int kTextureWidth = 1024;
static constexpr int kTextureHeight = 1024;
static constexpr int kLocalSize = 16;
static constexpr int kModeCount = 3;

struct ShaderSource {
    GLenum type;
    const char* source;
};

struct Int4 {
    GLint x;
    GLint y;
    GLint z;
    GLint w;
};

struct Float4 {
    GLfloat x;
    GLfloat y;
    GLfloat z;
    GLfloat w;
};

// Mirrors the std430 DebugProbe block in the compute shader.
struct DebugProbe {
    Int4 ids;              // pixel.xy, workGroup.xy
    Int4 localAndMode;     // localInvocation.xy, mode, written
    Float4 uvTimeLuma;     // uv.xy, time, luminance
    Float4 positionMouse;  // normalized position.xy, normalized mouse.xy
    Float4 intermediate;   // values specific to the current mode
    Float4 color;          // final rgba written to the image
};

static_assert(sizeof(DebugProbe) == 96, "DebugProbe must match the GLSL std430 layout");

static const char* kComputeShaderSrc = R"GLSL(
#version 430 core

// A work group contains 16 * 16 shader invocations.
// C++ dispatches ceil(textureSize / 16) groups in x/y.
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

// binding = 0 matches glBindImageTexture(0, ...).
// rgba32f means imageStore writes four 32-bit floats per pixel.
layout(rgba32f, binding = 0) uniform writeonly image2D uOutputImage;

// binding = 1 matches glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ...).
// This is just a tiny debug/statistics buffer for learning SSBO + atomics.
layout(std430, binding = 1) buffer DebugStats {
    uint brightPixelCount;
};

// Only one invocation writes this buffer. C++ reads it back for source-level debugging.
layout(std430, binding = 2) buffer DebugProbe {
    ivec4 probeIds;
    ivec4 probeLocalAndMode;
    vec4 probeUvTimeLuma;
    vec4 probePositionMouse;
    vec4 probeIntermediate;
    vec4 probeColor;
};

uniform ivec2 uImageSize;
uniform float uTime;
uniform vec2 uMouse;
uniform int uMode;
uniform ivec2 uProbePixel;

vec3 palette(float t)
{
    vec3 a = vec3(0.50, 0.50, 0.50);
    vec3 b = vec3(0.50, 0.50, 0.50);
    vec3 c = vec3(1.00, 1.00, 1.00);
    vec3 d = vec3(0.00, 0.33, 0.67);
    return a + b * cos(6.2831853 * (c * t + d));
}

vec3 waveMode(vec2 p, vec2 mouse, out vec4 intermediate)
{
    float d = length(p - mouse);
    float wave = 0.5 + 0.5 * sin(24.0 * d - uTime * 5.0);
    float sweep = 0.5 + 0.5 * sin(10.0 * p.x + 6.0 * p.y + uTime * 1.7);
    float glow = smoothstep(0.25, 0.0, d);
    intermediate = vec4(d, wave, sweep, glow);
    return palette(wave * 0.65 + sweep * 0.20 + uTime * 0.04) + glow * vec3(0.35, 0.45, 0.75);
}

vec3 mandelbrotMode(vec2 uv, out vec4 intermediate)
{
    vec2 c = vec2((uv.x - 0.5) * 3.1 - 0.55, (uv.y - 0.5) * 2.35);
    c += (uMouse - vec2(0.5)) * vec2(0.35, 0.25);

    vec2 z = vec2(0.0);
    int iter = 0;
    const int maxIter = 96;

    for (int i = 0; i < maxIter; ++i) {
        z = vec2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c;
        if (dot(z, z) > 4.0) {
            iter = i;
            break;
        }
        iter = maxIter;
    }

    intermediate = vec4(c, float(iter), dot(z, z));

    if (iter == maxIter) {
        return vec3(0.015, 0.018, 0.030);
    }

    float t = float(iter) / float(maxIter);
    return palette(t * 1.6 + uTime * 0.025);
}

vec3 fieldMode(vec2 p, out vec4 intermediate)
{
    vec2 q = p;
    float v = 0.0;

    for (int i = 0; i < 6; ++i) {
        float invLen = 1.0 / max(dot(q, q), 0.08);
        q = abs(q) * invLen - vec2(0.78, 0.52);
        q += 0.05 * vec2(sin(uTime + float(i)), cos(uTime * 0.7 + float(i)));
        v += exp(-abs(length(q) - 0.6) * 3.0);
    }

    intermediate = vec4(q, v, length(q));
    return palette(v * 0.13 + uTime * 0.02);
}

void main()
{
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    if (pixel.x >= uImageSize.x || pixel.y >= uImageSize.y) {
        return;
    }

    vec2 uv = (vec2(pixel) + vec2(0.5)) / vec2(uImageSize);
    float aspect = float(uImageSize.x) / float(uImageSize.y);

    vec2 p = uv * 2.0 - 1.0;
    p.x *= aspect;

    vec2 mouse = uMouse * 2.0 - 1.0;
    mouse.x *= aspect;

    vec3 color;
    vec4 intermediate;
    if (uMode == 0) {
        color = waveMode(p, mouse, intermediate);
    } else if (uMode == 1) {
        color = mandelbrotMode(uv, intermediate);
    } else {
        color = fieldMode(p, intermediate);
    }

    color = clamp(color, 0.0, 1.0);

    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    if (luma > 0.72) {
        atomicAdd(brightPixelCount, 1u);
    }

    if (all(equal(pixel, uProbePixel))) {
        probeIds = ivec4(pixel, ivec2(gl_WorkGroupID.xy));
        probeLocalAndMode = ivec4(ivec2(gl_LocalInvocationID.xy), uMode, 1);
        probeUvTimeLuma = vec4(uv, uTime, luma);
        probePositionMouse = vec4(p, mouse);
        probeIntermediate = intermediate;
        probeColor = vec4(color, 1.0);
    }

    imageStore(uOutputImage, pixel, vec4(color, 1.0));
}
)GLSL";

static const char* kFullscreenVertexShaderSrc = R"GLSL(
#version 330 core

out vec2 vUV;

void main()
{
    // Fullscreen triangle. It covers the screen without an index/vertex buffer.
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );

    vec2 uvs[3] = vec2[](
        vec2(0.0, 0.0),
        vec2(2.0, 0.0),
        vec2(0.0, 2.0)
    );

    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
    vUV = uvs[gl_VertexID];
}
)GLSL";

static const char* kFullscreenFragmentShaderSrc = R"GLSL(
#version 330 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uComputeTexture;

void main()
{
    FragColor = vec4(texture(uComputeTexture, vUV).rgb, 1.0);
}
)GLSL";

static const char* ShaderTypeName(GLenum type)
{
    switch (type) {
    case GL_VERTEX_SHADER: return "VERTEX";
    case GL_FRAGMENT_SHADER: return "FRAGMENT";
    case GL_COMPUTE_SHADER: return "COMPUTE";
    default: return "UNKNOWN";
    }
}

static GLuint CompileShader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint logLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);

        std::vector<char> log(static_cast<size_t>(std::max(1, logLen)));
        glGetShaderInfoLog(shader, logLen, nullptr, log.data());
        std::cerr << "[" << ShaderTypeName(type) << " shader compile error]\n"
                  << log.data() << std::endl;

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static GLuint LinkProgram(const std::vector<GLuint>& shaders)
{
    GLuint program = glCreateProgram();
    for (GLuint shader : shaders) {
        glAttachShader(program, shader);
    }

    glLinkProgram(program);

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint logLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);

        std::vector<char> log(static_cast<size_t>(std::max(1, logLen)));
        glGetProgramInfoLog(program, logLen, nullptr, log.data());
        std::cerr << "[program link error]\n" << log.data() << std::endl;

        glDeleteProgram(program);
        return 0;
    }

    for (GLuint shader : shaders) {
        glDetachShader(program, shader);
    }

    return program;
}

static GLuint CreateProgramFromSources(std::initializer_list<ShaderSource> stages)
{
    std::vector<GLuint> shaders;
    shaders.reserve(stages.size());

    for (const ShaderSource& stage : stages) {
        GLuint shader = CompileShader(stage.type, stage.source);
        if (!shader) {
            for (GLuint existing : shaders) {
                glDeleteShader(existing);
            }
            return 0;
        }
        shaders.push_back(shader);
    }

    GLuint program = LinkProgram(shaders);
    for (GLuint shader : shaders) {
        glDeleteShader(shader);
    }

    return program;
}

static void APIENTRY DebugMessageCallback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam)
{
    (void)source;
    (void)type;
    (void)id;
    (void)length;
    (void)userParam;

    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) {
        return;
    }

    std::cerr << "[OpenGL debug] " << message << std::endl;
}

static float Clamp01(double value)
{
    if (value < 0.0) return 0.0f;
    if (value > 1.0) return 1.0f;
    return static_cast<float>(value);
}

static bool PressedOnce(GLFWwindow* window, int key, bool& wasDown)
{
    bool down = glfwGetKey(window, key) == GLFW_PRESS;
    bool pressed = down && !wasDown;
    wasDown = down;
    return pressed;
}

static const char* ModeName(int mode)
{
    switch (mode) {
    case 0: return "waves";
    case 1: return "mandelbrot";
    case 2: return "field";
    default: return "unknown";
    }
}

static GLuint CreateOutputTexture()
{
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, kTextureWidth, kTextureHeight);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

static GLuint CreateStatsBuffer()
{
    GLuint buffer = 0;
    GLuint zero = 0;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GLuint), &zero, GL_DYNAMIC_COPY);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return buffer;
}

static GLuint CreateProbeBuffer()
{
    GLuint buffer = 0;
    DebugProbe zero = {};
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(DebugProbe), &zero, GL_DYNAMIC_COPY);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return buffer;
}

static GLuint ReadBrightPixelCount(GLuint statsBuffer)
{
    GLuint value = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, statsBuffer);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), &value);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return value;
}

static DebugProbe ReadDebugProbe(GLuint probeBuffer)
{
    DebugProbe value = {};
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, probeBuffer);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(DebugProbe), &value);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Put a breakpoint here and inspect "value" in Locals or Watch.
    return value;
}

static void PrintComputeLimits()
{
    GLint groupCount[3] = {};
    GLint groupSize[3] = {};
    for (int axis = 0; axis < 3; ++axis) {
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, axis, &groupCount[axis]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, axis, &groupSize[axis]);
    }

    GLint maxInvocations = 0;
    GLint maxImageUnits = 0;
    GLint maxSsboBindings = 0;
    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &maxInvocations);
    glGetIntegerv(GL_MAX_IMAGE_UNITS, &maxImageUnits);
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &maxSsboBindings);

    std::cout << "OpenGL compute limits\n";
    std::cout << "  GL_MAX_COMPUTE_WORK_GROUP_COUNT: "
              << groupCount[0] << ", " << groupCount[1] << ", " << groupCount[2] << "\n";
    std::cout << "  GL_MAX_COMPUTE_WORK_GROUP_SIZE:  "
              << groupSize[0] << ", " << groupSize[1] << ", " << groupSize[2] << "\n";
    std::cout << "  GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS: " << maxInvocations << "\n";
    std::cout << "  GL_MAX_IMAGE_UNITS: " << maxImageUnits << "\n";
    std::cout << "  GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS: " << maxSsboBindings << "\n";
}

static void UpdateWindowTitle(
    GLFWwindow* window,
    int mode,
    bool paused,
    GLuint brightPixels,
    const DebugProbe& probe,
    int groupsX,
    int groupsY)
{
    std::ostringstream title;
    title << "Compute Shader Demo | mode: " << ModeName(mode)
           << " | bright pixels: " << brightPixels
           << " | probe: (" << probe.ids.x << "," << probe.ids.y
           << ") luma=" << probe.uvTimeLuma.w
           << " | dispatch: " << groupsX << "x" << groupsY << " groups"
          << " | " << (paused ? "paused" : "running")
          << " | Space pause, M mode, Esc exit";
    glfwSetWindowTitle(window, title.str().c_str());
}

int main()
{
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW." << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(
        kWindowWidth,
        kWindowHeight,
        "Compute Shader Demo",
        nullptr,
        nullptr);

    if (!window) {
        std::cerr << "Failed to create an OpenGL 4.3 window. "
                  << "Compute shaders require OpenGL 4.3 or newer." << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD." << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    GLint major = 0;
    GLint minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    if (major < 4 || (major == 4 && minor < 3)) {
        std::cerr << "OpenGL " << major << "." << minor
                  << " detected. This demo needs OpenGL 4.3+." << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    if (glDebugMessageCallback) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(DebugMessageCallback, nullptr);
    }

    std::cout << "Renderer: " << glGetString(GL_RENDERER) << "\n";
    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << "\n";
    PrintComputeLimits();
    std::cout << "\nControls:\n"
              << "  Space: pause/resume time uniform\n"
              << "  M: switch compute shader mode\n"
              << "  Mouse: move the wave center / Mandelbrot offset\n"
              << "  Esc: close\n\n";

    GLuint computeProgram = CreateProgramFromSources({
        { GL_COMPUTE_SHADER, kComputeShaderSrc }
    });

    GLuint renderProgram = CreateProgramFromSources({
        { GL_VERTEX_SHADER, kFullscreenVertexShaderSrc },
        { GL_FRAGMENT_SHADER, kFullscreenFragmentShaderSrc }
    });

    if (!computeProgram || !renderProgram) {
        glDeleteProgram(computeProgram);
        glDeleteProgram(renderProgram);
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    GLuint outputTexture = CreateOutputTexture();
    GLuint statsBuffer = CreateStatsBuffer();
    GLuint probeBuffer = CreateProbeBuffer();

    GLuint emptyVao = 0;
    glGenVertexArrays(1, &emptyVao);

    const int groupsX = (kTextureWidth + kLocalSize - 1) / kLocalSize;
    const int groupsY = (kTextureHeight + kLocalSize - 1) / kLocalSize;

    GLint imageSizeLoc = glGetUniformLocation(computeProgram, "uImageSize");
    GLint timeLoc = glGetUniformLocation(computeProgram, "uTime");
    GLint mouseLoc = glGetUniformLocation(computeProgram, "uMouse");
    GLint modeLoc = glGetUniformLocation(computeProgram, "uMode");
    GLint probePixelLoc = glGetUniformLocation(computeProgram, "uProbePixel");
    GLint textureLoc = glGetUniformLocation(renderProgram, "uComputeTexture");

    int mode = 0;
    bool paused = false;
    bool spaceWasDown = false;
    bool mWasDown = false;
    float computeTime = 0.0f;
    double lastFrameTime = glfwGetTime();
    double lastTitleTime = 0.0;
    GLuint lastBrightPixels = 0;
    DebugProbe lastProbe = {};

    UpdateWindowTitle(window, mode, paused, lastBrightPixels, lastProbe, groupsX, groupsY);

    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        if (PressedOnce(window, GLFW_KEY_SPACE, spaceWasDown)) {
            paused = !paused;
        }

        if (PressedOnce(window, GLFW_KEY_M, mWasDown)) {
            mode = (mode + 1) % kModeCount;
        }

        double now = glfwGetTime();
        double dt = now - lastFrameTime;
        lastFrameTime = now;
        if (!paused) {
            computeTime += static_cast<float>(dt);
        }

        int windowWidth = 1;
        int windowHeight = 1;
        glfwGetWindowSize(window, &windowWidth, &windowHeight);

        double mouseX = 0.0;
        double mouseY = 0.0;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        float mouseU = Clamp01(mouseX / std::max(1, windowWidth));
        float mouseV = 1.0f - Clamp01(mouseY / std::max(1, windowHeight));
        int probeX = std::min(kTextureWidth - 1, static_cast<int>(mouseU * kTextureWidth));
        int probeY = std::min(kTextureHeight - 1, static_cast<int>(mouseV * kTextureHeight));

        GLuint zero = 0;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, statsBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), &zero);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, statsBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, probeBuffer);

        glUseProgram(computeProgram);
        glUniform2i(imageSizeLoc, kTextureWidth, kTextureHeight);
        glUniform1f(timeLoc, computeTime);
        glUniform2f(mouseLoc, mouseU, mouseV);
        glUniform1i(modeLoc, mode);
        glUniform2i(probePixelLoc, probeX, probeY);
        glBindImageTexture(0, outputTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        glDispatchCompute(groupsX, groupsY, 1);

        // The compute shader wrote an image and an SSBO.
        // This barrier makes those writes visible to texture sampling and CPU readback.
        glMemoryBarrier(
            GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
            GL_TEXTURE_FETCH_BARRIER_BIT |
            GL_SHADER_STORAGE_BARRIER_BIT |
            GL_BUFFER_UPDATE_BARRIER_BIT);

        if (now - lastTitleTime > 0.25) {
            lastBrightPixels = ReadBrightPixelCount(statsBuffer);
            lastProbe = ReadDebugProbe(probeBuffer);
            UpdateWindowTitle(window, mode, paused, lastBrightPixels, lastProbe, groupsX, groupsY);
            lastTitleTime = now;
        }

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        if (framebufferWidth > 0 && framebufferHeight > 0) {
            glViewport(0, 0, framebufferWidth, framebufferHeight);
            glClearColor(0.03f, 0.03f, 0.04f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            glUseProgram(renderProgram);
            glUniform1i(textureLoc, 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, outputTexture);
            glBindVertexArray(emptyVao);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &emptyVao);
    glDeleteBuffers(1, &statsBuffer);
    glDeleteBuffers(1, &probeBuffer);
    glDeleteTextures(1, &outputTexture);
    glDeleteProgram(renderProgram);
    glDeleteProgram(computeProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
