#include "ShadowMap.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <algorithm>
#include <iostream>

//Plano de perto usado no calculo das divisoes. Nao precisa bater com o da
//projecao real; so define onde a primeira cascata comeca.
static const float SHADOW_NEAR = 0.5f;

//Mistura entre divisao logaritmica e linear. A logaritmica e a correta em
//teoria mas deixa a primeira cascata minuscula; a linear desperdica
//resolucao no fundo. 0.6 puxa pro lado bom das duas.
static const float SPLIT_LAMBDA = 0.6f;

//Quanto a luz recua alem da fatia, em blocos. Um bloco que projeta sombra
//pode estar muito acima do que a camera enxerga: a montanha atras do morro
//ainda escurece o vale. O terreno chega a 256, entao 300 cobre qualquer caso.
static const float CASTER_MARGIN = 300.0f;

//De quantos em quantos frames cada cascata e refeita. A de perto todo frame;
//as de tras mudam devagar e podem esperar. As fases sao escolhidas pra que
//nunca caiam duas cascatas caras no mesmo frame.
static const unsigned int REFRESH_EVERY[SHADOW_CASCADES] = { 1, 2, 4 };
static const unsigned int REFRESH_PHASE[SHADOW_CASCADES] = { 0, 0, 1 };

ShadowMap::ShadowMap()
    : fbo(0), depthArray(0), frame(0), firstUpdate(true)
{
    for (int i = 0; i < SHADOW_CASCADES; i++)
    {
        cascades[i].lightSpace = glm::mat4(1.0f);
        cascades[i].splitDepth = 0.0f;
        cascades[i].texelWorld = 0.0f;
        refresh[i] = true;
    }
}

bool ShadowMap::create()
{
    //Um array de texturas em vez de N texturas soltas: o shader amostra as
    //tres com um sampler so, indexando a camada.
    glGenTextures(1, &depthArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, depthArray);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24,
        SHADOW_RES, SHADOW_RES, SHADOW_CASCADES,
        0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

    //Comparacao feita pelo proprio hardware: em vez de devolver a
    //profundidade gravada, a amostra ja devolve 0 ou 1 comparando com a
    //referencia. Com filtro LINEAR isso vira um PCF 2x2 de graca, porque a
    //interpolacao acontece DEPOIS da comparacao.
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    //Fora do mapa a borda vale 1, ou seja, iluminado. Com CLAMP_TO_EDGE a
    //ultima fileira de texels se esticaria pro infinito e pintaria faixas
    //de sombra no terreno todo alem da cascata.
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    float branco[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, branco);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    //So profundidade. Sem anexo de cor o rasterizador pula blending e escrita
    //de cor inteiros, que e metade do motivo do passe ser barato.
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthArray, 0, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cout << "ERRO::SHADOWMAP::FBO incompleto (0x"
            << std::hex << status << std::dec << ")" << std::endl;
        destroy();
        return false;
    }

    return true;
}

void ShadowMap::destroy()
{
    if (fbo != 0)
    {
        glDeleteFramebuffers(1, &fbo);
        fbo = 0;
    }

    if (depthArray != 0)
    {
        glDeleteTextures(1, &depthArray);
        depthArray = 0;
    }
}

void ShadowMap::update(const glm::mat4& view, float fovDeg, float aspect,
    const glm::vec3& sunDir)
{
    frame++;

    glm::mat4 invView = glm::inverse(view);

    float tanV = std::tan(glm::radians(fovDeg) * 0.5f);
    float tanH = tanV * aspect;

    //Onde cada cascata termina.
    float splits[SHADOW_CASCADES + 1];
    splits[0] = SHADOW_NEAR;

    for (int i = 1; i <= SHADOW_CASCADES; i++)
    {
        float p = (float)i / (float)SHADOW_CASCADES;
        float logSplit = SHADOW_NEAR * std::pow(SHADOW_DISTANCE / SHADOW_NEAR, p);
        float linSplit = SHADOW_NEAR + (SHADOW_DISTANCE - SHADOW_NEAR) * p;

        splits[i] = SPLIT_LAMBDA * logSplit + (1.0f - SPLIT_LAMBDA) * linSplit;
    }

    for (int i = 0; i < SHADOW_CASCADES; i++)
    {
        cascades[i].splitDepth = splits[i + 1];

        refresh[i] = firstUpdate || (frame % REFRESH_EVERY[i]) == REFRESH_PHASE[i];

        //Pular uma cascata significa manter o mapa E a matriz do frame
        //anterior. Trocar so a matriz amostraria o mapa velho com a
        //transformacao nova, e a sombra sairia deslocada.
        if (!refresh[i])
            continue;

        float perto = splits[i];
        float longe = splits[i + 1];

        //Os 8 cantos da fatia, em coordenada de mundo.
        glm::vec3 cantos[8];
        int k = 0;

        for (int lado = 0; lado < 2; lado++)
        {
            float d = (lado == 0) ? perto : longe;

            for (int sy = -1; sy <= 1; sy += 2)
            {
                for (int sx = -1; sx <= 1; sx += 2)
                {
                    //Em espaco de camera o olhar e -Z.
                    glm::vec4 p(sx * d * tanH, sy * d * tanV, -d, 1.0f);
                    cantos[k++] = glm::vec3(invView * p);
                }
            }
        }

        //Esfera envolvente em vez de caixa alinhada a luz. Uma caixa muda de
        //tamanho conforme a camera gira, e com ela muda a escala do mapa: a
        //sombra ferve a cada movimento do mouse. O raio de uma esfera so
        //depende do formato da fatia, que e rigido.
        glm::vec3 centro(0.0f);
        for (int c = 0; c < 8; c++)
            centro += cantos[c];
        centro /= 8.0f;

        float raio = 0.0f;
        for (int c = 0; c < 8; c++)
            raio = std::max(raio, glm::length(cantos[c] - centro));

        //Arredonda pra cima numa grade grossa, pra que ruido de ponto
        //flutuante no ultimo digito nao mude a escala de frame pra frame.
        raio = std::ceil(raio * 16.0f) / 16.0f;

        float texel = (2.0f * raio) / (float)SHADOW_RES;
        cascades[i].texelWorld = texel;

        //Ao meio-dia o sol quase encosta no eixo Y e o lookAt degenera.
        glm::vec3 up = (std::fabs(sunDir.y) > 0.99f)
            ? glm::vec3(0.0f, 0.0f, 1.0f)
            : glm::vec3(0.0f, 1.0f, 0.0f);

        //Prende o centro na grade de texels do mapa. Sem isso a sombra
        //formiga enquanto o jogador anda: cada frame amostraria a mesma
        //geometria numa grade deslocada por uma fracao de texel.
        glm::mat4 sonda = glm::lookAt(centro + sunDir, centro, up);
        glm::vec3 centroLuz = glm::vec3(sonda * glm::vec4(centro, 1.0f));

        centroLuz.x = std::floor(centroLuz.x / texel) * texel;
        centroLuz.y = std::floor(centroLuz.y / texel) * texel;

        centro = glm::vec3(glm::inverse(sonda) * glm::vec4(centroLuz, 1.0f));

        glm::mat4 lightView = glm::lookAt(
            centro + sunDir * (raio + CASTER_MARGIN), centro, up);

        //Ortografica porque o sol e direcional: os raios chegam paralelos,
        //sem ponto de fuga.
        glm::mat4 lightProj = glm::ortho(
            -raio, raio, -raio, raio,
            0.0f, CASTER_MARGIN + 2.0f * raio);

        cascades[i].lightSpace = lightProj * lightView;
    }

    firstUpdate = false;
}

void ShadowMap::beginCascade(int index) const
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthArray, 0, index);

    glViewport(0, 0, SHADOW_RES, SHADOW_RES);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void ShadowMap::end(int screenW, int screenH) const
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, screenW, screenH);
}

void ShadowMap::bindTexture(int unit) const
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D_ARRAY, depthArray);
}
