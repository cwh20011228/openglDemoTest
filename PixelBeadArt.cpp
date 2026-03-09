#include <iostream>
#include <vector>
#include <string>
#include <initializer_list>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <fstream>
#include <map>
#include <sstream>
#include <set>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include "./stb_image.h"

int windowsWidth = 1000;
int windowsHeight = 800;

struct ShaderSource {
    GLenum type;
    const std::string* src;
};

struct Vec2 {
    float x;
    float y;
};

struct Vec3 {
    float x;
    float y;
    float z;
};

struct ImageData {
    unsigned char* data = nullptr;
    int width = 0;
    int height = 0;
    int channels = 0;
};

struct BeadColor {
    const char* name;
    Vec3 color;
};

enum class PaletteStyle {
    Basic = 0,
    Pastel = 1,
    Retro = 2,
};

struct InstanceData {
    float offsetX;
    float offsetY;
    float colorR;
    float colorG;
    float colorB;
    float scale;
};

struct BeadRecipeItem {
    std::string name;
    int count = 0;
    Vec3 color;
};

struct CameraState {
    float zoom = 1.0f;
    float panX = 0.0f;
    float panY = 0.0f;
    bool dragging = false;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
};

struct AppState {
    int beadColumns = 64;
    bool usePalette = true;
    int paletteColorCount = 20;
    PaletteStyle paletteStyle = PaletteStyle::Basic;
    float beadScale = 0.82f;
    bool dirty = true;
    bool keyPressedUp = false;
    bool keyPressedDown = false;
    bool keyPressedQ = false;
    bool keyPressedLeft = false;
    bool keyPressedRight = false;
    bool keyPressedE = false;
    bool keyPressed1 = false;
    bool keyPressed2 = false;
    bool keyPressed3 = false;
    bool keyPressed4 = false;
    bool keyPressedZ = false;
    bool keyPressedX = false;
    bool keyPressedC = false;
};

struct BeadLayoutData {
    std::vector<std::vector<int>> paletteIndices;
};

static GLuint CompileShader(GLenum type, const std::string& source);
static GLuint LinkProgram(GLuint vs, GLuint fs);
static GLuint CreateProgramFromSources(std::initializer_list<ShaderSource> stages);
static const char* ShaderTypeName(GLenum type);
static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
static ImageData LoadImageFile(const std::string& path);
static void FreeImageFile(ImageData& image);
static std::vector<float> GenerateCircleVertices(int segments);
static Vec3 SampleAverageColor(const ImageData& image, int x0, int y0, int x1, int y1);
static GLuint CreateTextureFromImage(const ImageData& image);
static std::vector<float> GenerateQuadVertices(float left, float right, float top, float bottom);
static int FindPaletteIndex(const Vec3& color, int activePaletteCount, PaletteStyle style);
static std::vector<InstanceData> BuildBeadInstances(const ImageData& image, const AppState& state, int& outRows, std::vector<BeadRecipeItem>& outRecipe, BeadLayoutData& outLayout);
static void ExportBeadRecipe(const std::vector<BeadRecipeItem>& recipe, const std::string& imagePath, int columns, int rows, PaletteStyle paletteStyle, int paletteColorCount);
static void ExportBeadLayout(const BeadLayoutData& layout, const std::string& imagePath, int columns, int rows, PaletteStyle paletteStyle, int paletteColorCount);
static void UpdateCameraInput(GLFWwindow* window, CameraState& camera);
static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
static bool UpdateInput(GLFWwindow* window, AppState& state);
static std::string BuildWindowTitle(const AppState& state, const ImageData& image, int rows, const std::string& imagePath);
static const std::vector<BeadColor>& GetActivePalette(PaletteStyle style);
static const char* GetPaletteStyleName(PaletteStyle style);

static CameraState* g_cameraForScroll = nullptr;

static const std::vector<BeadColor> g_basicPalette = {
    {"White",      {0.95f, 0.95f, 0.95f}},
    {"Black",      {0.10f, 0.10f, 0.10f}},
    {"Red",        {0.85f, 0.18f, 0.18f}},
    {"Orange",     {0.92f, 0.48f, 0.14f}},
    {"Yellow",     {0.95f, 0.83f, 0.20f}},
    {"Lemon",      {0.98f, 0.92f, 0.45f}},
    {"Green",      {0.20f, 0.65f, 0.30f}},
    {"DarkGreen",  {0.10f, 0.40f, 0.18f}},
    {"Mint",       {0.55f, 0.86f, 0.68f}},
    {"Blue",       {0.18f, 0.45f, 0.85f}},
    {"DarkBlue",   {0.10f, 0.20f, 0.50f}},
    {"LightBlue",  {0.48f, 0.75f, 0.93f}},
    {"Cyan",       {0.30f, 0.82f, 0.88f}},
    {"Pink",       {0.93f, 0.55f, 0.72f}},
    {"Rose",       {0.82f, 0.36f, 0.52f}},
    {"Purple",     {0.56f, 0.33f, 0.75f}},
    {"Brown",      {0.45f, 0.28f, 0.18f}},
    {"LightBrown", {0.72f, 0.55f, 0.38f}},
    {"Gray",       {0.55f, 0.55f, 0.58f}},
    {"Skin",       {0.90f, 0.73f, 0.60f}},
};

static const std::vector<BeadColor> g_pastelPalette = {
    {"Cream",        {0.98f, 0.96f, 0.88f}},
    {"SoftBlack",    {0.20f, 0.20f, 0.24f}},
    {"Peach",        {0.96f, 0.68f, 0.62f}},
    {"Apricot",      {0.96f, 0.75f, 0.55f}},
    {"Butter",       {0.97f, 0.90f, 0.58f}},
    {"PastelYellow", {0.98f, 0.95f, 0.70f}},
    {"PastelGreen",  {0.62f, 0.86f, 0.65f}},
    {"Sage",         {0.56f, 0.73f, 0.58f}},
    {"Mint",         {0.70f, 0.92f, 0.84f}},
    {"Sky",          {0.54f, 0.76f, 0.95f}},
    {"SlateBlue",    {0.44f, 0.53f, 0.80f}},
    {"BabyBlue",     {0.76f, 0.88f, 0.97f}},
    {"Aqua",         {0.64f, 0.91f, 0.92f}},
    {"Blush",        {0.96f, 0.73f, 0.82f}},
    {"DustyRose",    {0.84f, 0.60f, 0.68f}},
    {"Lavender",     {0.76f, 0.68f, 0.92f}},
    {"Mocha",        {0.64f, 0.50f, 0.42f}},
    {"Sand",         {0.83f, 0.72f, 0.58f}},
    {"SoftGray",     {0.74f, 0.74f, 0.78f}},
    {"LightSkin",    {0.95f, 0.82f, 0.72f}},
};

static const std::vector<BeadColor> g_retroPalette = {
    {"Paper",       {0.94f, 0.91f, 0.82f}},
    {"Ink",         {0.12f, 0.10f, 0.08f}},
    {"Brick",       {0.70f, 0.24f, 0.20f}},
    {"BurntOrange", {0.78f, 0.41f, 0.18f}},
    {"Mustard",     {0.78f, 0.66f, 0.20f}},
    {"WarmYellow",  {0.90f, 0.80f, 0.36f}},
    {"Moss",        {0.34f, 0.55f, 0.26f}},
    {"Forest",      {0.20f, 0.33f, 0.18f}},
    {"Seafoam",     {0.46f, 0.70f, 0.58f}},
    {"RetroBlue",   {0.24f, 0.46f, 0.66f}},
    {"Navy",        {0.16f, 0.22f, 0.40f}},
    {"PowderBlue",  {0.58f, 0.72f, 0.82f}},
    {"Teal",        {0.22f, 0.56f, 0.56f}},
    {"Salmon",      {0.84f, 0.50f, 0.48f}},
    {"Wine",        {0.56f, 0.22f, 0.30f}},
    {"Plum",        {0.42f, 0.30f, 0.52f}},
    {"Walnut",      {0.36f, 0.24f, 0.16f}},
    {"Tan",         {0.69f, 0.58f, 0.42f}},
    {"RetroGray",   {0.50f, 0.50f, 0.46f}},
    {"BeigeSkin",   {0.82f, 0.69f, 0.55f}},
};

static const std::vector<std::string> g_candidateImagePaths = {
    "assets/textures/cute.png",
    "assets/textures/cute.png",
    "assets/textures/earth.jpg",
    "assets/textures/demo.png",
    "assets/textures/demo.jpg",
    "assets/textures/sample.png",
    "assets/textures/sample.jpg",
    "assets/textures/input.png",
    "assets/textures/input.jpg"
};

static const std::vector<BeadColor>& GetActivePalette(PaletteStyle style)
{
    switch (style) {
    case PaletteStyle::Pastel:
        return g_pastelPalette;
    case PaletteStyle::Retro:
        return g_retroPalette;
    case PaletteStyle::Basic:
    default:
        return g_basicPalette;
    }
}

static const char* GetPaletteStyleName(PaletteStyle style)
{
    switch (style) {
    case PaletteStyle::Pastel: return "Pastel";
    case PaletteStyle::Retro: return "Retro";
    case PaletteStyle::Basic:
    default: return "Basic";
    }
}

static const char* ShaderTypeName(GLenum type) {
    switch (type) {
    case GL_VERTEX_SHADER: return "VERTEX";
    case GL_FRAGMENT_SHADER: return "FRAGMENT";
    default: return "UNKNOWN";
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

static GLuint CompileShader(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint logLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        std::string log(logLen > 0 ? logLen : 1, '\0');
        if (logLen > 0) {
            glGetShaderInfoLog(shader, logLen, nullptr, &log[0]);
        }
        std::cerr << "{Shader Compile Error}[" << ShaderTypeName(type) << " shader]\n" << log << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint LinkProgram(GLuint vs, GLuint fs) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint logLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
        std::string log(logLen > 0 ? logLen : 1, '\0');
        if (logLen > 0) {
            glGetProgramInfoLog(program, logLen, nullptr, &log[0]);
        }
        std::cerr << "[Program Link Error]\n" << log << std::endl;
        glDeleteProgram(program);
        return 0;
    }

    glDetachShader(program, vs);
    glDetachShader(program, fs);
    return program;
}

static GLuint CreateProgramFromSources(std::initializer_list<ShaderSource> stages) {
    GLuint vs = 0;
    GLuint fs = 0;

    for (const auto& stage : stages) {
        if (!stage.src || stage.src->empty()) {
            continue;
        }

        if (stage.type == GL_VERTEX_SHADER) {
            vs = CompileShader(stage.type, *stage.src);
            if (!vs) return 0;
        }
        else if (stage.type == GL_FRAGMENT_SHADER) {
            fs = CompileShader(stage.type, *stage.src);
            if (!fs) {
                if (vs) glDeleteShader(vs);
                return 0;
            }
        }
    }

    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }

    GLuint program = LinkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

static ImageData LoadImageFile(const std::string& path)
{
    ImageData image;
    stbi_set_flip_vertically_on_load(0);
    image.data = stbi_load(path.c_str(), &image.width, &image.height, &image.channels, 3);
    if (image.data) {
        image.channels = 3;
    }
    return image;
}

static void FreeImageFile(ImageData& image)
{
    if (image.data) {
        stbi_image_free(image.data);
        image.data = nullptr;
    }
}

static std::vector<float> GenerateCircleVertices(int segments)
{
    std::vector<float> vertices;
    vertices.reserve((segments + 2) * 2);
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);

    const float pi = 3.1415926535f;
    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * pi * static_cast<float>(i) / static_cast<float>(segments);
        vertices.push_back(std::cos(angle));
        vertices.push_back(std::sin(angle));
    }
    return vertices;
}

static Vec3 SampleAverageColor(const ImageData& image, int x0, int y0, int x1, int y1)
{
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    int count = 0;

    x0 = std::max(0, std::min(x0, image.width));
    x1 = std::max(0, std::min(x1, image.width));
    y0 = std::max(0, std::min(y0, image.height));
    y1 = std::max(0, std::min(y1, image.height));

    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            int index = (y * image.width + x) * image.channels;
            r += image.data[index + 0];
            g += image.data[index + 1];
            b += image.data[index + 2];
            ++count;
        }
    }

    if (count == 0) {
        return { 0.0f, 0.0f, 0.0f };
    }

    return {
        static_cast<float>(r / count / 255.0),
        static_cast<float>(g / count / 255.0),
        static_cast<float>(b / count / 255.0)
    };
}

static GLuint CreateTextureFromImage(const ImageData& image)
{
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image.width, image.height, 0, GL_RGB, GL_UNSIGNED_BYTE, image.data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

static std::vector<float> GenerateQuadVertices(float left, float right, float top, float bottom)
{
    return {
        left,  bottom, 0.0f, 1.0f,
        right, bottom, 1.0f, 1.0f,
        right, top,    1.0f, 0.0f,

        left,  bottom, 0.0f, 1.0f,
        right, top,    1.0f, 0.0f,
        left,  top,    0.0f, 0.0f,
    };
}

static int FindPaletteIndex(const Vec3& color, int activePaletteCount, PaletteStyle style)
{
    const auto& palette = GetActivePalette(style);
    float bestDistance = FLT_MAX;
    int bestIndex = 0;
    int count = std::max(1, std::min(activePaletteCount, static_cast<int>(palette.size())));
    for (int i = 0; i < count; ++i) {
        float dr = color.x - palette[i].color.x;
        float dg = color.y - palette[i].color.y;
        float db = color.z - palette[i].color.z;
        float distance = dr * dr + dg * dg + db * db;
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = i;
        }
    }
    return bestIndex;
}

static std::vector<InstanceData> BuildBeadInstances(const ImageData& image, const AppState& state, int& outRows, std::vector<BeadRecipeItem>& outRecipe, BeadLayoutData& outLayout)
{
    std::vector<InstanceData> instances;
    outRecipe.clear();
    outLayout.paletteIndices.clear();
    if (!image.data || image.width <= 0 || image.height <= 0) {
        outRows = 0;
        return instances;
    }

    const auto& palette = GetActivePalette(state.paletteStyle);
    std::map<int, int> paletteCounter;

    int columns = std::max(1, state.beadColumns);
    int rows = std::max(1, static_cast<int>(std::round(static_cast<float>(image.height) / static_cast<float>(image.width) * columns)));
    outRows = rows;
    outLayout.paletteIndices.assign(rows, std::vector<int>(columns, -1));
    instances.reserve(columns * rows);

    float aspect = static_cast<float>(image.width) / static_cast<float>(image.height);
    float boardWidth = aspect >= 1.0f ? 0.82f : 0.82f * aspect;
    float boardHeight = aspect >= 1.0f ? 0.82f / aspect : 0.82f;

    float cellWidth = boardWidth / static_cast<float>(columns);
    float cellHeight = boardHeight / static_cast<float>(rows);
    float radius = 0.5f * std::min(cellWidth, cellHeight) * state.beadScale;

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < columns; ++col) {
            int x0 = col * image.width / columns;
            int x1 = (col + 1) * image.width / columns;
            int y0 = row * image.height / rows;
            int y1 = (row + 1) * image.height / rows;

            Vec3 color = SampleAverageColor(image, x0, y0, x1, y1);
            int paletteIndex = -1;
            if (state.usePalette) {
                paletteIndex = FindPaletteIndex(color, state.paletteColorCount, state.paletteStyle);
                color = palette[paletteIndex].color;
                paletteCounter[paletteIndex]++;
            }
            outLayout.paletteIndices[row][col] = paletteIndex;

            float localX = -boardWidth * 0.5f + (static_cast<float>(col) + 0.5f) * cellWidth;
            float localY = boardHeight * 0.5f - (static_cast<float>(row) + 0.5f) * cellHeight;

            instances.push_back({
                0.5f + localX,
                localY,
                color.x,
                color.y,
                color.z,
                radius
            });
        }
    }

    for (const auto& pair : paletteCounter) {
        BeadRecipeItem item;
        item.name = palette[pair.first].name;
        item.count = pair.second;
        item.color = palette[pair.first].color;
        outRecipe.push_back(item);
    }

    std::sort(outRecipe.begin(), outRecipe.end(), [](const BeadRecipeItem& a, const BeadRecipeItem& b) {
        return a.count > b.count;
    });

    return instances;
}

static void ExportBeadRecipe(const std::vector<BeadRecipeItem>& recipe, const std::string& imagePath, int columns, int rows, PaletteStyle paletteStyle, int paletteColorCount)
{
    std::ofstream file("pixel_bead_recipe.txt", std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "Failed to export recipe file." << std::endl;
        return;
    }

    std::set<std::string> usedColors;
    for (const auto& item : recipe) {
        if (item.count > 0) {
            usedColors.insert(item.name);
        }
    }

    file << "Pixel Bead Art Recipe\n";
    file << "Image: " << imagePath << "\n";
    file << "Grid: " << columns << " x " << rows << "\n";
    file << "Palette Style: " << GetPaletteStyleName(paletteStyle) << "\n";
    file << "Palette Enabled Colors: " << paletteColorCount << "\n";
    file << "Palette Actually Used Colors: " << usedColors.size() << "\n\n";
    file << "Color List:\n";

    int total = 0;
    for (const auto& item : recipe) {
        total += item.count;
        file << "- " << item.name
             << " | count=" << item.count
             << " | rgb("
             << static_cast<int>(item.color.x * 255.0f) << ", "
             << static_cast<int>(item.color.y * 255.0f) << ", "
             << static_cast<int>(item.color.z * 255.0f) << ")\n";
    }

    file << "\nTotal beads: " << total << "\n";
    file.close();
    std::cout << "Exported: pixel_bead_recipe.txt" << std::endl;
}

static void ExportBeadLayout(const BeadLayoutData& layout, const std::string& imagePath, int columns, int rows, PaletteStyle paletteStyle, int paletteColorCount)
{
    std::ofstream file("pixel_bead_layout.txt", std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "Failed to export layout file." << std::endl;
        return;
    }

    const auto& palette = GetActivePalette(paletteStyle);
    std::set<int> usedIndices;
    for (const auto& rowData : layout.paletteIndices) {
        for (int idx : rowData) {
            if (idx >= 0) {
                usedIndices.insert(idx);
            }
        }
    }

    file << "Pixel Bead Art Layout\n";
    file << "Image: " << imagePath << "\n";
    file << "Grid: " << columns << " x " << rows << "\n";
    file << "Palette Style: " << GetPaletteStyleName(paletteStyle) << "\n";
    file << "Palette Enabled Colors: " << paletteColorCount << "\n";
    file << "Palette Actually Used Colors: " << usedIndices.size() << "\n\n";

    for (int row = 0; row < rows; ++row) {
        file << "Row " << row << ":\n";
        for (int col = 0; col < columns; ++col) {
            int index = layout.paletteIndices[row][col];
            file << "  [" << row << "," << col << "] ";
            if (index >= 0 && index < static_cast<int>(palette.size())) {
                const BeadColor& color = palette[index];
                file << color.name
                     << " rgb=("
                     << static_cast<int>(color.color.x * 255.0f) << ", "
                     << static_cast<int>(color.color.y * 255.0f) << ", "
                     << static_cast<int>(color.color.z * 255.0f) << ")";
            }
            else {
                file << "UnknownColor";
            }
            file << "\n";
        }
        file << "\n";
    }

    file.close();
    std::cout << "Exported: pixel_bead_layout.txt" << std::endl;
}

static void UpdateCameraInput(GLFWwindow* window, CameraState& camera)
{
    double mouseX = 0.0;
    double mouseY = 0.0;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    int winWidth = 0;
    int winHeight = 0;
    glfwGetWindowSize(window, &winWidth, &winHeight);

    bool inRightHalf = mouseX >= winWidth * 0.5;
    bool middlePressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;

    if (middlePressed && inRightHalf) {
        if (!camera.dragging) {
            camera.dragging = true;
            camera.lastMouseX = mouseX;
            camera.lastMouseY = mouseY;
        }
        else {
            double dx = mouseX - camera.lastMouseX;
            double dy = mouseY - camera.lastMouseY;
            camera.panX += static_cast<float>(dx / winWidth) * 2.0f;
            camera.panY -= static_cast<float>(dy / winHeight) * 2.0f;
            camera.lastMouseX = mouseX;
            camera.lastMouseY = mouseY;
        }
    }
    else {
        camera.dragging = false;
    }
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    (void)window;
    (void)xoffset;
    if (!g_cameraForScroll) {
        return;
    }

    if (yoffset > 0.0) {
        g_cameraForScroll->zoom *= 1.1f;
    }
    else if (yoffset < 0.0) {
        g_cameraForScroll->zoom *= 0.9f;
    }
    g_cameraForScroll->zoom = std::max(0.4f, std::min(g_cameraForScroll->zoom, 5.0f));
}

static bool UpdateInput(GLFWwindow* window, AppState& state)
{
    bool changed = false;

    bool upPressed = glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS;
    if (upPressed && !state.keyPressedUp) {
        state.beadColumns = std::min(state.beadColumns + 4, 256);
        changed = true;
    }
    state.keyPressedUp = upPressed;

    bool downPressed = glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS;
    if (downPressed && !state.keyPressedDown) {
        state.beadColumns = std::max(state.beadColumns - 4, 8);
        changed = true;
    }
    state.keyPressedDown = downPressed;

    bool qPressed = glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS;
    if (qPressed && !state.keyPressedQ) {
        state.usePalette = !state.usePalette;
        changed = true;
    }
    state.keyPressedQ = qPressed;

    bool leftPressed = glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS;
    if (leftPressed && !state.keyPressedLeft) {
        state.beadScale = std::max(0.55f, state.beadScale - 0.03f);
        changed = true;
    }
    state.keyPressedLeft = leftPressed;

    bool rightPressed = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;
    if (rightPressed && !state.keyPressedRight) {
        state.beadScale = std::min(0.98f, state.beadScale + 0.03f);
        changed = true;
    }
    state.keyPressedRight = rightPressed;

    bool ePressed = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
    if (ePressed && !state.keyPressedE) {
        changed = true;
    }
    state.keyPressedE = ePressed;

    bool key1Pressed = glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS;
    if (key1Pressed && !state.keyPressed1) {
        state.paletteColorCount = 8;
        changed = true;
    }
    state.keyPressed1 = key1Pressed;

    bool key2Pressed = glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS;
    if (key2Pressed && !state.keyPressed2) {
        state.paletteColorCount = 12;
        changed = true;
    }
    state.keyPressed2 = key2Pressed;

    bool key3Pressed = glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS;
    if (key3Pressed && !state.keyPressed3) {
        state.paletteColorCount = 16;
        changed = true;
    }
    state.keyPressed3 = key3Pressed;

    bool key4Pressed = glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS;
    if (key4Pressed && !state.keyPressed4) {
        state.paletteColorCount = 20;
        changed = true;
    }
    state.keyPressed4 = key4Pressed;

    bool keyZPressed = glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS;
    if (keyZPressed && !state.keyPressedZ) {
        state.paletteStyle = PaletteStyle::Basic;
        changed = true;
    }
    state.keyPressedZ = keyZPressed;

    bool keyXPressed = glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS;
    if (keyXPressed && !state.keyPressedX) {
        state.paletteStyle = PaletteStyle::Pastel;
        changed = true;
    }
    state.keyPressedX = keyXPressed;

    bool keyCPressed = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
    if (keyCPressed && !state.keyPressedC) {
        state.paletteStyle = PaletteStyle::Retro;
        changed = true;
    }
    state.keyPressedC = keyCPressed;

    return changed;
}

static std::string BuildWindowTitle(const AppState& state, const ImageData& image, int rows, const std::string& imagePath)
{
    std::string title = "Pixel Bead Art | image=" + imagePath;
    title += " | src=" + std::to_string(image.width) + "x" + std::to_string(image.height);
    title += " | beads=" + std::to_string(state.beadColumns) + "x" + std::to_string(rows);
    title += state.usePalette ? " | palette=on" : " | palette=off";
    if (state.usePalette) {
        title += " | colors=" + std::to_string(state.paletteColorCount);
        title += " | style=" + std::string(GetPaletteStyleName(state.paletteStyle));
    }
    title += " | scale=" + std::to_string(state.beadScale).substr(0, 4);
    title += " | Z/X/C style | 1/2/3/4 colors | E export";
    return title;
}

const std::string imageVertexShaderSource = R"GLSL(
#version 460 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
out vec2 TexCoord;
void main()
{
    TexCoord = aTexCoord;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)GLSL";

const std::string imageFragmentShaderSource = R"GLSL(
#version 460 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
void main()
{
    FragColor = texture(uTexture, TexCoord);
}
)GLSL";

const std::string beadVertexShaderSource = R"GLSL(
#version 460 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aOffset;
layout (location = 2) in vec3 aColor;
layout (location = 3) in float aScale;

out vec3 beadColor;
out vec2 localPos;

uniform float uZoom;
uniform vec2 uPan;

void main()
{
    localPos = aPos;
    beadColor = aColor;
    vec2 finalPos = (aOffset + aPos * aScale) * uZoom + uPan;
    gl_Position = vec4(finalPos, 0.0, 1.0);
}
)GLSL";

const std::string beadFragmentShaderSource = R"GLSL(
#version 460 core
in vec3 beadColor;
in vec2 localPos;
out vec4 FragColor;

void main()
{
    float dist = length(localPos);
    if (dist > 1.0)
    {
        discard;
    }

    float rim = smoothstep(1.0, 0.15, dist);
    vec3 color = beadColor * (0.82 + rim * 0.22);
    FragColor = vec4(color, 1.0);
}
)GLSL";

int main(int argc, char* argv[])
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(windowsWidth, windowsHeight, "Pixel Bead Art", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_MULTISAMPLE);

    std::string imagePath;
    if (argc > 1) {
        imagePath = argv[1];
    }
    else {
        for (const auto& candidate : g_candidateImagePaths) {
            ImageData test = LoadImageFile(candidate);
            if (test.data) {
                imagePath = candidate;
                FreeImageFile(test);
                break;
            }
        }
    }

    if (imagePath.empty()) {
        std::cerr << "No image found. Put an image in assets/, such as assets/test.png, or pass a file path as argv[1]." << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    ImageData image = LoadImageFile(imagePath);
    if (!image.data) {
        std::cerr << "Failed to load image: " << imagePath << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    CameraState camera;
    g_cameraForScroll = &camera;
    glfwSetScrollCallback(window, scroll_callback);

    GLuint program = CreateProgramFromSources({
        {GL_VERTEX_SHADER, &beadVertexShaderSource},
        {GL_FRAGMENT_SHADER, &beadFragmentShaderSource},
    });

    GLuint imageProgram = CreateProgramFromSources({
        {GL_VERTEX_SHADER, &imageVertexShaderSource},
        {GL_FRAGMENT_SHADER, &imageFragmentShaderSource},
    });

    if (!program || !imageProgram) {
        if (program) glDeleteProgram(program);
        if (imageProgram) glDeleteProgram(imageProgram);
        FreeImageFile(image);
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    GLuint imageTexture = CreateTextureFromImage(image);

    std::vector<float> circleVertices = GenerateCircleVertices(48);

    GLuint vao = 0;
    GLuint circleVbo = 0;
    GLuint instanceVbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &circleVbo);
    glGenBuffers(1, &instanceVbo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, circleVbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(circleVertices.size() * sizeof(float)), circleVertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(InstanceData), nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)offsetof(InstanceData, offsetX));
    glVertexAttribDivisor(1, 1);

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)offsetof(InstanceData, colorR));
    glVertexAttribDivisor(2, 1);

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)offsetof(InstanceData, scale));
    glVertexAttribDivisor(3, 1);

    glBindVertexArray(0);

    std::vector<float> leftQuadVertices = GenerateQuadVertices(-0.95f, -0.05f, 0.9f, -0.9f);
    GLuint imageVao = 0;
    GLuint imageVbo = 0;
    glGenVertexArrays(1, &imageVao);
    glGenBuffers(1, &imageVbo);
    glBindVertexArray(imageVao);
    glBindBuffer(GL_ARRAY_BUFFER, imageVbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(leftQuadVertices.size() * sizeof(float)), leftQuadVertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);

    AppState appState;
    std::vector<InstanceData> instances;
    int beadRows = 0;
    std::vector<BeadRecipeItem> recipe;
    BeadLayoutData layout;

    std::cout << "Controls: UP/DOWN change bead count, LEFT/RIGHT change bead size, Q toggles palette mode, Z/X/C switch palette style, 1/2/3/4 select 8/12/16/20 colors, E exports recipe, MouseWheel zooms right view, MiddleMouse drags right view." << std::endl;
    std::cout << "Image: " << imagePath << " (" << image.width << "x" << image.height << ")" << std::endl;

    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, 1);
        }

        UpdateCameraInput(window, camera);

        bool inputChanged = UpdateInput(window, appState);
        if (inputChanged || appState.dirty) {
            instances = BuildBeadInstances(image, appState, beadRows, recipe, layout);
            glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(instances.size() * sizeof(InstanceData)), instances.data(), GL_DYNAMIC_DRAW);
            glfwSetWindowTitle(window, BuildWindowTitle(appState, image, beadRows, imagePath).c_str());
            appState.dirty = false;

            if (appState.keyPressedE) {
                ExportBeadRecipe(recipe, imagePath, appState.beadColumns, beadRows, appState.paletteStyle, appState.paletteColorCount);
                ExportBeadLayout(layout, imagePath, appState.beadColumns, beadRows, appState.paletteStyle, appState.paletteColorCount);
                std::cout << "Recipe stats:" << std::endl;
                for (const auto& item : recipe) {
                    std::cout << "  " << item.name << ": " << item.count << std::endl;
                }
            }
        }

        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(imageProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, imageTexture);
        glUniform1i(glGetUniformLocation(imageProgram, "uTexture"), 0);
        glBindVertexArray(imageVao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        glUseProgram(program);
        glUniform1f(glGetUniformLocation(program, "uZoom"), camera.zoom);
        glUniform2f(glGetUniformLocation(program, "uPan"), camera.panX, camera.panY);
        glBindVertexArray(vao);
        glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, static_cast<GLsizei>(circleVertices.size() / 2), static_cast<GLsizei>(instances.size()));
        glBindVertexArray(0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteTextures(1, &imageTexture);
    glDeleteBuffers(1, &imageVbo);
    glDeleteVertexArrays(1, &imageVao);
    glDeleteBuffers(1, &instanceVbo);
    glDeleteBuffers(1, &circleVbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(imageProgram);
    glDeleteProgram(program);

    FreeImageFile(image);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
