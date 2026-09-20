#ifndef SHADOWMAP_H
#define SHADOWMAP_H

#include <glm/glm.hpp>

//Quantas cascatas. Uma so nao daria conta: pra cobrir 160 blocos com um mapa
//unico, cada texel valeria uns 20 centimetros de bloco perto da camera, que e
//justamente onde o olho repara. Tres faixas resolvem isso gastando resolucao
//onde adianta.
constexpr int SHADOW_CASCADES = 3;

//Lado do mapa de cada cascata, em texels. E o botao principal de qualidade:
//dobrar aqui quadruplica memoria e preenchimento. 1024 da ~39 texels por
//bloco na cascata perto e ~7 na longe, que ja e mais do que o atlas 16x16
//consegue mostrar.
constexpr int SHADOW_RES = 1024;

//Ate onde a sombra alcanca, em blocos. Nao precisa acompanhar o render
//distance: a partir daqui o fog ja comeu o terreno e ninguem repara.
constexpr float SHADOW_DISTANCE = 160.0f;

//Quanto a sombra chega a escurecer no maximo. 1.0 daria preto absoluto, que
//nao existe na vida real: o ceu ilumina o que o sol nao alcanca.
constexpr float SHADOW_STRENGTH = 0.45f;

struct Cascade
{
    //projection * view da luz. Leva do mundo pro clip space do sol.
    glm::mat4 lightSpace;

    //Distancia da camera ate onde esta cascata manda. O fragment escolhe a
    //cascata comparando a propria profundidade com isso.
    float splitDepth;

    //Quanto vale um texel deste mapa, em blocos. O bias usa isso: o erro de
    //amostragem e proporcional ao tamanho do texel, entao um bias fixo ou
    //sobra na cascata perto ou falta na longe.
    float texelWorld;
};

class ShadowMap
{
public:
    ShadowMap();

    //Cria o FBO e o array de profundidade. false se o driver recusar.
    bool create();
    void destroy();

    //Recalcula as matrizes. Chame uma vez por frame, antes de desenhar.
    void update(const glm::mat4& view, float fovDeg, float aspect,
        const glm::vec3& sunDir);

    //Se esta cascata vai ser redesenhada neste frame. As distantes mudam
    //devagar, entao so sao refeitas de vez em quando.
    bool refreshing(int index) const { return refresh[index]; }

    //Liga o FBO na camada da cascata, ajusta o viewport e limpa.
    void beginCascade(int index) const;

    //Volta pro framebuffer da tela.
    void end(int screenW, int screenH) const;

    //Liga o array numa unidade de textura, pro passe de cor amostrar.
    void bindTexture(int unit) const;

    const Cascade& cascade(int i) const { return cascades[i]; }

private:
    unsigned int fbo;
    unsigned int depthArray;

    Cascade cascades[SHADOW_CASCADES];
    bool refresh[SHADOW_CASCADES];

    //Contador de frames, so pra escalonar quais cascatas refazer.
    unsigned int frame;
    bool firstUpdate;
};

#endif
