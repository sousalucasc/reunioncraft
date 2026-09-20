#ifndef FRUSTUM_H
#define FRUSTUM_H

#include <glm/glm.hpp>

//Os 6 planos que delimitam o que a camera enxerga.
//Cada plano e (a, b, c, d), com (a,b,c) normalizado apontando pra DENTRO
//do volume visivel. Um ponto esta dentro do plano quando ax+by+cz+d >= 0.
struct Frustum
{
    glm::vec4 planes[6];
};

//Extrai os planos da matriz projection * view (metodo de Gribb e Hartmann).
//Funciona porque cada plano ja existe como combinacao de duas linhas dessa
//matriz: nao precisa desmontar a projecao nem saber o fov.
Frustum extractFrustum(const glm::mat4& viewProjection);

//Falso apenas quando a caixa esta com certeza fora. Pode devolver verdadeiro
//pra caixa que so encosta num canto, e tudo bem: errar pro lado de desenhar
//custa um draw call, errar pro outro faz geometria sumir da tela.
bool aabbVisible(const Frustum& frustum, const glm::vec3& min, const glm::vec3& max);

#endif
