file(GLOB_RECURSE IMGUI ./ *.cpp)

# 将APP中的所有cpp编译为fw这个lib库
add_library(imguilib ${IMGUI})