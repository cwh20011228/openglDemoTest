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
#include <cstring>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <wincodec.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define IMGUI_IMPL_OPENGL_LOADER_GLAD
#include "./imgui/imgui.h"
#include "./imgui/imgui_impl_glfw.h"
#include "./imgui/imgui_impl_opengl3.h"
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

enum class CopyMode {
    ColorName = 0,
    Coordinate = 1,
    RGB = 2,
};

struct AppState {
    int beadColumns = 64;
    bool usePalette = true;
    int paletteColorCount = 20;
    PaletteStyle paletteStyle = PaletteStyle::Basic;
    float beadScale = 0.82f;
    float beadGap = 0.08f;
    float splitRatio = 0.50f;
    float imageAlpha = 1.0f;
    float beadAlpha = 1.0f;
    bool transparentTightExport = true;
    CopyMode copyMode = CopyMode::ColorName;
    bool showBeadGrid = true;
    float gridColor[3] = { 1.0f, 1.0f, 1.0f };
    float gridAlpha = 0.27f;
    float backgroundColor[3] = { 0.08f, 0.08f, 0.10f };
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

struct BeadCellVisualInfo {
    bool valid = false;
    int row = -1;
    int col = -1;
    float centerX = 0.0f;
    float centerY = 0.0f;
    float halfWidth = 0.0f;
    float halfHeight = 0.0f;
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
static void ExportBeadRecipeCsv(const std::vector<BeadRecipeItem>& recipe, const std::string& imagePath, int columns, int rows, PaletteStyle paletteStyle, int paletteColorCount);
static void ExportBeadLayoutJson(const BeadLayoutData& layout, const std::string& imagePath, int columns, int rows, PaletteStyle paletteStyle, int paletteColorCount, const AppState& state);
static void UpdateCameraInput(GLFWwindow* window, CameraState& camera);
static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
static bool UpdateInput(GLFWwindow* window, AppState& state);
static std::string BuildWindowTitle(const AppState& state, const ImageData& image, int rows, const std::string& imagePath);
static const std::vector<BeadColor>& GetActivePalette(PaletteStyle style);
static const char* GetPaletteStyleName(PaletteStyle style);
static int GetPaletteStyleIndex(PaletteStyle style);
static PaletteStyle GetPaletteStyleFromIndex(int index);

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

static int GetPaletteStyleIndex(PaletteStyle style)
{
    switch (style) {
    case PaletteStyle::Basic: return 0;
    case PaletteStyle::Pastel: return 1;
    case PaletteStyle::Retro: return 2;
    default: return 0;
    }
}

static PaletteStyle GetPaletteStyleFromIndex(int index)
{
    switch (index) {
    case 1: return PaletteStyle::Pastel;
    case 2: return PaletteStyle::Retro;
    case 0:
    default: return PaletteStyle::Basic;
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
    (void)window;
    windowsWidth = width;
    windowsHeight = height;
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

    const float contentLeft = -0.95f;
    const float contentRight = 0.95f;
    const float contentTop = 0.90f;
    const float contentBottom = -0.90f;
    const float paneGap = 0.03f;
    float splitX = contentLeft + (contentRight - contentLeft) * state.splitRatio;
    float beadPaneLeft = splitX + paneGap;
    float beadPaneRight = contentRight;
    float beadPaneWidth = std::max(0.10f, beadPaneRight - beadPaneLeft);
    float beadPaneHeight = contentTop - contentBottom;

    float aspect = static_cast<float>(image.width) / static_cast<float>(image.height);
    float boardWidth = beadPaneWidth;
    float boardHeight = boardWidth / aspect;
    if (boardHeight > beadPaneHeight) {
        boardHeight = beadPaneHeight;
        boardWidth = boardHeight * aspect;
    }

    float cellWidth = boardWidth / static_cast<float>(columns);
    float cellHeight = boardHeight / static_cast<float>(rows);
    float finalScale = std::max(0.10f, state.beadScale - state.beadGap);
    float radius = 0.5f * std::min(cellWidth, cellHeight) * finalScale;
    float boardCenterX = beadPaneLeft + beadPaneWidth * 0.5f;
    float boardCenterY = (contentTop + contentBottom) * 0.5f;

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
                boardCenterX + localX,
                boardCenterY + localY,
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

static void ExportBeadRecipeCsv(const std::vector<BeadRecipeItem>& recipe, const std::string& imagePath, int columns, int rows, PaletteStyle paletteStyle, int paletteColorCount)
{
    std::ofstream file("pixel_bead_recipe.csv", std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "Failed to export recipe CSV file." << std::endl;
        return;
    }

    file << "Pixel Bead Art Recipe\n";
    file << "Image, " << imagePath << "\n";
    file << "Grid, " << columns << " x " << rows << "\n";
    file << "Palette Style, " << GetPaletteStyleName(paletteStyle) << "\n";
    file << "Palette Enabled Colors, " << paletteColorCount << "\n";
    file << "Palette Actually Used Colors, " << recipe.size() << "\n\n";
    file << "Color Name, Count, R, G, B\n";

    for (const auto& item : recipe) {
        file << item.name << ", "
             << item.count << ", "
             << static_cast<int>(item.color.x * 255.0f) << ", "
             << static_cast<int>(item.color.y * 255.0f) << ", "
             << static_cast<int>(item.color.z * 255.0f) << "\n";
    }

    file.close();
    std::cout << "Exported: pixel_bead_recipe.csv" << std::endl;
}

static void ExportBeadLayoutJson(const BeadLayoutData& layout, const std::string& imagePath, int columns, int rows, PaletteStyle paletteStyle, int paletteColorCount, const AppState& state)
{
    std::ofstream file("pixel_bead_layout.json", std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "Failed to export JSON file." << std::endl;
        return;
    }

    const auto& palette = GetActivePalette(paletteStyle);
    std::map<int, int> colorCounter;
    int totalBeads = 0;
    for (const auto& rowData : layout.paletteIndices) {
        for (int idx : rowData) {
            if (idx >= 0) {
                colorCounter[idx]++;
                totalBeads++;
            }
        }
    }

    file << "{\n";
    file << "  \"image\": \"" << imagePath << "\",\n";
    file << "  \"paletteStyle\": \"" << GetPaletteStyleName(paletteStyle) << "\",\n";
    file << "  \"paletteEnabledColors\": " << paletteColorCount << ",\n";
    file << "  \"window\": {\n";
    file << "    \"width\": " << windowsWidth << ",\n";
    file << "    \"height\": " << windowsHeight << ",\n";
    file << "    \"splitRatio\": " << state.splitRatio << ",\n";
    file << "    \"imageAlpha\": " << state.imageAlpha << ",\n";
    file << "    \"beadAlpha\": " << state.beadAlpha << ",\n";
    file << "    \"backgroundColor\": [" << state.backgroundColor[0] << ',' << state.backgroundColor[1] << ',' << state.backgroundColor[2] << "]\n";
    file << "  },\n";
    file << "  \"exportOptions\": {\n";
    file << "    \"usePalette\": " << (state.usePalette ? "true" : "false") << ",\n";
    file << "    \"beadColumns\": " << state.beadColumns << ",\n";
    file << "    \"beadRows\": " << rows << ",\n";
    file << "    \"beadScale\": " << state.beadScale << ",\n";
    file << "    \"beadGap\": " << state.beadGap << ",\n";
    file << "    \"transparentTightExport\": " << (state.transparentTightExport ? "true" : "false") << "\n";
    file << "  },\n";
    file << "  \"stats\": {\n";
    file << "    \"totalBeads\": " << totalBeads << ",\n";
    file << "    \"usedColors\": " << colorCounter.size() << ",\n";
    file << "    \"recipe\": [\n";

    int recipeIndex = 0;
    for (const auto& pair : colorCounter) {
        const auto& color = palette[pair.first];
        file << "      {\"name\":\"" << color.name << "\",\"count\":" << pair.second
             << ",\"rgb\":["
             << static_cast<int>(color.color.x * 255.0f) << ','
             << static_cast<int>(color.color.y * 255.0f) << ','
             << static_cast<int>(color.color.z * 255.0f) << "]}";
        if (++recipeIndex < static_cast<int>(colorCounter.size())) {
            file << ',';
        }
        file << "\n";
    }

    file << "    ]\n";
    file << "  },\n";
    file << "  \"grid\": { \"columns\": " << columns << ", \"rows\": " << rows << " },\n";
    file << "  \"rows\": [\n";
    for (int row = 0; row < rows; ++row) {
        file << "    [";
        for (int col = 0; col < columns; ++col) {
            int index = layout.paletteIndices[row][col];
            if (index >= 0 && index < static_cast<int>(palette.size())) {
                const auto& color = palette[index];
                file << "{\"row\":" << row
                     << ",\"col\":" << col
                     << ",\"name\":\"" << color.name << "\""
                     << ",\"rgb\":["
                     << static_cast<int>(color.color.x * 255.0f) << ','
                     << static_cast<int>(color.color.y * 255.0f) << ','
                     << static_cast<int>(color.color.z * 255.0f) << "]}";
            }
            else {
                file << "{\"row\":" << row << ",\"col\":" << col << ",\"name\":\"UnknownColor\"}";
            }
            if (col + 1 < columns) {
                file << ',';
            }
        }
        file << "]";
        if (row + 1 < rows) {
            file << ',';
        }
        file << "\n";
    }
    file << "  ]\n";
    file << "}\n";
    file.close();
    std::cout << "Exported: pixel_bead_layout.json" << std::endl;
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
    (void)window;
    (void)state;
    return false;
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
    title += " | size=" + std::to_string(state.beadScale).substr(0, 4);
    title += " | gap=" + std::to_string(state.beadGap).substr(0, 4);
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
uniform float uAlpha;
void main()
{
    vec4 color = texture(uTexture, TexCoord);
    FragColor = vec4(color.rgb, color.a * uAlpha);
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
uniform float uAlpha;

void main()
{
    float dist = length(localPos);
    if (dist > 1.0)
    {
        discard;
    }

    float rim = smoothstep(1.0, 0.15, dist);
    vec3 color = beadColor * (0.82 + rim * 0.22);
    FragColor = vec4(color, uAlpha);
}
)GLSL";

static std::string OpenImageFileDialog();
static std::string SavePngFileDialog();
static bool SaveFramebufferToPng(const std::string& filename, int width, int height);
static bool SaveRightPaneToPng(const std::string& filename, int framebufferWidth, int framebufferHeight, float splitRatio);
static bool SaveRightPaneTightToPng(const std::string& filename, int framebufferWidth, int framebufferHeight, const AppState& state, const ImageData& image, int beadRows);
static std::vector<float> GenerateQuadVerticesWithTexCoords(float left, float right, float top, float bottom, float u0, float u1, float v0, float v1);
static bool TrySelectBeadCellFromMouse(GLFWwindow* window, const AppState& state, const ImageData& image, int beadRows, const CameraState& camera, int& selectedRow, int& selectedCol);
static std::string BuildPreviewRowText(const BeadLayoutData& layout, const std::vector<BeadColor>& palette, int rowIndex, int startCol, int count, CopyMode copyMode);
static std::string BuildPreviewColumnText(const BeadLayoutData& layout, const std::vector<BeadColor>& palette, int startRow, int colIndex, int count, CopyMode copyMode);
static bool QueryBeadCellFromMouse(GLFWwindow* window, const AppState& state, const ImageData& image, int beadRows, const CameraState& camera, BeadCellVisualInfo& outInfo);
static BeadCellVisualInfo GetBeadCellVisualInfo(const AppState& state, const ImageData& image, int beadRows, int row, int col, const CameraState& camera, int winWidth, int winHeight);
static std::vector<ImVec2> GenerateOutlineVertices(float centerX, float centerY, float halfWidth, float halfHeight);
static std::vector<ImVec2> GenerateGridVertices(const AppState& state, const ImageData& image, int beadRows, const CameraState& camera, int winWidth, int winHeight);
static std::vector<ImVec2> GenerateImageSampleRectVertices(const AppState& state, const ImageData& image, int beadRows, int row, int col, int winWidth, int winHeight);

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

    HRESULT comInitResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool comInitialized = SUCCEEDED(comInitResult) || comInitResult == S_FALSE;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

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
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    ImageData image = LoadImageFile(imagePath);
    if (!image.data) {
        std::cerr << "Failed to load image: " << imagePath << std::endl;
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
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
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
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
    std::string pendingSavePngPath;
    std::string pendingSaveBeadPngPath;
    std::string pendingSaveBeadTightPngPath;
    int previewStartRow = 0;
    int previewStartCol = 0;
    int previewRows = 8;
    int previewCols = 8;
    int selectedRow = -1;
    int selectedCol = -1;
    bool leftMousePressedLastFrame = false;
    BeadCellVisualInfo hoveredCell;

    std::cout << "ImGui ready. Image: " << imagePath << " (" << image.width << "x" << image.height << ")" << std::endl;

    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, 1);
        }

        UpdateCameraInput(window, camera);
        bool inputChanged = UpdateInput(window, appState);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (!ImGui::GetIO().WantCaptureKeyboard && selectedRow >= 0 && selectedCol >= 0) {
            bool upPressed = glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS;
            bool downPressed = glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS;
            bool leftPressed = glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS;
            bool rightPressed = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;
            bool shiftPressed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
            int moveStep = shiftPressed ? 5 : 1;

            if (upPressed && !appState.keyPressedUp) {
                selectedRow = std::max(0, selectedRow - moveStep);
            }
            if (downPressed && !appState.keyPressedDown) {
                selectedRow = std::min(std::max(0, beadRows - 1), selectedRow + moveStep);
            }
            if (leftPressed && !appState.keyPressedLeft) {
                selectedCol = std::max(0, selectedCol - moveStep);
            }
            if (rightPressed && !appState.keyPressedRight) {
                selectedCol = std::min(std::max(0, appState.beadColumns - 1), selectedCol + moveStep);
            }

            appState.keyPressedUp = upPressed;
            appState.keyPressedDown = downPressed;
            appState.keyPressedLeft = leftPressed;
            appState.keyPressedRight = rightPressed;
        }
        else {
            appState.keyPressedUp = false;
            appState.keyPressedDown = false;
            appState.keyPressedLeft = false;
            appState.keyPressedRight = false;
        }

        bool leftMousePressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        hoveredCell = {};
        if (!ImGui::GetIO().WantCaptureMouse) {
            QueryBeadCellFromMouse(window, appState, image, beadRows, camera, hoveredCell);
        }
        if (leftMousePressed && !leftMousePressedLastFrame && hoveredCell.valid && !ImGui::GetIO().WantCaptureMouse) {
            selectedRow = hoveredCell.row;
            selectedCol = hoveredCell.col;
        }
        leftMousePressedLastFrame = leftMousePressed;

        ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(430.0f, 620.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Pixel Bead Control");

        ImGui::Text("Image: %s", imagePath.c_str());
        ImGui::Text("Source Size: %d x %d", image.width, image.height);
        ImGui::Separator();

        if (ImGui::Button("Load Image")) {
            std::string selectedPath = OpenImageFileDialog();
            if (!selectedPath.empty()) {
                ImageData newImage = LoadImageFile(selectedPath);
                if (newImage.data) {
                    FreeImageFile(image);
                    glDeleteTextures(1, &imageTexture);
                    image = newImage;
                    imagePath = selectedPath;
                    imageTexture = CreateTextureFromImage(image);
                    camera.zoom = 1.0f;
                    camera.panX = 0.0f;
                    camera.panY = 0.0f;
                    previewStartRow = 0;
                    previewStartCol = 0;
                    appState.dirty = true;
                }
            }
        }
        if (ImGui::Button("Save Current PNG")) {
            pendingSavePngPath = SavePngFileDialog();
        }
        if (ImGui::Button("Save Bead PNG Only")) {
            pendingSaveBeadPngPath = SavePngFileDialog();
        }
        if (ImGui::Button("Save Bead PNG Tight")) {
            pendingSaveBeadTightPngPath = SavePngFileDialog();
        }
        ImGui::Checkbox("Transparent Tight Export", &appState.transparentTightExport);
        if (ImGui::Button("Export CSV")) {
            ExportBeadRecipeCsv(recipe, imagePath, appState.beadColumns, beadRows, appState.paletteStyle, appState.paletteColorCount);
        }
        ImGui::SameLine();
        if (ImGui::Button("Export JSON")) {
            ExportBeadLayoutJson(layout, imagePath, appState.beadColumns, beadRows, appState.paletteStyle, appState.paletteColorCount, appState);
        }

        int beadColumns = appState.beadColumns;
        if (ImGui::SliderInt("Bead Columns", &beadColumns, 8, 256)) {
            appState.beadColumns = beadColumns;
            appState.dirty = true;
        }

        float beadScale = appState.beadScale;
        if (ImGui::SliderFloat("Bead Size", &beadScale, 0.30f, 1.00f, "%.2f")) {
            appState.beadScale = beadScale;
            appState.dirty = true;
        }

        float beadGap = appState.beadGap;
        if (ImGui::SliderFloat("Bead Gap", &beadGap, 0.00f, 0.35f, "%.2f")) {
            appState.beadGap = beadGap;
            appState.dirty = true;
        }

        float splitRatio = appState.splitRatio;
        if (ImGui::SliderFloat("Original/Bead Split", &splitRatio, 0.25f, 0.75f, "%.2f")) {
            appState.splitRatio = splitRatio;
            appState.dirty = true;
        }

        if (ImGui::SliderFloat("Original Alpha", &appState.imageAlpha, 0.05f, 1.0f, "%.2f")) {
        }
        if (ImGui::SliderFloat("Bead Alpha", &appState.beadAlpha, 0.05f, 1.0f, "%.2f")) {
        }

        ImGui::Checkbox("Show Grid Lines", &appState.showBeadGrid);
        if (appState.showBeadGrid) {
            ImGui::ColorEdit3("Grid Color", appState.gridColor);
            ImGui::SliderFloat("Grid Alpha", &appState.gridAlpha, 0.05f, 1.0f, "%.2f");
        }
        ImGui::ColorEdit3("Background", appState.backgroundColor);

        bool usePalette = appState.usePalette;
        if (ImGui::Checkbox("Use Palette", &usePalette)) {
            appState.usePalette = usePalette;
            appState.dirty = true;
        }

        const char* paletteItems[] = { "Basic", "Pastel", "Retro" };
        int paletteIndex = GetPaletteStyleIndex(appState.paletteStyle);
        if (ImGui::Combo("Palette Style", &paletteIndex, paletteItems, IM_ARRAYSIZE(paletteItems))) {
            appState.paletteStyle = GetPaletteStyleFromIndex(paletteIndex);
            appState.paletteColorCount = std::min(appState.paletteColorCount, static_cast<int>(GetActivePalette(appState.paletteStyle).size()));
            appState.dirty = true;
        }

        if (appState.usePalette) {
            int maxPaletteCount = static_cast<int>(GetActivePalette(appState.paletteStyle).size());
            int paletteCount = appState.paletteColorCount;
            if (ImGui::SliderInt("Palette Color Count", &paletteCount, 1, maxPaletteCount)) {
                appState.paletteColorCount = paletteCount;
                appState.dirty = true;
            }
        }

        if (ImGui::Button("Reset View")) {
            camera.zoom = 1.0f;
            camera.panX = 0.0f;
            camera.panY = 0.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Export Files")) {
            ExportBeadRecipe(recipe, imagePath, appState.beadColumns, beadRows, appState.paletteStyle, appState.paletteColorCount);
            ExportBeadLayout(layout, imagePath, appState.beadColumns, beadRows, appState.paletteStyle, appState.paletteColorCount);
        }

        ImGui::Separator();
        ImGui::Text("Result Grid: %d x %d", appState.beadColumns, beadRows);
        ImGui::Text("Zoom: %.2f", camera.zoom);
        ImGui::Text("Pan: %.2f, %.2f", camera.panX, camera.panY);
        ImGui::Text("Palette: %s", GetPaletteStyleName(appState.paletteStyle));
        ImGui::Text("Mouse wheel: zoom | Middle drag: pan");

        if (ImGui::CollapsingHeader("Color Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("ColorStatsTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Swatch");
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("Count");
                ImGui::TableSetupColumn("RGB");
                ImGui::TableHeadersRow();
                for (int i = 0; i < static_cast<int>(recipe.size()); ++i) {
                    const auto& item = recipe[i];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::ColorButton(("##swatch" + std::to_string(i)).c_str(), ImVec4(item.color.x, item.color.y, item.color.z, 1.0f), ImGuiColorEditFlags_NoTooltip, ImVec2(18, 18));
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(item.name.c_str());
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%d", item.count);
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%d,%d,%d",
                        static_cast<int>(item.color.x * 255.0f),
                        static_cast<int>(item.color.y * 255.0f),
                        static_cast<int>(item.color.z * 255.0f));
                }
                ImGui::EndTable();
            }
        }

        if (ImGui::CollapsingHeader("Layout Preview", ImGuiTreeNodeFlags_DefaultOpen)) {
            previewRows = std::max(1, std::min(previewRows, beadRows > 0 ? beadRows : 1));
            previewCols = std::max(1, std::min(previewCols, appState.beadColumns));
            ImGui::SliderInt("Preview Rows", &previewRows, 1, beadRows > 0 ? std::min(beadRows, 12) : 1);
            ImGui::SliderInt("Preview Cols", &previewCols, 1, std::min(appState.beadColumns, 12));
            ImGui::SliderInt("Start Row", &previewStartRow, 0, beadRows > previewRows ? beadRows - previewRows : 0);
            ImGui::SliderInt("Start Col", &previewStartCol, 0, appState.beadColumns > previewCols ? appState.beadColumns - previewCols : 0);

            const char* copyModeItems[] = { "Color Name", "Coordinate", "RGB" };
            int copyModeIndex = static_cast<int>(appState.copyMode);
            if (ImGui::Combo("Copy Mode", &copyModeIndex, copyModeItems, IM_ARRAYSIZE(copyModeItems))) {
                appState.copyMode = static_cast<CopyMode>(copyModeIndex);
            }

            const auto& palette = GetActivePalette(appState.paletteStyle);
            if (ImGui::Button("Copy Preview Row")) {
                int rowToCopy = selectedRow >= 0 ? selectedRow : previewStartRow;
                std::string text = BuildPreviewRowText(layout, palette, rowToCopy, previewStartCol, previewCols, appState.copyMode);
                if (!text.empty()) {
                    ImGui::SetClipboardText(text.c_str());
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Copy Preview Column")) {
                int colToCopy = selectedCol >= 0 ? selectedCol : previewStartCol;
                std::string text = BuildPreviewColumnText(layout, palette, previewStartRow, colToCopy, previewRows, appState.copyMode);
                if (!text.empty()) {
                    ImGui::SetClipboardText(text.c_str());
                }
            }

            if (ImGui::BeginTable("LayoutPreviewTable", previewCols + 1, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX)) {
                ImGui::TableSetupColumn("R/C");
                for (int c = 0; c < previewCols; ++c) {
                    ImGui::TableSetupColumn(std::to_string(previewStartCol + c).c_str());
                }
                ImGui::TableHeadersRow();

                for (int r = 0; r < previewRows; ++r) {
                    int rowIndex = previewStartRow + r;
                    if (rowIndex >= beadRows) break;
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%d", rowIndex);
                    for (int c = 0; c < previewCols; ++c) {
                        int colIndex = previewStartCol + c;
                        if (colIndex >= appState.beadColumns) break;
                        ImGui::TableSetColumnIndex(c + 1);
                        int paletteIdx = layout.paletteIndices.empty() ? -1 : layout.paletteIndices[rowIndex][colIndex];
                        if (paletteIdx >= 0 && paletteIdx < static_cast<int>(palette.size())) {
                            std::string shortName = palette[paletteIdx].name;
                            if (shortName.size() > 3) shortName = shortName.substr(0, 3);
                            std::string cellId = shortName + "##cell_" + std::to_string(rowIndex) + "_" + std::to_string(colIndex);
                            bool isSelected = (selectedRow == rowIndex && selectedCol == colIndex);
                            if (ImGui::Selectable(cellId.c_str(), isSelected, ImGuiSelectableFlags_AllowItemOverlap)) {
                                selectedRow = rowIndex;
                                selectedCol = colIndex;
                            }
                            if (ImGui::IsItemHovered()) {
                                ImGui::SetTooltip("[%d,%d] %s", rowIndex, colIndex, palette[paletteIdx].name);
                            }
                        }
                        else {
                            std::string cellId = std::string("--##cell_") + std::to_string(rowIndex) + "_" + std::to_string(colIndex);
                            bool isSelected = (selectedRow == rowIndex && selectedCol == colIndex);
                            if (ImGui::Selectable(cellId.c_str(), isSelected, ImGuiSelectableFlags_AllowItemOverlap)) {
                                selectedRow = rowIndex;
                                selectedCol = colIndex;
                            }
                        }
                    }
                }
                ImGui::EndTable();
            }
        }

        if (selectedRow >= 0 && selectedCol >= 0 && !layout.paletteIndices.empty() && selectedRow < beadRows && selectedCol < appState.beadColumns) {
            const auto& palette = GetActivePalette(appState.paletteStyle);
            int paletteIdx = layout.paletteIndices[selectedRow][selectedCol];
            ImGui::Separator();
            ImGui::Text("Selected Cell: [%d, %d]", selectedRow, selectedCol);
            ImGui::Text("Arrow Keys: Move selection");
            ImGui::Text("Shift + Arrow: Fast move");
            if (paletteIdx >= 0 && paletteIdx < static_cast<int>(palette.size())) {
                const auto& color = palette[paletteIdx];
                ImGui::Text("Color: %s", color.name);
                ImGui::Text("RGB: %d, %d, %d",
                    static_cast<int>(color.color.x * 255.0f),
                    static_cast<int>(color.color.y * 255.0f),
                    static_cast<int>(color.color.z * 255.0f));
            }
            else {
                ImGui::Text("Color: UnknownColor");
            }
        }

        ImGui::End();

        if (inputChanged || appState.dirty) {
            instances = BuildBeadInstances(image, appState, beadRows, recipe, layout);
            glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(instances.size() * sizeof(InstanceData)), instances.data(), GL_DYNAMIC_DRAW);

            const float contentLeft = -0.95f;
            const float contentRight = 0.95f;
            const float contentTop = 0.90f;
            const float contentBottom = -0.90f;
            const float paneGap = 0.03f;
            float splitX = contentLeft + (contentRight - contentLeft) * appState.splitRatio;
            float imagePaneLeft = contentLeft;
            float imagePaneRight = splitX - paneGap;
            std::vector<float> dynamicQuadVertices = GenerateQuadVerticesWithTexCoords(
                imagePaneLeft,
                imagePaneRight,
                contentTop,
                contentBottom,
                0.0f,
                1.0f,
                0.0f,
                1.0f);
            glBindBuffer(GL_ARRAY_BUFFER, imageVbo);
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(dynamicQuadVertices.size() * sizeof(float)), dynamicQuadVertices.data(), GL_STATIC_DRAW);

            glfwSetWindowTitle(window, BuildWindowTitle(appState, image, beadRows, imagePath).c_str());
            appState.dirty = false;
        }

        glClearColor(appState.backgroundColor[0], appState.backgroundColor[1], appState.backgroundColor[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(imageProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, imageTexture);
        glUniform1i(glGetUniformLocation(imageProgram, "uTexture"), 0);
        glUniform1f(glGetUniformLocation(imageProgram, "uAlpha"), appState.imageAlpha);
        glBindVertexArray(imageVao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        glUseProgram(program);
        glUniform1f(glGetUniformLocation(program, "uZoom"), camera.zoom);
        glUniform2f(glGetUniformLocation(program, "uPan"), camera.panX, camera.panY);
        glUniform1f(glGetUniformLocation(program, "uAlpha"), appState.beadAlpha);
        glBindVertexArray(vao);
        glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, static_cast<GLsizei>(circleVertices.size() / 2), static_cast<GLsizei>(instances.size()));
        glBindVertexArray(0);

        ImDrawList* foreground = ImGui::GetForegroundDrawList();
        if (hoveredCell.valid) {
            std::vector<ImVec2> hoverOutline = GenerateOutlineVertices(hoveredCell.centerX, hoveredCell.centerY, hoveredCell.halfWidth, hoveredCell.halfHeight);
            foreground->AddPolyline(hoverOutline.data(), static_cast<int>(hoverOutline.size()), IM_COL32(255, 220, 40, 220), 0, 2.0f);

            if (!layout.paletteIndices.empty() && hoveredCell.row >= 0 && hoveredCell.row < beadRows && hoveredCell.col >= 0 && hoveredCell.col < appState.beadColumns) {
                const auto& palette = GetActivePalette(appState.paletteStyle);
                int paletteIdx = layout.paletteIndices[hoveredCell.row][hoveredCell.col];
                if (paletteIdx >= 0 && paletteIdx < static_cast<int>(palette.size())) {
                    const auto& color = palette[paletteIdx];
                    int colorCount = 0;
                    for (const auto& item : recipe) {
                        if (item.name == color.name) {
                            colorCount = item.count;
                            break;
                        }
                    }
                    ImGui::BeginTooltip();
                    ImGui::Text("[%d,%d] %s", hoveredCell.row, hoveredCell.col, color.name);
                    ImGui::Text("Palette Index: %d", paletteIdx);
                    ImGui::Text("Used Count: %d", colorCount);
                    ImGui::Text("RGB: %d, %d, %d",
                        static_cast<int>(color.color.x * 255.0f),
                        static_cast<int>(color.color.y * 255.0f),
                        static_cast<int>(color.color.z * 255.0f));
                    ImGui::EndTooltip();
                }
            }
        }

        int winWidth = 0;
        int winHeight = 0;
        glfwGetWindowSize(window, &winWidth, &winHeight);
        if (appState.showBeadGrid) {
            std::vector<ImVec2> gridLines = GenerateGridVertices(appState, image, beadRows, camera, winWidth, winHeight);
            ImU32 gridColor = IM_COL32(
                static_cast<int>(appState.gridColor[0] * 255.0f),
                static_cast<int>(appState.gridColor[1] * 255.0f),
                static_cast<int>(appState.gridColor[2] * 255.0f),
                static_cast<int>(appState.gridAlpha * 255.0f));
            for (size_t i = 0; i + 1 < gridLines.size(); i += 2) {
                foreground->AddLine(gridLines[i], gridLines[i + 1], gridColor, 1.0f);
            }
        }

        if (selectedRow >= 0 && selectedCol >= 0) {
            std::vector<ImVec2> imageSampleRect = GenerateImageSampleRectVertices(appState, image, beadRows, selectedRow, selectedCol, winWidth, winHeight);
            if (!imageSampleRect.empty()) {
                foreground->AddPolyline(imageSampleRect.data(), static_cast<int>(imageSampleRect.size()), IM_COL32(120, 255, 120, 255), 0, 3.0f);
            }

            BeadCellVisualInfo selectedCell = GetBeadCellVisualInfo(appState, image, beadRows, selectedRow, selectedCol, camera, winWidth, winHeight);
            if (selectedCell.valid) {
                std::vector<ImVec2> selectOutline = GenerateOutlineVertices(selectedCell.centerX, selectedCell.centerY, selectedCell.halfWidth, selectedCell.halfHeight);
                foreground->AddPolyline(selectOutline.data(), static_cast<int>(selectOutline.size()), IM_COL32(40, 255, 255, 255), 0, 3.0f);
            }
        }
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (!pendingSavePngPath.empty()) {
            int framebufferWidth = 0;
            int framebufferHeight = 0;
            glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
            if (SaveFramebufferToPng(pendingSavePngPath, framebufferWidth, framebufferHeight)) {
                std::cout << "Saved PNG: " << pendingSavePngPath << std::endl;
            }
            else {
                std::cerr << "Failed to save PNG: " << pendingSavePngPath << std::endl;
            }
            pendingSavePngPath.clear();
        }

        if (!pendingSaveBeadPngPath.empty()) {
            int framebufferWidth = 0;
            int framebufferHeight = 0;
            glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
            if (SaveRightPaneToPng(pendingSaveBeadPngPath, framebufferWidth, framebufferHeight, appState.splitRatio)) {
                std::cout << "Saved bead PNG: " << pendingSaveBeadPngPath << std::endl;
            }
            else {
                std::cerr << "Failed to save bead PNG: " << pendingSaveBeadPngPath << std::endl;
            }
            pendingSaveBeadPngPath.clear();
        }

        if (!pendingSaveBeadTightPngPath.empty()) {
            int framebufferWidth = 0;
            int framebufferHeight = 0;
            glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
            if (SaveRightPaneTightToPng(pendingSaveBeadTightPngPath, framebufferWidth, framebufferHeight, appState, image, beadRows)) {
                std::cout << "Saved tight bead PNG: " << pendingSaveBeadTightPngPath << std::endl;
            }
            else {
                std::cerr << "Failed to save tight bead PNG: " << pendingSaveBeadTightPngPath << std::endl;
            }
            pendingSaveBeadTightPngPath.clear();
        }

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
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (comInitialized) {
        CoUninitialize();
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

static std::vector<float> GenerateQuadVerticesWithTexCoords(float left, float right, float top, float bottom, float u0, float u1, float v0, float v1)
{
    return {
        left,  bottom, u0, v1,
        right, bottom, u1, v1,
        right, top,    u1, v0,
        left,  bottom, u0, v1,
        right, top,    u1, v0,
        left,  top,    u0, v0,
    };
}

static std::string OpenImageFileDialog()
{
    char fileName[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = "Image Files\0*.png;*.jpg;*.jpeg;*.bmp\0All Files\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameA(&ofn)) {
        return std::string(fileName);
    }
    return std::string();
}

static std::string SavePngFileDialog()
{
    char fileName[MAX_PATH] = "pixel_bead_render.png";
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = "PNG Files\0*.png\0All Files\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "png";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (GetSaveFileNameA(&ofn)) {
        return std::string(fileName);
    }
    return std::string();
}

static bool SaveFramebufferToPng(const std::string& filename, int width, int height)
{
    if (width <= 0 || height <= 0) {
        return false;
    }

    std::vector<unsigned char> pixels(width * height * 4);
    std::vector<unsigned char> flipped(width * height * 4);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    for (int y = 0; y < height; ++y) {
        std::memcpy(
            flipped.data() + static_cast<size_t>(y) * width * 4,
            pixels.data() + static_cast<size_t>(height - 1 - y) * width * 4,
            static_cast<size_t>(width) * 4);
    }

    int wideLength = MultiByteToWideChar(CP_ACP, 0, filename.c_str(), -1, nullptr, 0);
    if (wideLength <= 0) {
        return false;
    }
    std::wstring wideFilename(static_cast<size_t>(wideLength), L'\0');
    MultiByteToWideChar(CP_ACP, 0, filename.c_str(), -1, &wideFilename[0], wideLength);

    IWICImagingFactory* factory = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapEncoder* encoder = nullptr;
    IWICBitmapFrameEncode* frame = nullptr;
    IPropertyBag2* propertyBag = nullptr;

    bool success = false;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, COINIT_APARTMENTTHREADED, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) goto Cleanup;
    hr = factory->CreateStream(&stream);
    if (FAILED(hr)) goto Cleanup;
    hr = stream->InitializeFromFilename(wideFilename.c_str(), GENERIC_WRITE);
    if (FAILED(hr)) goto Cleanup;
    hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    if (FAILED(hr)) goto Cleanup;
    hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
    if (FAILED(hr)) goto Cleanup;
    hr = encoder->CreateNewFrame(&frame, &propertyBag);
    if (FAILED(hr)) goto Cleanup;
    hr = frame->Initialize(propertyBag);
    if (FAILED(hr)) goto Cleanup;
    hr = frame->SetSize(width, height);
    if (FAILED(hr)) goto Cleanup;
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppRGBA;
    hr = frame->SetPixelFormat(&format);
    if (FAILED(hr)) goto Cleanup;
    hr = frame->WritePixels(height, width * 4, static_cast<UINT>(flipped.size()), flipped.data());
    if (FAILED(hr)) goto Cleanup;
    hr = frame->Commit();
    if (FAILED(hr)) goto Cleanup;
    hr = encoder->Commit();
    if (FAILED(hr)) goto Cleanup;
    success = true;
Cleanup:
    if (propertyBag) propertyBag->Release();
    if (frame) frame->Release();
    if (encoder) encoder->Release();
    if (stream) stream->Release();
    if (factory) factory->Release();
    return success;
}

static bool SaveRightPaneToPng(const std::string& filename, int framebufferWidth, int framebufferHeight, float splitRatio)
{
    if (framebufferWidth <= 0 || framebufferHeight <= 0) {
        return false;
    }

    int x0 = std::max(0, std::min(static_cast<int>(std::round(framebufferWidth * splitRatio)), framebufferWidth - 1));
    int rightWidth = framebufferWidth - x0;
    if (rightWidth <= 0) {
        return false;
    }

    std::vector<unsigned char> pixels(static_cast<size_t>(rightWidth) * framebufferHeight * 4);
    std::vector<unsigned char> flipped(static_cast<size_t>(rightWidth) * framebufferHeight * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(x0, 0, rightWidth, framebufferHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    for (int y = 0; y < framebufferHeight; ++y) {
        std::memcpy(flipped.data() + static_cast<size_t>(y) * rightWidth * 4,
                    pixels.data() + static_cast<size_t>(framebufferHeight - 1 - y) * rightWidth * 4,
                    static_cast<size_t>(rightWidth) * 4);
    }
    return SaveFramebufferToPng(filename, rightWidth, framebufferHeight) ? true : ([&]() {
        int wideLength = MultiByteToWideChar(CP_ACP, 0, filename.c_str(), -1, nullptr, 0);
        if (wideLength <= 0) return false;
        std::wstring wideFilename(static_cast<size_t>(wideLength), L'\0');
        MultiByteToWideChar(CP_ACP, 0, filename.c_str(), -1, &wideFilename[0], wideLength);
        IWICImagingFactory* factory = nullptr;
        IWICStream* stream = nullptr;
        IWICBitmapEncoder* encoder = nullptr;
        IWICBitmapFrameEncode* frame = nullptr;
        IPropertyBag2* propertyBag = nullptr;
        bool success = false;
        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, COINIT_APARTMENTTHREADED, IID_PPV_ARGS(&factory));
        if (FAILED(hr)) goto Cleanup;
        hr = factory->CreateStream(&stream); if (FAILED(hr)) goto Cleanup;
        hr = stream->InitializeFromFilename(wideFilename.c_str(), GENERIC_WRITE); if (FAILED(hr)) goto Cleanup;
        hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder); if (FAILED(hr)) goto Cleanup;
        hr = encoder->Initialize(stream, WICBitmapEncoderNoCache); if (FAILED(hr)) goto Cleanup;
        hr = encoder->CreateNewFrame(&frame, &propertyBag); if (FAILED(hr)) goto Cleanup;
        hr = frame->Initialize(propertyBag); if (FAILED(hr)) goto Cleanup;
        hr = frame->SetSize(rightWidth, framebufferHeight); if (FAILED(hr)) goto Cleanup;
        WICPixelFormatGUID format = GUID_WICPixelFormat32bppRGBA;
        hr = frame->SetPixelFormat(&format); if (FAILED(hr)) goto Cleanup;
        hr = frame->WritePixels(framebufferHeight, rightWidth * 4, static_cast<UINT>(flipped.size()), flipped.data()); if (FAILED(hr)) goto Cleanup;
        hr = frame->Commit(); if (FAILED(hr)) goto Cleanup;
        hr = encoder->Commit(); if (FAILED(hr)) goto Cleanup;
        success = true;
Cleanup:
        if (propertyBag) propertyBag->Release();
        if (frame) frame->Release();
        if (encoder) encoder->Release();
        if (stream) stream->Release();
        if (factory) factory->Release();
        return success;
    })();
}

static bool SaveRightPaneTightToPng(const std::string& filename, int framebufferWidth, int framebufferHeight, const AppState& state, const ImageData& image, int beadRows)
{
    if (framebufferWidth <= 0 || framebufferHeight <= 0 || beadRows <= 0 || image.width <= 0 || image.height <= 0) {
        return false;
    }

    const float contentLeft = -0.95f;
    const float contentRight = 0.95f;
    const float contentTop = 0.90f;
    const float contentBottom = -0.90f;
    const float paneGap = 0.03f;
    float splitX = contentLeft + (contentRight - contentLeft) * state.splitRatio;
    float beadPaneLeft = splitX + paneGap;
    float beadPaneRight = contentRight;
    float beadPaneWidth = std::max(0.10f, beadPaneRight - beadPaneLeft);
    float beadPaneHeight = contentTop - contentBottom;

    float aspect = static_cast<float>(image.width) / static_cast<float>(image.height);
    float boardWidth = beadPaneWidth;
    float boardHeight = boardWidth / aspect;
    if (boardHeight > beadPaneHeight) {
        boardHeight = beadPaneHeight;
        boardWidth = boardHeight * aspect;
    }

    float boardCenterX = beadPaneLeft + beadPaneWidth * 0.5f;
    float boardCenterY = (contentTop + contentBottom) * 0.5f;
    float left = boardCenterX - boardWidth * 0.5f;
    float right = boardCenterX + boardWidth * 0.5f;
    float bottom = boardCenterY - boardHeight * 0.5f;
    float top = boardCenterY + boardHeight * 0.5f;

    int x0 = std::max(0, std::min(static_cast<int>(std::floor((left * 0.5f + 0.5f) * framebufferWidth)), framebufferWidth - 1));
    int x1 = std::max(1, std::min(static_cast<int>(std::ceil((right * 0.5f + 0.5f) * framebufferWidth)), framebufferWidth));
    int y0 = std::max(0, std::min(static_cast<int>(std::floor((bottom * 0.5f + 0.5f) * framebufferHeight)), framebufferHeight - 1));
    int y1 = std::max(1, std::min(static_cast<int>(std::ceil((top * 0.5f + 0.5f) * framebufferHeight)), framebufferHeight));

    int width = x1 - x0;
    int height = y1 - y0;
    if (width <= 0 || height <= 0) {
        return false;
    }

    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
    std::vector<unsigned char> flipped(static_cast<size_t>(width) * height * 4);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(x0, y0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    for (int y = 0; y < height; ++y) {
        std::memcpy(
            flipped.data() + static_cast<size_t>(y) * width * 4,
            pixels.data() + static_cast<size_t>(height - 1 - y) * width * 4,
            static_cast<size_t>(width) * 4);
    }

    if (state.transparentTightExport) {
        const int bgR = static_cast<int>(state.backgroundColor[0] * 255.0f + 0.5f);
        const int bgG = static_cast<int>(state.backgroundColor[1] * 255.0f + 0.5f);
        const int bgB = static_cast<int>(state.backgroundColor[2] * 255.0f + 0.5f);
        for (size_t i = 0; i + 3 < flipped.size(); i += 4) {
            int r = flipped[i + 0];
            int g = flipped[i + 1];
            int b = flipped[i + 2];
            int maxDiff = std::max({ std::abs(r - bgR), std::abs(g - bgG), std::abs(b - bgB) });
            if (maxDiff <= 2) {
                flipped[i + 3] = 0;
                continue;
            }

            int alpha = std::max(0, std::min(maxDiff * 6, 255));
            if (alpha < 255) {
                auto recover = [alpha](int c, int bg) -> unsigned char {
                    float value = bg + (c - bg) * (255.0f / std::max(1, alpha));
                    value = std::max(0.0f, std::min(value, 255.0f));
                    return static_cast<unsigned char>(value);
                };
                flipped[i + 0] = recover(r, bgR);
                flipped[i + 1] = recover(g, bgG);
                flipped[i + 2] = recover(b, bgB);
                flipped[i + 3] = static_cast<unsigned char>(alpha);
            }
        }
    }

    int wideLength = MultiByteToWideChar(CP_ACP, 0, filename.c_str(), -1, nullptr, 0);
    if (wideLength <= 0) {
        return false;
    }
    std::wstring wideFilename(static_cast<size_t>(wideLength), L'\0');
    MultiByteToWideChar(CP_ACP, 0, filename.c_str(), -1, &wideFilename[0], wideLength);

    IWICImagingFactory* factory = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapEncoder* encoder = nullptr;
    IWICBitmapFrameEncode* frame = nullptr;
    IPropertyBag2* propertyBag = nullptr;

    bool success = false;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) goto Cleanup;
    hr = factory->CreateStream(&stream);
    if (FAILED(hr)) goto Cleanup;
    hr = stream->InitializeFromFilename(wideFilename.c_str(), GENERIC_WRITE);
    if (FAILED(hr)) goto Cleanup;
    hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    if (FAILED(hr)) goto Cleanup;
    hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
    if (FAILED(hr)) goto Cleanup;
    hr = encoder->CreateNewFrame(&frame, &propertyBag);
    if (FAILED(hr)) goto Cleanup;
    hr = frame->Initialize(propertyBag);
    if (FAILED(hr)) goto Cleanup;
    hr = frame->SetSize(width, height);
    if (FAILED(hr)) goto Cleanup;
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppRGBA;
    hr = frame->SetPixelFormat(&format);
    if (FAILED(hr)) goto Cleanup;
    hr = frame->WritePixels(height, width * 4, static_cast<UINT>(flipped.size()), flipped.data());
    if (FAILED(hr)) goto Cleanup;
    hr = frame->Commit();
    if (FAILED(hr)) goto Cleanup;
    hr = encoder->Commit();
    if (FAILED(hr)) goto Cleanup;
    success = true;

Cleanup:
    if (propertyBag) propertyBag->Release();
    if (frame) frame->Release();
    if (encoder) encoder->Release();
    if (stream) stream->Release();
    if (factory) factory->Release();
    return success;
}

// 尝试从鼠标位置选择珠子单元格
static bool TrySelectBeadCellFromMouse(GLFWwindow* window, const AppState& state, const ImageData& image, int beadRows, const CameraState& camera, int& selectedRow, int& selectedCol)
{
    if (!window || beadRows <= 0 || image.width <= 0 || image.height <= 0) {
        return false;
    }

    double mouseX = 0.0;
    double mouseY = 0.0;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    int winWidth = 0;
    int winHeight = 0;
    glfwGetWindowSize(window, &winWidth, &winHeight);
    if (winWidth <= 0 || winHeight <= 0) {
        return false;
    }

    float ndcX = static_cast<float>(mouseX / winWidth) * 2.0f - 1.0f;
    float ndcY = 1.0f - static_cast<float>(mouseY / winHeight) * 2.0f;
    float worldX = (ndcX - camera.panX) / camera.zoom;
    float worldY = (ndcY - camera.panY) / camera.zoom;

    const float contentLeft = -0.95f;
    const float contentRight = 0.95f;
    const float contentTop = 0.90f;
    const float contentBottom = -0.90f;
    const float paneGap = 0.03f;
    float splitX = contentLeft + (contentRight - contentLeft) * state.splitRatio;
    float beadPaneLeft = splitX + paneGap;
    float beadPaneRight = contentRight;
    float beadPaneWidth = std::max(0.10f, beadPaneRight - beadPaneLeft);
    float beadPaneHeight = contentTop - contentBottom;

    float aspect = static_cast<float>(image.width) / static_cast<float>(image.height);
    float boardWidth = beadPaneWidth;
    float boardHeight = boardWidth / aspect;
    if (boardHeight > beadPaneHeight) {
        boardHeight = beadPaneHeight;
        boardWidth = boardHeight * aspect;
    }

    float boardCenterX = beadPaneLeft + beadPaneWidth * 0.5f;
    float boardCenterY = (contentTop + contentBottom) * 0.5f;
    float left = boardCenterX - boardWidth * 0.5f;
    float right = boardCenterX + boardWidth * 0.5f;
    float bottom = boardCenterY - boardHeight * 0.5f;
    float top = boardCenterY + boardHeight * 0.5f;

    if (worldX < left || worldX > right || worldY < bottom || worldY > top) {
        return false;
    }

    float cellWidth = boardWidth / static_cast<float>(state.beadColumns);
    float cellHeight = boardHeight / static_cast<float>(beadRows);
    int col = std::max(0, std::min(static_cast<int>((worldX - left) / cellWidth), state.beadColumns - 1));
    int row = std::max(0, std::min(static_cast<int>((top - worldY) / cellHeight), beadRows - 1));

    selectedRow = row;
    selectedCol = col;
    return true;
}

static bool QueryBeadCellFromMouse(GLFWwindow* window, const AppState& state, const ImageData& image, int beadRows, const CameraState& camera, BeadCellVisualInfo& outInfo)
{
    outInfo = {};
    int row = -1;
    int col = -1;
    if (!TrySelectBeadCellFromMouse(window, state, image, beadRows, camera, row, col)) {
        return false;
    }

    int winWidth = 0;
    int winHeight = 0;
    glfwGetWindowSize(window, &winWidth, &winHeight);
    outInfo = GetBeadCellVisualInfo(state, image, beadRows, row, col, camera, winWidth, winHeight);
    return outInfo.valid;
}

static BeadCellVisualInfo GetBeadCellVisualInfo(const AppState& state, const ImageData& image, int beadRows, int row, int col, const CameraState& camera, int winWidth, int winHeight)
{
    BeadCellVisualInfo info;
    if (beadRows <= 0 || row < 0 || col < 0 || row >= beadRows || col >= state.beadColumns || winWidth <= 0 || winHeight <= 0 || image.width <= 0 || image.height <= 0) {
        return info;
    }

    const float contentLeft = -0.95f;
    const float contentRight = 0.95f;
    const float contentTop = 0.90f;
    const float contentBottom = -0.90f;
    const float paneGap = 0.03f;
    float splitX = contentLeft + (contentRight - contentLeft) * state.splitRatio;
    float beadPaneLeft = splitX + paneGap;
    float beadPaneRight = contentRight;
    float beadPaneWidth = std::max(0.10f, beadPaneRight - beadPaneLeft);
    float beadPaneHeight = contentTop - contentBottom;

    float aspect = static_cast<float>(image.width) / static_cast<float>(image.height);
    float boardWidth = beadPaneWidth;
    float boardHeight = boardWidth / aspect;
    if (boardHeight > beadPaneHeight) {
        boardHeight = beadPaneHeight;
        boardWidth = boardHeight * aspect;
    }

    float boardCenterX = beadPaneLeft + beadPaneWidth * 0.5f;
    float boardCenterY = (contentTop + contentBottom) * 0.5f;
    float left = boardCenterX - boardWidth * 0.5f;
    float top = boardCenterY + boardHeight * 0.5f;
    float cellWidth = boardWidth / static_cast<float>(state.beadColumns);
    float cellHeight = boardHeight / static_cast<float>(beadRows);

    float cellLeft = left + static_cast<float>(col) * cellWidth;
    float cellRight = cellLeft + cellWidth;
    float cellTop = top - static_cast<float>(row) * cellHeight;
    float cellBottom = cellTop - cellHeight;

    auto toScreenX = [winWidth, &camera](float x) {
        float ndc = x * camera.zoom + camera.panX;
        return (ndc * 0.5f + 0.5f) * static_cast<float>(winWidth);
    };
    auto toScreenY = [winHeight, &camera](float y) {
        float ndc = y * camera.zoom + camera.panY;
        return (1.0f - (ndc * 0.5f + 0.5f)) * static_cast<float>(winHeight);
    };

    float screenLeft = toScreenX(cellLeft);
    float screenRight = toScreenX(cellRight);
    float screenTop = toScreenY(cellTop);
    float screenBottom = toScreenY(cellBottom);

    info.valid = true;
    info.row = row;
    info.col = col;
    info.centerX = 0.5f * (screenLeft + screenRight);
    info.centerY = 0.5f * (screenTop + screenBottom);
    info.halfWidth = 0.5f * std::abs(screenRight - screenLeft);
    info.halfHeight = 0.5f * std::abs(screenBottom - screenTop);
    return info;
}

static std::vector<ImVec2> GenerateOutlineVertices(float centerX, float centerY, float halfWidth, float halfHeight)
{
    return {
        ImVec2(centerX - halfWidth, centerY - halfHeight),
        ImVec2(centerX + halfWidth, centerY - halfHeight),
        ImVec2(centerX + halfWidth, centerY + halfHeight),
        ImVec2(centerX - halfWidth, centerY + halfHeight),
        ImVec2(centerX - halfWidth, centerY - halfHeight),
    };
}

static std::vector<ImVec2> GenerateGridVertices(const AppState& state, const ImageData& image, int beadRows, const CameraState& camera, int winWidth, int winHeight)
{
    std::vector<ImVec2> vertices;
    if (beadRows <= 0 || state.beadColumns <= 0 || image.width <= 0 || image.height <= 0 || winWidth <= 0 || winHeight <= 0) {
        return vertices;
    }

    BeadCellVisualInfo firstCell = GetBeadCellVisualInfo(state, image, beadRows, 0, 0, camera, winWidth, winHeight);
    BeadCellVisualInfo lastCell = GetBeadCellVisualInfo(state, image, beadRows, beadRows - 1, state.beadColumns - 1, camera, winWidth, winHeight);
    if (!firstCell.valid || !lastCell.valid) {
        return vertices;
    }

    float left = firstCell.centerX - firstCell.halfWidth;
    float right = lastCell.centerX + lastCell.halfWidth;
    float top = firstCell.centerY - firstCell.halfHeight;
    float bottom = lastCell.centerY + lastCell.halfHeight;
    float cellWidth = firstCell.halfWidth * 2.0f;
    float cellHeight = firstCell.halfHeight * 2.0f;

    vertices.reserve(static_cast<size_t>((state.beadColumns + beadRows + 2) * 2));
    for (int col = 0; col <= state.beadColumns; ++col) {
        float x = left + cellWidth * static_cast<float>(col);
        vertices.emplace_back(x, top);
        vertices.emplace_back(x, bottom);
    }
    for (int row = 0; row <= beadRows; ++row) {
        float y = top + cellHeight * static_cast<float>(row);
        vertices.emplace_back(left, y);
        vertices.emplace_back(right, y);
    }
    return vertices;
}

static std::vector<ImVec2> GenerateImageSampleRectVertices(const AppState& state, const ImageData& image, int beadRows, int row, int col, int winWidth, int winHeight)
{
    std::vector<ImVec2> vertices;
    if (image.width <= 0 || image.height <= 0 || beadRows <= 0 || row < 0 || col < 0 || col >= state.beadColumns || row >= beadRows || winWidth <= 0 || winHeight <= 0) {
        return vertices;
    }

    const float contentLeft = -0.95f;
    const float contentRight = 0.95f;
    const float contentTop = 0.90f;
    const float contentBottom = -0.90f;
    const float paneGap = 0.03f;
    float splitX = contentLeft + (contentRight - contentLeft) * state.splitRatio;
    float imagePaneLeft = contentLeft;
    float imagePaneRight = splitX - paneGap;

    float u0 = static_cast<float>(col) / static_cast<float>(state.beadColumns);
    float u1 = static_cast<float>(col + 1) / static_cast<float>(state.beadColumns);
    float v0 = static_cast<float>(row) / static_cast<float>(beadRows);
    float v1 = static_cast<float>(row + 1) / static_cast<float>(beadRows);

    float leftNdc = imagePaneLeft + (imagePaneRight - imagePaneLeft) * u0;
    float rightNdc = imagePaneLeft + (imagePaneRight - imagePaneLeft) * u1;
    float topNdc = contentTop + (contentBottom - contentTop) * v0;
    float bottomNdc = contentTop + (contentBottom - contentTop) * v1;

    auto toScreenX = [winWidth](float x) {
        return (x * 0.5f + 0.5f) * static_cast<float>(winWidth);
    };
    auto toScreenY = [winHeight](float y) {
        return (1.0f - (y * 0.5f + 0.5f)) * static_cast<float>(winHeight);
    };

    vertices.emplace_back(toScreenX(leftNdc), toScreenY(topNdc));
    vertices.emplace_back(toScreenX(rightNdc), toScreenY(topNdc));
    vertices.emplace_back(toScreenX(rightNdc), toScreenY(bottomNdc));
    vertices.emplace_back(toScreenX(leftNdc), toScreenY(bottomNdc));
    vertices.emplace_back(toScreenX(leftNdc), toScreenY(topNdc));
    return vertices;
}

static std::string BuildPreviewRowText(const BeadLayoutData& layout, const std::vector<BeadColor>& palette, int rowIndex, int startCol, int count, CopyMode copyMode)
{
    if (rowIndex < 0 || rowIndex >= static_cast<int>(layout.paletteIndices.size())) {
        return std::string();
    }

    std::ostringstream oss;
    oss << "Row " << rowIndex << ": ";
    const auto& row = layout.paletteIndices[rowIndex];
    int endCol = std::min(startCol + count, static_cast<int>(row.size()));
    for (int col = startCol; col < endCol; ++col) {
        if (copyMode == CopyMode::Coordinate) {
            oss << '[' << rowIndex << ',' << col << ']';
        }
        else {
            int index = row[col];
            if (index >= 0 && index < static_cast<int>(palette.size())) {
                if (copyMode == CopyMode::RGB) {
                    oss << static_cast<int>(palette[index].color.x * 255.0f) << ','
                        << static_cast<int>(palette[index].color.y * 255.0f) << ','
                        << static_cast<int>(palette[index].color.z * 255.0f);
                }
                else {
                    oss << palette[index].name;
                }
            }
            else {
                oss << "UnknownColor";
            }
        }
        if (col + 1 < endCol) {
            oss << " | ";
        }
    }
    return oss.str();
}

static std::string BuildPreviewColumnText(const BeadLayoutData& layout, const std::vector<BeadColor>& palette, int startRow, int colIndex, int count, CopyMode copyMode)
{
    if (layout.paletteIndices.empty() || colIndex < 0) {
        return std::string();
    }

    std::ostringstream oss;
    oss << "Column " << colIndex << ": ";
    int endRow = std::min(startRow + count, static_cast<int>(layout.paletteIndices.size()));
    for (int row = startRow; row < endRow; ++row) {
        if (colIndex >= static_cast<int>(layout.paletteIndices[row].size())) {
            break;
        }
        if (copyMode == CopyMode::Coordinate) {
            oss << '[' << row << ',' << colIndex << ']';
        }
        else {
            int index = layout.paletteIndices[row][colIndex];
            if (index >= 0 && index < static_cast<int>(palette.size())) {
                if (copyMode == CopyMode::RGB) {
                    oss << static_cast<int>(palette[index].color.x * 255.0f) << ','
                        << static_cast<int>(palette[index].color.y * 255.0f) << ','
                        << static_cast<int>(palette[index].color.z * 255.0f);
                }
                else {
                    oss << palette[index].name;
                }
            }
            else {
                oss << "UnknownColor";
            }
        }
        if (row + 1 < endRow) {
            oss << " | ";
        }
    }
    return oss.str();
}
