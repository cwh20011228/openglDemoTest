# 递归将本文件夹下的所有cpp放到变量WRAPPER中
file(GLOB_RECURSE WRAPPER ./ *.cpp)

# 将WRAPPER中的所有cpp编译为 wrapper.lib 静态库
add_library(wrapper ${WRAPPER})