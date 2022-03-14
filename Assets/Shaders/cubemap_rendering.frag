layout(location=0) out vec4 out_color;

in vec3 textureDir; // direction vector representing a 3D texture coordinate

uniform samplerCube cubemap; // cubemap texture sampler

void main()
{
    out_color = texture(cubemap, textureDir);
}
