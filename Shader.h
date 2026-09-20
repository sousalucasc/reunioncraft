#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>

class Shader
{
public:
    //ID do shader program linkado. Fica 0 se algo falhou.
    unsigned int ID;

    //Le, compila e linka os dois shaders a partir dos arquivos.
    Shader(const char* vertexPath, const char* fragmentPath);

    //Ativa o programa.
    void use() const;

    //Local de um uniform. Guarde o resultado se for usar num laco: procurar
    //por nome custa uma busca por string DENTRO do driver, e com um chunk
    //por draw call isso vira centenas de buscas por frame.
    int uniformLocation(const std::string& name) const;

    //Versoes por local, pro caminho quente do desenho.
    void setVec3(int location, const glm::vec3& value) const;
    void setMat4(int location, const glm::mat4& value) const;

    //Helpers de uniform, por nome. O local e memorizado na primeira chamada.
    void setBool(const std::string& name, bool value) const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;
    void setMat4(const std::string& name, const glm::mat4& value) const;

private:
    //Memoria dos locais ja consultados. mutable porque os setters sao const.
    mutable std::unordered_map<std::string, int> locationCache;

    static unsigned int compile(GLenum type, const char* source, const char* label);
    static bool checkErrors(unsigned int object, const char* label, bool isProgram);
};

#endif
