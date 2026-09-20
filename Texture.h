#ifndef TEXTURE_H
#define TEXTURE_H

#include <glad/glad.h>
#include <glm/glm.hpp>

//Carrega um PNG pra GPU com filtro GL_NEAREST (visual pixelado, sem borrar).
class Texture
{
public:
    //ID da textura. Fica 0 se o arquivo nao carregou.
    unsigned int ID;
    int width;
    int height;

    Texture(const char* path);

    //Liga a textura numa unidade (0, 1, ...) pro sampler do shader.
    void bind(unsigned int unit = 0) const;
};

//Grade do atlas: 16x16 tiles de 16 pixels (textures/atlas.png).
constexpr int ATLAS_GRID = 16;

//Indices dos tiles, na ordem em que o atlas foi montado.
enum AtlasTile
{
    TILE_DIRT = 0,
    TILE_GRASS_TOP,
    TILE_GRASS_SIDE,
    TILE_STONE,
    TILE_COBBLESTONE,
    TILE_SAND,
    TILE_GRAVEL,
    TILE_BEDROCK,
    TILE_PLANKS_OAK,
    TILE_LOG_OAK,
    TILE_LOG_OAK_TOP,
    TILE_LEAVES_OAK,
    TILE_COAL_ORE,
    TILE_IRON_ORE,
    TILE_GOLD_ORE,
    TILE_DIAMOND_ORE,
    TILE_GLASS,
    TILE_BRICK,
    TILE_SANDSTONE,
    TILE_SNOW,
    TILE_ICE,
    TILE_CLAY,
    TILE_OBSIDIAN,
    TILE_WATER
};

//Devolve (u0, v0, u1, v1) do tile dentro do atlas.
//O indice conta da esquerda pra direita e de cima pra baixo, como se le a imagem.
glm::vec4 atlasUV(int tileIndex);

#endif
