#include <iostream>
#include <vector>
#include <string>
#include <initializer_list>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include "./stb_image.h"

#include "./wrapper/checkError.h"

// 窗口的逻辑尺寸
int windowsWidth = 800;
int windowsHeight = 600;

struct ShaderSource {
    GLenum type;
    const std::string* src;     // 允许nullptr或empty，表示跳过该stage
};

// 声明函数
static GLuint CompileShader(GLenum type, const std::string& source);
static GLuint LinkProgram(GLuint vs, GLuint fs);
static GLuint LinkProgram(GLuint vs, GLuint gs, GLuint fs);
static GLuint LinkProgram(std::initializer_list<GLuint> shaders);
static GLuint CreateProgramFromSources(const std::string& vertexSrc, const std::string& fragmentSrc);
static GLuint CreateProgramFromSources(const std::string& vertexSrc, const std::string& fragmentSrc, const std::string* geometrySrc);
static GLuint CreateProgramFromSources(std::initializer_list<ShaderSource> stages);

/*
* GLFW的头文件中回调函数的定义：
* typedef void (*GLFWframebuffersizefun)(GLFWwindow* window, int width, int height);
*/
void framebuffer_size_callback(GLFWwindow* window, int width, int height);

void SetMultisampleEnabled(bool enabled);

static const char* ShaderTypeName(GLenum type) {
    switch (type){
    case GL_VERTEX_SHADER:          return "VERTEX";
    case GL_FRAGMENT_SHADER:        return "FRAGMENT";
    case GL_GEOMETRY_SHADER:        return "GEOMETRY";
    case GL_TESS_CONTROL_SHADER:    return "TESS_CONTROL";
    case GL_TESS_EVALUATION_SHADER: return "TESS_EVALUATION";
    case GL_COMPUTE_SHADER:         return "COMPUTE";
    default:                        return "UNKNOWN";
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

		std::string log(logLen > 0 ? logLen : 1 ,'\0');
        if (logLen > 0){
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

    // 检查链接错误
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint logLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);

        std::string log(logLen, '\0');
        if (!log.empty()) {
            glGetProgramInfoLog(program, logLen, nullptr, &log[0]);
        }

        std::cerr << "[Program Link Error]\n" << log << "\n";

        glDeleteProgram(program);
        return 0;
    }

    glDetachShader(program, vs);
    glDetachShader(program, fs);

    return program;
}

static GLuint LinkProgram(GLuint vs, GLuint gs, GLuint fs) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, gs);
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

        std::cerr << "[Program Link Error]\n" << log << "\n";

        glDeleteProgram(program);
        return 0;
    }

    glDetachShader(program, vs);
    glDetachShader(program, gs);
    glDetachShader(program, fs);
    return program;
}

static GLuint LinkProgram(std::initializer_list<GLuint> shaders) {
    GLuint program = glCreateProgram();

    for (GLuint s : shaders) {
        if (s) {
            glAttachShader(program, s);
        }
    }

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

        for (GLuint s : shaders) {
            if (s) glDetachShader(program, s);
        }

        glDeleteProgram(program);
        return 0;
    }

    for (GLuint s : shaders) {
        if (s) glDetachShader(program, s);
    }

    return program;
}

static GLuint CreateProgramFromSources(
    const std::string& vertexSrc,
    const std::string& fragmentSrc) {
    GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexSrc);
    if (!vs) return 0;

    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentSrc);
    if (!fs) {
        glDeleteShader(vs);
        return 0;
    }

    GLuint program = LinkProgram(vs, fs);

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program; // 0 表示失败
}

static GLuint CreateProgramFromSources(
    const std::string& vertexSrc,
    const std::string& fragmentSrc,
    const std::string* geometrySrc) {
    GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexSrc);
    if (!vs) return 0;

    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentSrc);
    if (!fs) {
        glDeleteShader(vs);
        return 0;
    }

    GLuint gs = 0;
    if (geometrySrc && !geometrySrc->empty()) {
        gs = CompileShader(GL_GEOMETRY_SHADER, *geometrySrc);
        if (!gs) {
            glDeleteShader(vs);
            glDeleteShader(fs);
            return 0;
        }
    }

    GLuint program = 0;
    if (gs) {
        program = LinkProgram(vs, gs, fs);
    }
    else {
        program = LinkProgram(vs, fs);
    }

    // 清理 shader
    if (gs) {
        glDeleteShader(gs);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

/* 用法示例：
* GLuint program = CreateProgramFromSources({
    {GL_VERTEX_SHADER,   &vertexShaderSource},
    {GL_FRAGMENT_SHADER, &fragmentShaderSource},
    });
*/
static GLuint CreateProgramFromSources(std::initializer_list<ShaderSource> stages) {
    std::vector<GLuint> compiled;
    compiled.reserve(stages.size());

    // (1) compile
    for (const auto& st : stages) {
        if (!st.src || st.src->empty()) {       // 跳过空stage
            continue;
        }

        GLuint sh = CompileShader(st.type, *st.src);
        if (!sh) {
            // 编译失败，清理已经编译成功的shader
            for (GLuint s : compiled) {
                glDeleteShader(s);
            }

            return 0;
        }
        compiled.push_back(sh);
    }

    // (2) link：initializer_list 需要编译期列表，我们用临时 vector 来 attach/link
    GLuint program = glCreateProgram();
    for (GLuint s : compiled) {
        glAttachShader(program, s);
    }
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

        for (GLuint s : compiled) {
            if (s) glDetachShader(program, s);
        }

        glDeleteProgram(program);
        for (GLuint s : compiled) {
            glDeleteShader(s);
        }

        program = 0;
        return 0;
    }

    for (GLuint s : compiled) {
        if (s) glDetachShader(program, s);
    }

    for (GLuint s : compiled) {
        glDeleteShader(s);
    }

    return program;
}

void SetMultisampleEnabled(bool enabled)
{
    if (enabled) {
        glEnable(GL_MULTISAMPLE);
    }
    else {
        glDisable(GL_MULTISAMPLE);
    }   
}

const std::string vertexShaderSource = R"GLSL(
#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
out vec3 ourColor;

void main() {
	gl_Position = vec4(aPos, 1.0);
	ourColor = aColor;
}
)GLSL";

const std::string fragmentShaderSource = R"GLSL(
#version 460 core
out vec4 FragColor;
in vec3 ourColor;

void main() {
	FragColor = vec4(ourColor, 1.0f);
}
)GLSL";



int main(int argc, char* argv[]) {
    // (1)初始化GLFW
	glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// 向 GLFW “请求”默认 framebuffer 使用 4x MSAA.
    /*
    * glfw的msaa是在创建窗口之前决定的，发生在OpenGL context创建之前，默认framebuffer分配之前。
	* 默认framebuffer已经固定了多重采样的采样数之后，是无法动态更改的。
    */
    glfwWindowHint(GLFW_SAMPLES, 4);

    // (2)创建窗口
    GLFWwindow* window = glfwCreateWindow(windowsWidth, windowsHeight, "OpenGL Default MSAA Demo", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // (3)加载OpenGL函数指针
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    int maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    std::cout << "GL_MAX_SAMPLES = " << maxSamples << std::endl;

	int currentSamples = 0;
    glGetIntegerv(GL_SAMPLES, &currentSamples);
    std::cout << "Current Samples = " << currentSamples << std::endl;

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    // 初始化时手动同步一次 viewport（防止回调还没触发）
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    // 开启 OpenGL 的多重采样状态
    glEnable(GL_MULTISAMPLE);

    // (4)创建着色器程序
    //GLuint shaderProgram = CreateProgramFromSources(vertexShaderSource, fragmentShaderSource);

    GLuint shaderProgram = CreateProgramFromSources(
        {
            {GL_VERTEX_SHADER,   &vertexShaderSource},
		    {GL_FRAGMENT_SHADER, &fragmentShaderSource},
        });

    if (!shaderProgram) {
        std::cerr << "Failed to create shader program" << std::endl;
        return -1;
    }

    // (5) 设置顶点数据
    float vertices[] = {
        // 位置              // 颜色
         0.0f,  0.5f, 0.0f,  1.0f, 0.0f, 0.0f, // 顶部红色
        -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f, // 左下绿色
         0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f  // 右下蓝色
    };

    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // 位置属性
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // 颜色属性
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 主循环
    while (!glfwWindowShouldClose(window)) {
		// 按键0表示开启多重采样，按键1表示关闭多重采样
        if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS) {
            SetMultisampleEnabled(true);
        }

        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
            SetMultisampleEnabled(false);
        }

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // 清理
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    glfwDestroyWindow(window);
    glfwTerminate();
	return 0;
}