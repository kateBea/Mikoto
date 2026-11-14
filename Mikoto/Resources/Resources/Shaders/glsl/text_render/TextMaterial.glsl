struct FontRenderParams {
    mat4 Projection;
    mat4 View;
    mat4 Model;

    vec4 OutlineColor;
    float OutlineWidth;

    vec4 Position;
    vec4 Size;
    vec4 Color;
    vec2 TextureCoords[4];
    uint TextureIndex;
};