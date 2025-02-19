#!/bin/bash

# Need to specify -fshader-stage=<stage>. See glslc --help for more info
# See issue: https://github.com/WebGLTools/GL-Shader-Validator/issues/9
glslc -O -fshader-stage='vertex' StandardVertexShader.glsl -o StandardVertexShader.sprv
glslc -O -fshader-stage='fragment' StandardFragmentShader.glsl -o StandardFragmentShader.sprv
glslc -O -fshader-stage='fragment' WireframeFragmentShader.glsl -o WireframeFragmentShader.sprv
