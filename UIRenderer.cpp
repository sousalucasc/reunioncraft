#include "UIRenderer.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

//Largura de avanco de um caractere, em fracao do tamanho pedido.
//Consolas ocupa pouco menos da metade da celula, entao meio tamanho
//encosta os glifos sem sobrepor a tinta.
static const float CHAR_ADVANCE = 0.5f;

UIRenderer::UIRenderer(const char* vertPath, const char* fragPath, const char* fontPath)
    : shader(vertPath, fragPath), font(fontPath), VAO(0), VBO(0), projection(1.0f)
{
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    //8 floats por vertice: posicao em pixels, uv, cor RGBA.
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

void UIRenderer::begin(int screenWidth, int screenHeight)
{
    //Origem no canto superior esquerdo, que e como se pensa layout de tela.
    projection = glm::ortho(0.0f, (float)screenWidth, (float)screenHeight, 0.0f);

    solidBatch.clear();
    tileBatch.clear();
    textBatch.clear();
}

void UIRenderer::pushQuad(std::vector<float>& into, float x, float y, float w, float h,
    const glm::vec4& uv, const glm::vec4& color)
{
    //uv vem como (u0, vBaixo, u1, vCima), igual ao atlasUV. Como o Y da tela
    //cresce pra baixo e o da textura pra cima, o topo do quad usa o V de cima.
    float u0 = uv.x, u1 = uv.z;
    float vBot = uv.y, vTop = uv.w;

    const float corners[6][4] = {
        { x,     y,     u0, vTop },
        { x + w, y + h, u1, vBot },
        { x + w, y,     u1, vTop },

        { x,     y,     u0, vTop },
        { x,     y + h, u0, vBot },
        { x + w, y + h, u1, vBot }
    };

    for (int i = 0; i < 6; i++)
    {
        into.push_back(corners[i][0]);
        into.push_back(corners[i][1]);
        into.push_back(corners[i][2]);
        into.push_back(corners[i][3]);
        into.push_back(color.r);
        into.push_back(color.g);
        into.push_back(color.b);
        into.push_back(color.a);
    }
}

void UIRenderer::rect(float x, float y, float w, float h, const glm::vec4& color)
{
    pushQuad(solidBatch, x, y, w, h, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f), color);
}

void UIRenderer::tile(float x, float y, float size, int atlasTile, const glm::vec4& color)
{
    pushQuad(tileBatch, x, y, size, size, atlasUV(atlasTile), color);
}

float UIRenderer::textWidth(const std::string& s, float charSize) const
{
    return (float)s.size() * charSize * CHAR_ADVANCE;
}

void UIRenderer::text(const std::string& s, float x, float y, float charSize, const glm::vec4& color)
{
    float cursor = x;

    for (size_t i = 0; i < s.size(); i++)
    {
        unsigned char c = (unsigned char)s[i];

        //Fora da faixa que o atlas cobre: so avanca, sem desenhar.
        if (c >= 32 && c <= 126)
        {
            //A grade da fonte tambem e 16x16, entao o calculo de tile serve.
            pushQuad(textBatch, cursor, y, charSize, charSize, atlasUV(c - 32), color);
        }

        cursor += charSize * CHAR_ADVANCE;
    }
}

void UIRenderer::flush(std::vector<float>& batch, const Texture* texture)
{
    if (batch.empty())
        return;

    shader.setInt("useTexture", texture != NULL ? 1 : 0);

    if (texture != NULL)
        texture->bind(0);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, batch.size() * sizeof(float), batch.data(), GL_DYNAMIC_DRAW);

    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(batch.size() / 8));

    glBindVertexArray(0);
}

void UIRenderer::end(const Texture& blockAtlas)
{
    //Interface fica por cima de tudo e nao participa do mundo 3D.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader.use();
    shader.setMat4("projection", projection);
    shader.setInt("uiTexture", 0);

    flush(solidBatch, NULL);
    flush(tileBatch, &blockAtlas);
    flush(textBatch, &font);

    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}
