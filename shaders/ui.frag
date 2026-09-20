#version 330 core

in vec2 UV;
in vec4 Color;

out vec4 FragColor;

uniform sampler2D uiTexture;

// Lote de cor solida nao amostra textura nenhuma: o mesmo shader serve
// pros retangulos, pros tiles do atlas e pro texto.
uniform int useTexture;

void main()
{
    vec4 c = Color;

    if (useTexture == 1)
        c *= texture(uiTexture, UV);

    // Pixel totalmente transparente nem chega ao blending.
    if (c.a < 0.01)
        discard;

    FragColor = c;
}
