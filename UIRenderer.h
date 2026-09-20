#ifndef UIRENDERER_H
#define UIRENDERER_H

#include "Shader.h"
#include "Texture.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>

//Desenha em coordenada de tela, em pixels, com a origem no canto superior
//esquerdo. Acumula tudo em tres lotes (solido, tiles do atlas, texto) e
//manda cada um num draw call so no fim do frame.
class UIRenderer
{
public:
    UIRenderer(const char* vertPath, const char* fragPath, const char* fontPath);

    bool ready() const { return shader.ID != 0 && font.ID != 0; }

    //Zera os lotes e fixa o tamanho da tela deste frame.
    void begin(int screenWidth, int screenHeight);

    //Retangulo de cor solida.
    void rect(float x, float y, float w, float h, const glm::vec4& color);

    //Um tile do atlas de blocos.
    void tile(float x, float y, float size, int atlasTile, const glm::vec4& color);

    //Texto. Fonte monoespacada: cada caractere avanca metade do tamanho.
    void text(const std::string& s, float x, float y, float charSize, const glm::vec4& color);
    float textWidth(const std::string& s, float charSize) const;

    //Manda os tres lotes pra GPU, na ordem: solido, tiles, texto.
    void end(const Texture& blockAtlas);

private:
    void pushQuad(std::vector<float>& into, float x, float y, float w, float h,
        const glm::vec4& uv, const glm::vec4& color);
    void flush(std::vector<float>& batch, const Texture* texture);

    Shader shader;
    Texture font;

    unsigned int VAO;
    unsigned int VBO;

    glm::mat4 projection;

    //8 floats por vertice: 2 de posicao, 2 de uv, 4 de cor.
    std::vector<float> solidBatch;
    std::vector<float> tileBatch;
    std::vector<float> textBatch;
};

#endif
