LOCAL_PATH := $(call my-dir)

#############################################################
# Build and copy mission services

include $(CLEAR_VARS)

LOCAL_MODULE := airsdk-singulair-cv-service
LOCAL_CATEGORY_PATH := airsdk/missions/samples/singulair
LOCAL_DESTDIR := $(airsdk-hello.payload-dir)/services

LOCAL_SRC_FILES := sample.cpp processing.cpp evaluation.cpp

LOCAL_CXXFLAGS := -std=c++17

LOCAL_LIBRARIES := \
	libairsdk-singulair-cv-service-msghub \
	libmsghub \
	libpomp \
	libtelemetry \
	libulog \
	libvideo-ipc \
	libvideo-ipc-client-config \
	protobuf \
	singulair-torch \
	singulair-ia \
	singulair-utils-convertion \
	singulair-utils-performance \
	opencv4 \
	libjpeg

# singulair-opencv4 

$(foreach _f, $(call all-files-under,torch/model/,.pt), \
			$(eval LOCAL_COPY_FILES += $(_f):$(airsdk-hello.services-dir)/$(_f)))
LOCAL_COPY_FILES += /home/champion/airsdk/packages/airsdk-samples/hello/services/singulair/15.jpg:$(airsdk-hello.services-dir)/
# LOCAL_COPY_FILES := torch/model/*.pt:missions/com.parrot.missions.samples.hello/payload/services/torch/model/model.pt

include $(BUILD_EXECUTABLE)

#############################################################
# Messages exchanged between mission and native cv service

cv_service_proto_path := protobuf
cv_service_proto_files := $(call all-files-under,$(cv_service_proto_path),.proto)

include $(CLEAR_VARS)

LOCAL_MODULE := libairsdk-singulair-cv-service-pbpy
LOCAL_CATEGORY_PATH := airsdk/missions/samples/singulair
LOCAL_LIBRARIES := \
	protobuf-python

$(foreach __f,$(cv_service_proto_files), \
	$(eval LOCAL_CUSTOM_MACROS += $(subst $(space),,protoc-macro:python, \
		$(TARGET_OUT_STAGING)/usr/lib/python/site-packages, \
		$(LOCAL_PATH)/$(__f), \
		$(LOCAL_PATH)/$(cv_service_proto_path))) \
)

include $(BUILD_CUSTOM)

include $(CLEAR_VARS)

LOCAL_MODULE := libairsdk-singulair-cv-service-pb
LOCAL_CATEGORY_PATH := airsdk/missions/samples/singulair
LOCAL_CXXFLAGS := -std=c++11
LOCAL_LIBRARIES := protobuf
LOCAL_EXPORT_C_INCLUDES := $(call local-get-build-dir)/gen

$(foreach __f,$(cv_service_proto_files), \
	$(eval LOCAL_CUSTOM_MACROS += $(subst $(space),,protoc-macro:cpp,gen, \
		$(LOCAL_PATH)/$(__f), \
		$(LOCAL_PATH)/$(cv_service_proto_path))) \
)

include $(BUILD_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := libairsdk-singulair-cv-service-msghub
LOCAL_CATEGORY_PATH := airsdk/missions/samples/singulair
LOCAL_CXXFLAGS := -std=c++11
LOCAL_LIBRARIES := protobuf libairsdk-singulair-cv-service-pb libmsghub
LOCAL_EXPORT_C_INCLUDES := $(call local-get-build-dir)/gen

$(foreach __f,$(cv_service_proto_files), \
	$(eval LOCAL_CUSTOM_MACROS += $(subst $(space),,msghub-macro:cpp,gen, \
		$(LOCAL_PATH)/$(__f), \
		$(LOCAL_PATH)/$(cv_service_proto_path))) \
)

include $(BUILD_LIBRARY)
