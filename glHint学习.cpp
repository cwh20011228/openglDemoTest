#include <iostream>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

static const char* kVertexShaderSrc = R"GLSL(
#version 330 core
layout(location = 0) in vec2 aPos;
uniform vec2 uOffset;
void main()
{
    gl_Position = vec4(aPos + uOffset, 0.0, 1.0);
}
)GLSL";

static const char* kFragmentShaderSrc = R"GLSL(
#version 330 core
out vec4 FragColor;
uniform vec3 uColor;
void main()
{
    FragColor = vec4(uColor, 1.0);
}
)GLSL";

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
        std::cerr << "Shader compile error: " << log.data() << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint CreateProgram()
{
    GLuint vs = CompileShader(GL_VERTEX_SHADER, kVertexShaderSrc);
    if (!vs) return 0;
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFragmentShaderSrc);
    if (!fs) {
        glDeleteShader(vs);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint logLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
        std::vector<char> log(static_cast<size_t>(std::max(1, logLen)));
        glGetProgramInfoLog(program, logLen, nullptr, log.data());
        std::cerr << "Program link error: " << log.data() << std::endl;
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

int main()
{
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW." << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1200, 700, "glHint line smoothing compare", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window." << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to init GLAD." << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    GLuint program = CreateProgram();
    if (!program) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    const std::vector<float> vertices = {
        -0.9f, -0.7f,   0.9f,  0.7f,
        -0.9f,  0.7f,   0.9f, -0.7f,
        -0.95f, 0.0f,   0.95f, 0.0f,
        -0.5f, -0.9f,   0.5f,  0.9f,
        -0.75f, 0.9f,   0.75f,-0.9f,
    };

    GLuint vao = 0;
    GLuint vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    GLint offsetLoc = glGetUniformLocation(program, "uOffset");
    GLint colorLoc = glGetUniformLocation(program, "uColor");

    bool useHintOnRight = true;
    bool hPressedLast = false;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        bool hPressed = glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS;
        if (hPressed && !hPressedLast) {
            useHintOnRight = !useHintOnRight;
            std::cout << "Right half glHint: " << (useHintOnRight ? "ON" : "OFF") << std::endl;
        }
        hPressedLast = hPressed;

        int fbWidth = 0;
        int fbHeight = 0;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

        glClearColor(0.08f, 0.08f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(program);
        glBindVertexArray(vao);

        glLineWidth(4.0f);

        int halfWidth = fbWidth / 2;

        glViewport(0, 0, halfWidth, fbHeight);
        glDisable(GL_LINE_SMOOTH);
        glHint(GL_LINE_SMOOTH_HINT, GL_DONT_CARE);
        glUniform2f(offsetLoc, 0.0f, 0.0f);
        glUniform3f(colorLoc, 1.0f, 0.35f, 0.25f);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices.size() / 2));

        glViewport(halfWidth, 0, fbWidth - halfWidth, fbHeight);
        glEnable(GL_LINE_SMOOTH);
        glHint(GL_LINE_SMOOTH_HINT, useHintOnRight ? GL_NICEST : GL_FASTEST);
        glUniform2f(offsetLoc, 0.0f, 0.0f);
        glUniform3f(colorLoc, 0.25f, 0.85f, 1.0f);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices.size() / 2));

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}