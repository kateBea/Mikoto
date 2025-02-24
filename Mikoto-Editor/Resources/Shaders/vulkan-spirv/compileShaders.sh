#!/bin/bash

# Need to specify -fshader-stage=<stage>. See glslc --help for more info
# See issue: https://github.com/WebGLTools/GL-Shader-Validator/issues/9
glslc -O -fshader-stage='vertex' StandardVertexShader.glsl -o StandardVertexShader.sprv
glslc -O -fshader-stage='fragment' StandardFragmentShader.glsl -o StandardFragmentShader.sprv
glslc -O -fshader-stage='fragment' ColoredFragmentShader.glsl -o ColoredFragmentShader.sprv

# Deferred
glslc -O -fshader-stage='vertex' DeferredVertexShader.glsl -o DeferredVertexShader.sprv
glslc -O -fshader-stage='fragment' DeferredFragmentShader.glsl -o DeferredFragmentShader.sprv
glslc -O -fshader-stage='vertex' MultipleRenderTargetsVert.glsl -o MultipleRenderTargetsVert.sprv
glslc -O -fshader-stage='fragment' MultipleRenderTargetsFrag.glsl -o MultipleRenderTargetsFrag.sprv

# PBR
glslc -O -fshader-stage='vertex' PhysicallyBasedMatVertex.glsl -o PhysicallyBasedMatVertex.sprv
glslc -O -fshader-stage='fragment' PhysicallyBasedMatFragment.glsl -o PhysicallyBasedMatFragment.sprv
