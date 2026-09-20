#ifndef CHUNKMESHER_H
#define CHUNKMESHER_H

#include "World.h"

#include <vector>
#include <cstdint>

//Um conjunto de vertices e indices. 8 floats por vertice:
//3 de posicao, 3 de cor ja com luz e AA aplicados, 2 de uv.
struct MeshBuffer
{
    std::vector<float> vertices;
    std::vector<uint32_t> indices;

    int faceCount() const { return (int)indices.size() / 6; }

    void clear()
    {
        vertices.clear();
        indices.clear();
    }
};

//Dados crus da mesh, sem nada de OpenGL.
//Separado de proposito: na fase 6.4 isso vai ser gerado fora da thread
//principal, e contexto OpenGL nao atravessa thread.
struct MeshData
{
    //Opaco e recorte juntos: o discard no shader resolve vidro e folha,
    //e nenhum dos dois depende de ordem de desenho.
    MeshBuffer solid;
    //Agua. Vai num segundo passe, com blending e ordenada.
    MeshBuffer water;

    int faceCount() const { return solid.faceCount() + water.faceCount(); }
};

//Emite apenas as faces que dao pro ar ou pra um bloco transparente.
//Face entre dois blocos solidos nao chega a existir.
//Consulta o World, nao o Chunk: e isso que evita parede na costura entre chunks.
//As posicoes ja saem em coordenada de mundo, entao o model fica identidade.
void buildChunkMesh(const World& world, ChunkPos pos, MeshData& out);

//Os buffers na GPU. Guarda dois conjuntos: um por passe.
class ChunkMesh
{
public:
    ChunkMesh();

    //Substitui o conteudo dos buffers. Cria na primeira chamada.
    void upload(const MeshData& data);

    void drawSolid() const;
    void drawWater() const;

    bool hasWater() const { return water.indexCount > 0; }

    //Libera os buffers na GPU. Chamado quando o chunk e descarregado;
    //sem isso o streaming vaza um VAO e dois VBOs por chunk.
    void destroy();

private:
    struct Buffers
    {
        unsigned int VAO;
        unsigned int VBO;
        unsigned int EBO;
        int indexCount;
        bool created;
    };

    static void uploadBuffer(Buffers& b, const MeshBuffer& data);
    static void drawBuffer(const Buffers& b);
    static void destroyBuffer(Buffers& b);

    Buffers solid;
    Buffers water;
};

#endif
