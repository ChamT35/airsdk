LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := testing
LOCAL_CATEGORY_PATH := airsdk/missions/samples/testing
LOCAL_DESTDIR := $(airsdk-hello.payload-dir)/services

LOCAL_CXXFLAGS  := -std=c++14 
LOCAL_SRC_FILES := test.cpp
LOCAL_LIBRARIES:= \
	singulair-torch 

include $(BUILD_EXECUTABLE)