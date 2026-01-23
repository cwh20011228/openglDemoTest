# 需求的最低cmake程序版本
cmake_minimum_required(VERSION 3.12)

# 本工程的名字
project(LearnOpenGL)

# 本工程支持的c++版本库
set(CMAKE_CXX_STARDARD 17)

# 往项目中加入一个全局的预编译宏
add_definitions(-DDEBUG)

# 把需要拷贝的资源路径都放在ASSETS里面
file(GLOB ASSETS  "./assets" )

# 把ASSETS指代的目录集合的内容，都拷贝到可执行程序目录中
file(COPY ${ASSETS} DESTINATION ${CMAKE_BINARY_DIR})


# 指定编译过程中应该搜索头文件的目录
include_directories(
    SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/include
)

# 指定链接库的目录
link_directories(
    SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/lib
)

# 将wrapper文件夹纳入编译系统
add_subdirectory(wrapper)


# 本工程所有cpp文件编译，链接，生成exe
add_executable(MSAA_TestDemo  "main.cpp" "glad.c" ) 

# 将glfw3.lib wrapper app链接到openglStudy
target_link_libraries(MSAA_TestDemo glfw3.lib)
#target_link_libraries(openglStudy glfw3.lib assimp-vc143-mtd.lib zlibstaticd.lib  wrapper imguilib)