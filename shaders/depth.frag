#version 330 core

in vec2 TexCoord;
in vec2 TileBase;

uniform sampler2D blockTexture;

const float TILE_STEP = 1.0 / 16.0;

// Sem saida de cor: o FBO so tem anexo de profundidade. O unico trabalho
// aqui e decidir se o fragmento chega a existir.
void main()
{
    // Folha tem buraco na textura. Sem o discard cada arvore projetaria a
    // sombra de um cubo macico em vez de uma copa rendilhada. E o unico
    // fetch de textura do passe inteiro, e so existe por causa disso.
    if (texture(blockTexture, TileBase + fract(TexCoord) * TILE_STEP).a < 0.5)
        discard;
}
