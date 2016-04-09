#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "scene_materials.h"
#include "main_menu.h"
#include "../../../scene/scene.h"
#include "../../../scene/material.h"
#include <glm/gtc/type_ptr.hpp>

using namespace ImGui;

void UISceneMaterials::Draw()
{
    if (!UIMainMenu::drawSceneMaterials) { return; }

    static auto scene = static_cast<Scene *>(nullptr);
    static auto material = static_cast<Material *>(nullptr);
    // control variables
    static auto selected = -1;
    static glm::vec3 ambient;
    static glm::vec3 specular;
    static glm::vec3 diffuse;
    static float shininess;
    static std::vector<char> name;

    // active scene changed
    if (scene != Scene::Active().get())
    {
        scene = Scene::Active().get();
        selected = -1;
        material = nullptr;
    }

    // no active scene
    if (!scene) { return; }

    // begin editor
    Begin("Materials", &UIMainMenu::drawSceneMaterials);
    PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
    Columns(2);

    for (int i = 0; i < scene->materials.size(); i++)
    {
        auto &current = scene->materials[i];
        PushID(i);
        BeginGroup();

        // selected becomes the clicked selectable
        if (Selectable(current->name.c_str(), i == selected))
        {
            selected = i;
            material = current.get();
            ambient = material->Ambient();
            specular = material->Specular();
            diffuse = material->Diffuse();
            shininess = material->Shininess();
            // copy name to a standard vector
            name.clear();
            copy(material->name.begin(), material->name.end(), back_inserter(name));
            name.push_back('\0');
        }

        EndGroup();
        PopID();
    }

    NextColumn();

    if (selected >= 0 && material != nullptr)
    {
        if(InputText("Name", name.data(), name.size()))
        {
            material->name = std::string(name.data());
        }

        if(ColorEdit3("Ambient", value_ptr(ambient)))
        {
            material->Ambient(ambient);
        }

        if (ColorEdit3("Diffuse", value_ptr(diffuse)))
        {
            material->Diffuse(diffuse);
        }

        if (ColorEdit3("Specular", value_ptr(specular)))
        {
            material->Specular(specular);
        }

        if (DragFloat("Shininess", &shininess, 0.001f, 0.0f, 1.0f))
        {
            material->Shininess(shininess);
        }
    }
    else
    {
        Text("No Material Selected");
    }

    PopStyleVar();
    End();
}

UISceneMaterials::UISceneMaterials()
{
}
UISceneMaterials::~UISceneMaterials()
{
}