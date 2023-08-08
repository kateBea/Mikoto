#!/bin/bash

# Need to specify -fshader-stage=<stage>. See glslc --help for more info
# See issue: https://github.com/WebGLTools/GL-Shader-Validator/issues/9
glslc -fshader-stage='vertex' basicVert.glsl -o basicVert.sprv
glslc -fshader-stage='fragment' basicFrag.glsl -o basicFrag.sprv