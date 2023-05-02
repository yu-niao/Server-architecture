

export BUILD_ROOT = $(shell pwd)

#定义头文件的路径变量
export INCLUDE_PATH = $(BUILD_ROOT)/_include

#定义我们要编译的目录
BUILD_DIR = $(BUILD_ROOT)/signal/ \
			$(BUILD_ROOT)/proc/   \
			$(BUILD_ROOT)/net/    \
			$(BUILD_ROOT)/misc/   \
			$(BUILD_ROOT)/logic/   \
			$(BUILD_ROOT)/app/ 


export DEBUG = true

