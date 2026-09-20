#ifndef CHUNKMESHER_H
#define CHUNKMESHER_H

#include "World.h"

#include <vector>
#include <cstdint>

//Dados crus da mesh, sem nada de OpenGL.
//Separado de proposito: na fase 6 isso vai ser gerado fora da thread principal,
//e contexto OpenGL nao atravessa thread.
struct MeshData
{
    //8 floats por vertice: 3 de posicao, 3 de cor/tint, 2 de uv.
    std::vector<float> vertices;
    std::vector<uint32_t> indices;

    int faceCount() const { return (int)indices.size() / 6; }
};

//Emite apenas as faces que dao pro ar ou pra um bloco transparente.
//Face entre dois blocos solidos nao chega a existir.
//Consulta o World, nao o Chunk: e isso que evita parede na costura entre chunks.
//As posicoes ja saem em coordenada de mundo, entao o model fica identidade.
void buildChunkMesh(const World& world, ChunkPos pos, MeshData& out);

//Os buffers na GPU. Recebe um MeshData pronto e desenha.
class ChunkMesh
{
public:
    ChunkMesh();

    //Substitui o conteudo dos buffers. Cria na primeira chamada.
    void upload(const MeshData& data);
    void draw() const;

    //Libera os buffers na GPU. Chamado quando o chunk e descarregado;
    //sem isso o streaming vaza um VAO e dois VBOs por chunk.
    void destroy();

private:
    unsigned int VAO;
    unsigned int VBO;
    unsigned int EBO;
    int indexCount;
    bool created;
};

#endif
