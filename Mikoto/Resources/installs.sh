#!/usr/bin/env bash
# install.sh — Install Mikoto Linux dependencies

# Vulkan
sudo apt install vulkan-tools
sudo apt install libvulkan-dev
sudo apt install vulkan-utility-libraries-dev spirv-tools

# Native file dialog
sudo apt-get install libgtk-3-dev

# GLFW
sudo apt install libwayland-dev libxkbcommon-dev xorg-dev

# Optional: GLSL Compiler to SPIR-V
sudo apt install glslc