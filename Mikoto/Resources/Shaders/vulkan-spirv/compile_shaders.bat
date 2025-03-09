glslc -O -fshader-stage="vertex" StandardVertexShader.glsl -o StandardVertexShader.sprv
glslc -O -fshader-stage="fragment" StandardFragmentShader.glsl -o StandardFragmentShader.sprv
glslc -O -fshader-stage="fragment" WireframeFragmentShader.glsl -o WireframeFragmentShader.sprv

glslc -O -fshader-stage="fragment" PBRFragmentShader.glsl -o PBRFragmentShader.sprv
glslc -O -fshader-stage="vertex" PBRVertexShader.glsl -o PBRVertexShader.sprv

glslc -O -fshader-stage="fragment" Outline_Frag.glsl -o Outline_Frag.sprv
glslc -O -fshader-stage="vertex" Outline_Vert.glsl -o Outline_Vert.sprv

glslc -O -fshader-stage="compute" Compute_Shader_Test.glsl -o Compute_Shader_Test.sprv
glslc -O -fshader-stage="compute" Compute_Shader.glsl -o Compute_Shader.sprv