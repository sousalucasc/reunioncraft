#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>

enum class CameraMovement
{
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN
};

//Camera FPS estilo modo criativo: voa livre, sem gravidade nem colisao.
class Camera
{
public:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;

    float yaw;
    float pitch;
    float fov;
    float speed;
    float sensitivity;

    Camera(glm::vec3 startPosition = glm::vec3(0.0f, 0.0f, 3.0f));

    glm::mat4 getView() const;
    glm::mat4 getProjection(float aspect) const;

    //deltaTime deixa a velocidade independente do FPS.
    void processKeyboard(CameraMovement direction, float deltaTime);
    //Offsets ja em pixels; pitch fica travado em +-89 graus.
    void processMouse(float xoffset, float yoffset);

private:
    //Recalcula front/right/up a partir de yaw e pitch.
    void updateVectors();
};

#endif
