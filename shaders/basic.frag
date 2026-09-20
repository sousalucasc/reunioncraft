#version 330 core

in vec3 ourColor;
in vec2 TexCoord;

out vec4 FragColor;

uniform sampler2D blockTexture;

void main()
{
    vec4 texel = texture(blockTexture, TexCoord);

    // Folha tem buracos transparentes na textura. Sem isso eles virariam
    // pixels pretos. discard descarta o fragmento sem precisar de blending,
    // que e mais barato e nao depende de ordem de desenho.
    if (texel.a < 0.5)
        discard;

    // ourColor carrega o tint do bloco; vira fator de luz por face na fase 6.
    FragColor = texel * vec4(ourColor, 1.0);
}
