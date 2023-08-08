#version 410 core
out vec4 fragmentColor;

struct Material {
    sampler2D diffuse;
    sampler2D specular;

    float shininess;
};

struct Light {
    vec3 position;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

in vec3 fragPosition;
in vec3 normals;
in vec2 textureCoordinates;

uniform vec3 viewPos;

uniform Material material;
uniform Light light;

void main()
{
    // ambient
    vec3 ambient = light.ambient * texture(material.diffuse, textureCoordinates).rgb;

    // diffuse
    vec3 norm = normalize(normals);
    vec3 lightDir = normalize(light.position - fragPosition);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = light.diffuse * diff * texture(material.diffuse, textureCoordinates).rgb;

    // specular
    vec3 viewDir = normalize(viewPos - fragPosition);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * spec * texture(material.specular, textureCoordinates).rgb;

    vec3 result = ambient + diffuse + specular;
    fragmentColor = vec4(result, 1.0);
}