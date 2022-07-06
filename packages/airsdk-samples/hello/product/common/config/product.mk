COMMON_CONFIG_DIR := $(call my-dir)

# Add Singulair SDK
TARGET_SDK_DIRS += $(ALCHEMY_WORKSPACE_DIR)/sdk/custom

# Add Common skeleton
TARGET_SKEL_DIRS += $(COMMON_CONFIG_DIR)/../skel

# Include buildext mission config modules
include $(ALCHEMY_WORKSPACE_DIR)/build/dragon_buildext_mission/product.mk

$(info TARGET_CONFIG_DIR $(TARGET_CONFIG_DIR))