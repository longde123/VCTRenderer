#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "voxelizer_renderer.h"
#include "shadow_map_renderer.h"
#include "../scene/scene.h"
#include "../core/assets_manager.h"
#include "../scene/camera.h"
#include "../scene/texture.h"
#include "../scene/material.h"
#include "../../../rendering/render_window.h"

#include "../programs/voxelization_program.h"
#include "../programs/voxel_drawer_program.h"

#include <oglplus/context.hpp>
#include <oglplus/framebuffer.hpp>
#include <glm/gtx/transform.hpp>
#include "../../../scene/light.h"

bool VoxelizerRenderer::ShowVoxels = false;

void VoxelizerRenderer::Render()
{
    // negative framestep means this is disabled
    if (framestep < 0) { return; }

    static Scene * previous = nullptr;
    static auto frameCount = 1;
    static auto &scene = Scene::Active();

    if (!scene || !scene->IsLoaded()) { return; }

    // scene changed or loaded
    if (previous != scene.get())
    {
        UpdateProjectionMatrices(scene->rootNode->boundaries);
        // update voxelization
        VoxelizeScene();
    }

    // store current for next call
    previous = scene.get();

    // another frame called on render, if framestep is > 0
    // Voxelization will happen every framestep frame
    if (framestep != 0 && frameCount++ % framestep == 0)
    {
        frameCount = 1;
        // update voxelization
        VoxelizeScene();
    }

    if (ShowVoxels)
    {
        DrawVoxels();
    }
}

void VoxelizerRenderer::SetMatricesUniforms(const Node &node) const
{
    // no space matrices for voxelization pass during node rendering
    auto &prog = CurrentProgram<VoxelizationProgram>();
    prog.matrices.model.Set(node.ModelMatrix());
    prog.matrices.normal.Set(inverse(transpose(node.ModelMatrix())));
}

void VoxelizerRenderer::SetMaterialUniforms(const Material &material) const
{
    using namespace oglplus;
    auto &prog = CurrentProgram<VoxelizationProgram>();
    prog.material.diffuse.Set(material.Diffuse());
    // set textures
    Texture::Active(RawTexture::Diffuse);
    material.BindTexture(RawTexture::Diffuse);
    prog.diffuseMap.Set(RawTexture::Diffuse);
    Texture::Active(RawTexture::Specular);
    material.BindTexture(RawTexture::Specular);
}

void VoxelizerRenderer::SetUpdateFrequency(const unsigned int framestep)
{
    this->framestep = framestep;
}

void VoxelizerRenderer::VoxelizeScene()
{
    static oglplus::Context gl;
    static auto &scene = Scene::Active();
    static auto zero = 0;

    if (!scene || !scene->IsLoaded()) { return; }

    SetAsActive();
    oglplus::DefaultFramebuffer().Bind(oglplus::FramebufferTarget::Draw);
    gl.ColorMask(false, false, false, false);
    gl.Viewport(volumeDimension, volumeDimension);
    gl.Clear().ColorBuffer().DepthBuffer();
    // active voxelization pass program
    CurrentProgram<VoxelizationProgram>(VoxelizationPass());
    // rendering flags
    gl.Disable(oglplus::Capability::CullFace);
    gl.Disable(oglplus::Capability::DepthTest);
    UseFrustumCulling = false;
    // voxelization pass uniforms
    SetVoxelizationPassUniforms();
    // bind the volume texture to be writen in shaders
    voxelAlbedo.ClearImage(0, oglplus::PixelDataFormat::RedInteger, &zero);
    voxelAlbedo.BindImage(0, 0, true, 0, oglplus::AccessSpecifier::WriteOnly,
                          oglplus::ImageUnitFormat::R32UI);
    // draw scene triangles
    scene->rootNode->DrawList();
}

void VoxelizerRenderer::DrawVoxels()
{
    static auto &camera = Camera::Active();
    static auto &scene = Scene::Active();
    static auto &info = Window().Info();

    if (!camera || !scene || !scene->IsLoaded()) { return; }

    static oglplus::Context gl;
    oglplus::DefaultFramebuffer().Bind(oglplus::FramebufferTarget::Draw);
    gl.ColorMask(true, true, true, true);
    gl.ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    gl.Viewport(info.width, info.height);
    gl.Clear().ColorBuffer().DepthBuffer();
    // Open GL flags
    gl.ClearDepth(1.0f);
    gl.Enable(oglplus::Capability::DepthTest);
    gl.Enable(oglplus::Capability::CullFace);
    gl.FrontFace(oglplus::FaceOrientation::CCW);
    gl.CullFace(oglplus::Face::Back);
    auto &prog = VoxelDrawerShader();
    CurrentProgram<VoxelDrawerProgram>(prog);
    // control vars
    auto sceneBox = scene->rootNode->boundaries;
    auto voxelSize = volumeGridSize / volumeDimension;
    // voxel grid projection matrices
    auto &viewMatrix = camera->ViewMatrix();
    auto &projectionMatrix = camera->ProjectionMatrix();
    // pass voxel drawer uniforms
    voxelAlbedo.BindImage(0, 0, true, 0, oglplus::AccessSpecifier::ReadOnly,
                          oglplus::ImageUnitFormat::R32UI);
    prog.volumeDimension.Set(volumeDimension);
    prog.voxelSize.Set(voxelSize);
    prog.matrices.modelViewProjection.Set(projectionMatrix * viewMatrix);
    // bind vertex buffer array to draw, needed but all geometry is generated
    // in the geometry shader
    voxelDrawerArray.Bind();
    gl.DrawArrays(oglplus::PrimitiveType::Points, 0, voxelCount);
}

void VoxelizerRenderer::UpdateProjectionMatrices(const BoundingBox &sceneBox)
{
    auto axisSize = sceneBox.Extent() * 2.0f;
    auto &center = sceneBox.Center();
    volumeGridSize = glm::max(axisSize.x, glm::max(axisSize.y, axisSize.z));
    auto halfSize = volumeGridSize / 2.0f;
    auto projection = glm::ortho(-halfSize, halfSize, -halfSize, halfSize, 0.0f,
                                 volumeGridSize);
    viewProjectionMatrix[0] = lookAt(center + glm::vec3(halfSize, 0.0f, 0.0f),
                                     center, glm::vec3(0.0f, 1.0f, 0.0f));
    viewProjectionMatrix[1] = lookAt(center + glm::vec3(0.0f, halfSize, 0.0f),
                                     center, glm::vec3(0.0f, 0.0f, -1.0f));
    viewProjectionMatrix[2] = lookAt(center + glm::vec3(0.0f, 0.0f, halfSize),
                                     center, glm::vec3(0.0f, 1.0f, 0.0f));

    for (auto &matrix : viewProjectionMatrix)
    {
        matrix = projection * matrix;
    }
}

void VoxelizerRenderer::CreateVolume(oglplus::Texture &texture) const
{
    using namespace oglplus;
    static auto zero = 0;
    texture.Bind(TextureTarget::_3D);
    texture.Image3D(TextureTarget::_3D, 0,
                    PixelDataInternalFormat::R32UI,
                    volumeDimension, volumeDimension, volumeDimension, 0,
                    PixelDataFormat::RedInteger, PixelDataType::UnsignedInt,
                    nullptr);
    texture.ClearImage(0, PixelDataFormat::RedInteger, &zero);
    texture.BaseLevel(TextureTarget::_3D, 0);
    texture.MaxLevel(TextureTarget::_3D, 0);
    texture.SwizzleRGBA(TextureTarget::_3D,
                        TextureSwizzle::Red,
                        TextureSwizzle::Green,
                        TextureSwizzle::Blue,
                        TextureSwizzle::Alpha);
    texture.MinFilter(TextureTarget::_3D, TextureMinFilter::Linear);
    texture.MagFilter(TextureTarget::_3D, TextureMagFilter::Linear);
    texture.GenerateMipmap(TextureTarget::_3D);
}

VoxelizerRenderer::VoxelizerRenderer(RenderWindow &window) : Renderer(window)
{
    framestep = 5; // only on scene change
    volumeDimension = 128;
    voxelCount = volumeDimension * volumeDimension * volumeDimension;
    CreateVolume(voxelAlbedo);
}

VoxelizerRenderer::~VoxelizerRenderer()
{
}

VoxelizationProgram &VoxelizerRenderer::VoxelizationPass()
{
    static auto &assets = AssetsManager::Instance();
    static auto &prog = *static_cast<VoxelizationProgram *>
                        (assets->programs["Voxelization"].get());
    return prog;
}

VoxelDrawerProgram &VoxelizerRenderer::VoxelDrawerShader()
{
    static auto &assets = AssetsManager::Instance();
    static auto &prog = *static_cast<VoxelDrawerProgram *>
                        (assets->programs["VoxelDrawer"].get());
    return prog;
}

void VoxelizerRenderer::SetVoxelizationPassUniforms() const
{
    auto &shadowing = *static_cast<ShadowMapRenderer *>(AssetsManager::Instance()
                      ->renderers["Shadowmapping"].get());
    auto &prog = CurrentProgram<VoxelizationProgram>();
    prog.viewProjections[0].Set(viewProjectionMatrix[0]);
    prog.viewProjections[1].Set(viewProjectionMatrix[1]);
    prog.viewProjections[2].Set(viewProjectionMatrix[2]);
    prog.volumeDimension.Set(volumeDimension);
    // set directional lights uniforms
    auto &directionals = Light::Directionals();
    auto &points = Light::Points();
    auto &spots = Light::Spots();
    // uniform arrays of lights
    auto &uDirectionals = prog.directionalLight;
    auto &uPoints = prog.pointLight;
    auto &uSpots = prog.spotLight;

    for (int i = 0; i < directionals.size(); i++)
    {
        auto &light = directionals[i];
        auto &uLight = uDirectionals[i];
        auto &intensity = light->Intensity();
        // update view space direction-position
        uLight.direction.Set(light->Direction());
        uLight.diffuse.Set(light->Diffuse() * intensity.y);
    }

    for (int i = 0; i < points.size(); i++)
    {
        auto &light = points[i];
        auto &uLight = uPoints[i];
        auto &intensity = light->Intensity();
        // update view space direction-position
        uLight.position.Set(light->Position());
        uLight.diffuse.Set(light->Diffuse() * intensity.y);
        uLight.attenuation.constant.Set(light->attenuation.Constant());
        uLight.attenuation.linear.Set(light->attenuation.Linear());
        uLight.attenuation.quadratic.Set(light->attenuation.Quadratic());
    }

    for (int i = 0; i < spots.size(); i++)
    {
        auto &light = spots[i];
        auto &uLight = uSpots[i];
        auto &intensity = light->Intensity();
        // update view space direction-position
        uLight.position.Set(light->Position());
        uLight.direction.Set(light->Direction());
        uLight.diffuse.Set(light->Diffuse() * intensity.y);
        uLight.attenuation.constant.Set(light->attenuation.Constant());
        uLight.attenuation.linear.Set(light->attenuation.Linear());
        uLight.attenuation.quadratic.Set(light->attenuation.Quadratic());
        uLight.angleInnerCone.Set(cos(light->AngleInnerCone()));
        uLight.angleOuterCone.Set(cos(light->AngleOuterCone()));
    }

    // pass number of lights per type
    prog.lightTypeCount[0].Set(directionals.size());
    prog.lightTypeCount[1].Set(points.size());
    prog.lightTypeCount[2].Set(spots.size());
    // pass shadowing setup
    prog.lightViewProjection.Set(shadowing.LightSpaceMatrix());
    shadowing.BindReading(6);
    prog.shadowMap.Set(6);
}
