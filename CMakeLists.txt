cmake_minimum_required(VERSION 3.10)  # 设置 CMake 最低版本要求
project(danish)                     # 设置项目名称

set(CMAKE_CXX_STANDARD 14)            # 设置 C++ 标准（可根据需要更改）
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 包含头文件目录
# include_directories(include)

# 添加源文件
set(SOURCES
	src/main.c                      # 添加你的源文件
	src/page.c
	src/ssh.c
	src/site.cpp
   )

# 添加库和依赖项
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
include_directories(${GTK3_INCLUDE_DIRS})
pkg_check_modules(VTE REQUIRED vte-2.91)
include_directories(${VTE_INCLUDE_DIRS})
pkg_check_modules(TINYXML REQUIRED tinyxml)

set(include
	include/
	${GTK3_INCLUDE_DIRS}
	${VTE_INCLUDE_DIRS}
   )

# 添加可执行文件
add_executable(danish ${SOURCES})  # 创建可执行文件

# 设置目标的头文件目录
target_include_directories(danish PRIVATE include)

# 查找并链接外部库
# 假设 SomeLibrary 是一个外部库
# add_subdirectory(lib/SomeLibrary)
set(MY_LIBRARIES
	${GTK3_LIBRARIES}
	${VTE_LIBRARIES}
	${TINYXML_LIBRARIES}
   )
target_link_libraries(danish ${MY_LIBRARIES})

# 安装目标
install(TARGETS danish DESTINATION bin)

# 设置安装时的配置
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install)

# 可选：添加测试
enable_testing()
add_test(NAME MyTest COMMAND MyExecutable)
