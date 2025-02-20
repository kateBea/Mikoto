glslc -O -fshader-stage="vertex" StandardVertexShader.glsl -o StandardVertexShader.sprv
glslc -O -fshader-stage="fragment" StandardFragmentShader.glsl -o StandardFragmentShader.sprv
glslc -O -fshader-stage="fragment" WireframeFragmentShader.glsl -o WireframeFragmentShader.sprv

glslc -O -fshader-stage="fragment" PBRFragmentShader.glsl -o PBRFragmentShader.sprv
glslc -O -fshader-stage="vertex" PBRVertexShader.glsl -o PBRVertexShader.sprv