#!/bin/bash

# Need to specify -fshader-stage=<stage>. See glslc --help for more info
# See issue: https://github.com/WebGLTools/GL-Shader-Validator/issues/9
glslc -fshader-stage='vertex' StandardVertexShader.glsl -o StandardVertexShader.sprv
glslc -fshader-stage='fragment' StandardFragmentShader.glsl -o StandardFragmentShader.sprv
glslc -fshader-stage='fragment' ColoredFragmentShader.glsl -o ColoredFragmentShader.sprv

# Deferred
glslc -fshader-stage='vertex' DeferredVertexShader.glsl -o DeferredVertexShader.sprv
glslc -fshader-stage='fragment' DeferredFragmentShader.glsl -o DeferredFragmentShader.sprv
glslc -fshader-stage='vertex' MultipleRenderTargetsVert.glsl -o MultipleRenderTargetsVert.sprv
glslc -fshader-stage='fragment' MultipleRenderTargetsFrag.glsl -o MultipleRenderTargetsFrag.sprv