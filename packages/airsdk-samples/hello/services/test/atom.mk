LOCAL_PATH := $(call my-dir)

airsdk-hello.test-dir := $(airsdk-hello.payload-dir)/test

include $(CLEAR_VARS)

LOCAL_MODULE := testing
LOCAL_CATEGORY_PATH := airsdk/missions/samples/test
LOCAL_DESTDIR := $(airsdk-hello.payload-dir)/test

LOCAL_CXXFLAGS  := -std=c++14 
LOCAL_SRC_FILES := test.cpp
LOCAL_LIBRARIES:= \
	singulair-torch 

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_MODULE := nn-benchmark
LOCAL_CATEGORY_PATH := airsdk/missions/samples/test
LOCAL_DESTDIR := $(airsdk-hello.payload-dir)/test

LOCAL_CXXFLAGS  := -std=c++14  
LOCAL_SRC_FILES := model_benchmark.cpp
LOCAL_LIBRARIES:= \
	singulair-torch 

$(foreach _f, $(call all-files-under,torch/model/,.pt), \
			$(eval LOCAL_COPY_FILES += $(_f):$(airsdk-hello.test-dir)/$(_f)))
			
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_MODULE := function-benchmark
LOCAL_CATEGORY_PATH := airsdk/missions/samples/test
LOCAL_DESTDIR := $(airsdk-hello.payload-dir)/test


LOCAL_SRC_FILES := model_benchmark.cpp
LOCAL_LIBRARIES:= \
	opencv \
	singulair-torch \
	singulair-ia \
	singulair-utils-convertion

$(foreach _f, $(call all-files-under,torch/model/,.pt), \
			$(eval LOCAL_COPY_FILES += $(_f):$(airsdk-hello.test-dir)/$(_f)))
			
include $(BUILD_EXECUTABLE)