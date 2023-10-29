#!/bin/bash

# Need to specify -fshader-stage=<stage>. See glslc --help for more info
# See issue: https://github.com/WebGLTools/GL-Shader-Validator/issues/9
glslc -fshader-stage='vertex' StandardVertexShader.glsl -o StandardVertexShader.sprv
glslc -fshader-stage='fragment' StandardFragmentShader.glsl -o StandardFragmentShader.sprv
glslc -fshader-stage='fragment' ColoredFragmentShader.glsl -o ColoredFragmentShader.sprv