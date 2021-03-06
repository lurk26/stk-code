#include "graphics/irr_driver.hpp"

#include "config/user_config.hpp"
#include "graphics/callbacks.hpp"
#include "graphics/camera.hpp"
#include "graphics/glwrap.hpp"
#include "graphics/lens_flare.hpp"
#include "graphics/light.hpp"
#include "graphics/lod_node.hpp"
#include "graphics/material_manager.hpp"
#include "graphics/particle_kind_manager.hpp"
#include "graphics/per_camera_node.hpp"
#include "graphics/post_processing.hpp"
#include "graphics/referee.hpp"
#include "graphics/rtts.hpp"
#include "graphics/screenquad.hpp"
#include "graphics/shaders.hpp"
#include "graphics/stkmeshscenenode.hpp"
#include "graphics/stkinstancedscenenode.hpp"
#include "graphics/wind.hpp"
#include "io/file_manager.hpp"
#include "items/item.hpp"
#include "items/item_manager.hpp"
#include "modes/world.hpp"
#include "physics/physics.hpp"
#include "tracks/track.hpp"
#include "utils/constants.hpp"
#include "utils/helpers.hpp"
#include "utils/log.hpp"
#include "utils/profiler.hpp"

#include <algorithm>
#include <limits>

#define MAX2(a, b) ((a) > (b) ? (a) : (b))
#define MIN2(a, b) ((a) > (b) ? (b) : (a))

static void getXYZ(GLenum face, float i, float j, float &x, float &y, float &z)
{
    switch (face)
    {
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        x = 1.;
        y = -i;
        z = -j;
        break;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        x = -1.;
        y = -i;
        z = j;
        break;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        x = j;
        y = 1.;
        z = i;
        break;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        x = j;
        y = -1;
        z = -i;
        break;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        x = j;
        y = -i;
        z = 1;
        break;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        x = -j;
        y = -i;
        z = -1;
        break;
    }

    float norm = sqrt(x * x + y * y + z * z);
    x /= norm, y /= norm, z /= norm;
    return;
}

static void getYml(GLenum face, size_t width, size_t height,
    float *Y00,
    float *Y1minus1, float *Y10, float *Y11,
    float *Y2minus2, float *Y2minus1, float *Y20, float *Y21, float *Y22)
{
    for (unsigned i = 0; i < width; i++)
    {
        for (unsigned j = 0; j < height; j++)
        {
            float x, y, z;
            float fi = float(i), fj = float(j);
            fi /= width, fj /= height;
            fi = 2 * fi - 1, fj = 2 * fj - 1;
            getXYZ(face, fi, fj, x, y, z);

            // constant part of Ylm
            float c00 = 0.282095f;
            float c1minus1 = 0.488603f;
            float c10 = 0.488603f;
            float c11 = 0.488603f;
            float c2minus2 = 1.092548f;
            float c2minus1 = 1.092548f;
            float c21 = 1.092548f;
            float c20 = 0.315392f;
            float c22 = 0.546274f;

            size_t idx = i * height + j;

            Y00[idx] = c00;
            Y1minus1[idx] = c1minus1 * y;
            Y10[idx] = c10 * z;
            Y11[idx] = c11 * x;
            Y2minus2[idx] = c2minus2 * x * y;
            Y2minus1[idx] = c2minus1 * y * z;
            Y21[idx] = c21 * x * z;
            Y20[idx] = c20 * (3 * z * z - 1);
            Y22[idx] = c22 * (x * x - y * y);
        }
    }
}

static float getTexelValue(unsigned i, unsigned j, size_t width, size_t height, float *Coeff, float *Y00, float *Y1minus1, float *Y10, float *Y11,
    float *Y2minus2, float * Y2minus1, float * Y20, float *Y21, float *Y22)
{
    float d = sqrt((float)(i * i + j * j + 1));
    float solidangle = 1.;
    size_t idx = i * height + j;
    float reconstructedVal = Y00[idx] * Coeff[0];
    reconstructedVal += Y1minus1[i * height + j] * Coeff[1] + Y10[i * height + j] * Coeff[2] + Y11[i * height + j] * Coeff[3];
    reconstructedVal += Y2minus2[idx] * Coeff[4] + Y2minus1[idx] * Coeff[5] + Y20[idx] * Coeff[6] + Y21[idx] * Coeff[7] + Y22[idx] * Coeff[8];
    reconstructedVal /= solidangle;
    return MAX2(255.0f * reconstructedVal, 0.f);
}

static void unprojectSH(float *output[], size_t width, size_t height,
    float *Y00[],
    float *Y1minus1[], float *Y10[], float *Y11[],
    float *Y2minus2[], float *Y2minus1[], float * Y20[], float *Y21[], float *Y22[],
    float *blueSHCoeff, float *greenSHCoeff, float *redSHCoeff)
{
    for (unsigned face = 0; face < 6; face++)
    {
        for (unsigned i = 0; i < width; i++)
        {
            for (unsigned j = 0; j < height; j++)
            {
                float fi = float(i), fj = float(j);
                fi /= width, fj /= height;
                fi = 2 * fi - 1, fj = 2 * fj - 1;

                output[face][4 * height * i + 4 * j + 2] = getTexelValue(i, j, width, height,
                    redSHCoeff,
                    Y00[face], Y1minus1[face], Y10[face], Y11[face], Y2minus2[face], Y2minus1[face], Y20[face], Y21[face], Y22[face]);
                output[face][4 * height * i + 4 * j + 1] = getTexelValue(i, j, width, height,
                    greenSHCoeff,
                    Y00[face], Y1minus1[face], Y10[face], Y11[face], Y2minus2[face], Y2minus1[face], Y20[face], Y21[face], Y22[face]);
                output[face][4 * height * i + 4 * j] = getTexelValue(i, j, width, height,
                    blueSHCoeff,
                    Y00[face], Y1minus1[face], Y10[face], Y11[face], Y2minus2[face], Y2minus1[face], Y20[face], Y21[face], Y22[face]);
            }
        }
    }
}

static void projectSH(float *color[], size_t width, size_t height,
    float *Y00[],
    float *Y1minus1[], float *Y10[], float *Y11[],
    float *Y2minus2[], float *Y2minus1[], float * Y20[], float *Y21[], float *Y22[],
    float *blueSHCoeff, float *greenSHCoeff, float *redSHCoeff
    )
{
    for (unsigned i = 0; i < 9; i++)
    {
        blueSHCoeff[i] = 0;
        greenSHCoeff[i] = 0;
        redSHCoeff[i] = 0;
    }
    float wh = float(width * height);
    for (unsigned face = 0; face < 6; face++)
    {
        for (unsigned i = 0; i < width; i++)
        {
            for (unsigned j = 0; j < height; j++)
            {
                size_t idx = i * height + j;
                float fi = float(i), fj = float(j);
                fi /= width, fj /= height;
                fi = 2 * fi - 1, fj = 2 * fj - 1;


                float d = sqrt(fi * fi + fj * fj + 1);

                // Constant obtained by projecting unprojected ref values
                float solidangle = 2.75f / (wh * pow(d, 1.5f));
                // pow(., 2.2) to convert from srgb
                float b = pow(color[face][4 * height * i + 4 * j] / 255.f, 2.2f);
                float g = pow(color[face][4 * height * i + 4 * j + 1] / 255.f, 2.2f);
                float r = pow(color[face][4 * height * i + 4 * j + 2] / 255.f, 2.2f);

                assert(b >= 0.);

                blueSHCoeff[0] += b * Y00[face][idx] * solidangle;
                blueSHCoeff[1] += b * Y1minus1[face][idx] * solidangle;
                blueSHCoeff[2] += b * Y10[face][idx] * solidangle;
                blueSHCoeff[3] += b * Y11[face][idx] * solidangle;
                blueSHCoeff[4] += b * Y2minus2[face][idx] * solidangle;
                blueSHCoeff[5] += b * Y2minus1[face][idx] * solidangle;
                blueSHCoeff[6] += b * Y20[face][idx] * solidangle;
                blueSHCoeff[7] += b * Y21[face][idx] * solidangle;
                blueSHCoeff[8] += b * Y22[face][idx] * solidangle;

                greenSHCoeff[0] += g * Y00[face][idx] * solidangle;
                greenSHCoeff[1] += g * Y1minus1[face][idx] * solidangle;
                greenSHCoeff[2] += g * Y10[face][idx] * solidangle;
                greenSHCoeff[3] += g * Y11[face][idx] * solidangle;
                greenSHCoeff[4] += g * Y2minus2[face][idx] * solidangle;
                greenSHCoeff[5] += g * Y2minus1[face][idx] * solidangle;
                greenSHCoeff[6] += g * Y20[face][idx] * solidangle;
                greenSHCoeff[7] += g * Y21[face][idx] * solidangle;
                greenSHCoeff[8] += g * Y22[face][idx] * solidangle;


                redSHCoeff[0] += r * Y00[face][idx] * solidangle;
                redSHCoeff[1] += r * Y1minus1[face][idx] * solidangle;
                redSHCoeff[2] += r * Y10[face][idx] * solidangle;
                redSHCoeff[3] += r * Y11[face][idx] * solidangle;
                redSHCoeff[4] += r * Y2minus2[face][idx] * solidangle;
                redSHCoeff[5] += r * Y2minus1[face][idx] * solidangle;
                redSHCoeff[6] += r * Y20[face][idx] * solidangle;
                redSHCoeff[7] += r * Y21[face][idx] * solidangle;
                redSHCoeff[8] += r * Y22[face][idx] * solidangle;
            }
        }
    }
}

static void displayCoeff(float *SHCoeff)
{
    printf("L00:%f\n", SHCoeff[0]);
    printf("L1-1:%f, L10:%f, L11:%f\n", SHCoeff[1], SHCoeff[2], SHCoeff[3]);
    printf("L2-2:%f, L2-1:%f, L20:%f, L21:%f, L22:%f\n", SHCoeff[4], SHCoeff[5], SHCoeff[6], SHCoeff[7], SHCoeff[8]);
}

// Only for 9 coefficients
static void testSH(unsigned char *color[6], size_t width, size_t height,
    float *blueSHCoeff, float *greenSHCoeff, float *redSHCoeff)
{
    float *Y00[6];
    float *Y1minus1[6];
    float *Y10[6];
    float *Y11[6];
    float *Y2minus2[6];
    float *Y2minus1[6];
    float *Y20[6];
    float *Y21[6];
    float *Y22[6];

    float *testoutput[6];
    for (unsigned i = 0; i < 6; i++)
    {
        testoutput[i] = new float[width * height * 4];
        for (unsigned j = 0; j < width * height; j++)
        {
            testoutput[i][4 * j] = float(0xFF & color[i][4 * j]);
            testoutput[i][4 * j + 1] = float(0xFF & color[i][4 * j + 1]);
            testoutput[i][4 * j + 2] = float(0xFF & color[i][4 * j + 2]);
        }
    }

    for (unsigned face = 0; face < 6; face++)
    {
        Y00[face] = new float[width * height];
        Y1minus1[face] = new float[width * height];
        Y10[face] = new float[width * height];
        Y11[face] = new float[width * height];
        Y2minus2[face] = new float[width * height];
        Y2minus1[face] = new float[width * height];
        Y20[face] = new float[width * height];
        Y21[face] = new float[width * height];
        Y22[face] = new float[width * height];

        getYml(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, width, height, Y00[face], Y1minus1[face], Y10[face], Y11[face], Y2minus2[face], Y2minus1[face], Y20[face], Y21[face], Y22[face]);
    }

    /*    blueSHCoeff[0] = 0.54,
    blueSHCoeff[1] = .6, blueSHCoeff[2] = -.27, blueSHCoeff[3] = .01,
    blueSHCoeff[4] = -.12, blueSHCoeff[5] = -.47, blueSHCoeff[6] = -.15, blueSHCoeff[7] = .14, blueSHCoeff[8] = -.3;
    greenSHCoeff[0] = .44,
    greenSHCoeff[1] = .35, greenSHCoeff[2] = -.18, greenSHCoeff[3] = -.06,
    greenSHCoeff[4] = -.05, greenSHCoeff[5] = -.22, greenSHCoeff[6] = -.09, greenSHCoeff[7] = .21, greenSHCoeff[8] = -.05;
    redSHCoeff[0] = .79,
    redSHCoeff[1] = .39, redSHCoeff[2] = -.34, redSHCoeff[3] = -.29,
    redSHCoeff[4] = -.11, redSHCoeff[5] = -.26, redSHCoeff[6] = -.16, redSHCoeff[7] = .56, redSHCoeff[8] = .21;

    printf("Blue:\n");
    displayCoeff(blueSHCoeff);
    printf("Green:\n");
    displayCoeff(greenSHCoeff);
    printf("Red:\n");
    displayCoeff(redSHCoeff);*/

    projectSH(testoutput, width, height,
        Y00,
        Y1minus1, Y10, Y11,
        Y2minus2, Y2minus1, Y20, Y21, Y22,
        blueSHCoeff, greenSHCoeff, redSHCoeff
        );

    //printf("Blue:\n");
    //displayCoeff(blueSHCoeff);
    //printf("Green:\n");
    //displayCoeff(greenSHCoeff);
    //printf("Red:\n");
    //displayCoeff(redSHCoeff);



    // Convolute in frequency space
    /*    float A0 = 3.141593;
    float A1 = 2.094395;
    float A2 = 0.785398;
    blueSHCoeff[0] *= A0;
    greenSHCoeff[0] *= A0;
    redSHCoeff[0] *= A0;
    for (unsigned i = 0; i < 3; i++)
    {
    blueSHCoeff[1 + i] *= A1;
    greenSHCoeff[1 + i] *= A1;
    redSHCoeff[1 + i] *= A1;
    }
    for (unsigned i = 0; i < 5; i++)
    {
    blueSHCoeff[4 + i] *= A2;
    greenSHCoeff[4 + i] *= A2;
    redSHCoeff[4 + i] *= A2;
    }


    unprojectSH(testoutput, width, height,
    Y00,
    Y1minus1, Y10, Y11,
    Y2minus2, Y2minus1, Y20, Y21, Y22,
    blueSHCoeff, greenSHCoeff, redSHCoeff
    );*/


    /*    printf("Blue:\n");
    displayCoeff(blueSHCoeff);
    printf("Green:\n");
    displayCoeff(greenSHCoeff);
    printf("Red:\n");
    displayCoeff(redSHCoeff);

    printf("\nAfter projection\n\n");*/



    for (unsigned i = 0; i < 6; i++)
    {
        for (unsigned j = 0; j < width * height; j++)
        {
            color[i][4 * j] = char(MIN2(testoutput[i][4 * j], 255));
            color[i][4 * j + 1] = char(MIN2(testoutput[i][4 * j + 1], 255));
            color[i][4 * j + 2] = char(MIN2(testoutput[i][4 * j + 2], 255));
        }
    }

    for (unsigned face = 0; face < 6; face++)
    {
        delete[] testoutput[face];
        delete[] Y00[face];
        delete[] Y1minus1[face];
        delete[] Y10[face];
        delete[] Y11[face];
        delete[] Y2minus2[face];
        delete[] Y2minus1[face];
        delete[] Y20[face];
        delete[] Y21[face];
        delete[] Y22[face];
    }
}

/** Generate an opengl cubemap texture from 6 2d textures.
Out of legacy the sequence of textures maps to :
- 1st texture maps to GL_TEXTURE_CUBE_MAP_POSITIVE_Y
- 2nd texture maps to GL_TEXTURE_CUBE_MAP_NEGATIVE_Y
- 3rd texture maps to GL_TEXTURE_CUBE_MAP_POSITIVE_X
- 4th texture maps to GL_TEXTURE_CUBE_MAP_NEGATIVE_X
- 5th texture maps to GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
- 6th texture maps to GL_TEXTURE_CUBE_MAP_POSITIVE_Z
*  \param textures sequence of 6 textures.
*/
GLuint generateCubeMapFromTextures(const std::vector<video::ITexture *> &textures)
{
    assert(textures.size() == 6);

    GLuint result;
    glGenTextures(1, &result);

    unsigned w = 0, h = 0;
    for (unsigned i = 0; i < 6; i++)
    {
        w = MAX2(w, textures[i]->getOriginalSize().Width);
        h = MAX2(h, textures[i]->getOriginalSize().Height);
    }

    const unsigned texture_permutation[] = { 2, 3, 0, 1, 5, 4 };
    char *rgba[6];
    for (unsigned i = 0; i < 6; i++)
        rgba[i] = new char[w * h * 4];
    for (unsigned i = 0; i < 6; i++)
    {
        unsigned idx = texture_permutation[i];

        video::IImage* image = irr_driver->getVideoDriver()->createImageFromData(
            textures[idx]->getColorFormat(),
            textures[idx]->getSize(),
            textures[idx]->lock(),
            false
            );
        textures[idx]->unlock();

        image->copyToScaling(rgba[i], w, h);
        image->drop();

        glBindTexture(GL_TEXTURE_CUBE_MAP, result);
        if (UserConfigParams::m_texture_compression)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_COMPRESSED_SRGB_ALPHA, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, (GLvoid*)rgba[i]);
        else
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_SRGB_ALPHA, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, (GLvoid*)rgba[i]);
    }
    for (unsigned i = 0; i < 6; i++)
        delete[] rgba[i];
    return result;
}

void IrrDriver::generateSkyboxCubemap()
{
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    assert(SkyboxTextures.size() == 6);
    SkyboxCubeMap = generateCubeMapFromTextures(SkyboxTextures);
}

void IrrDriver::generateDiffuseCoefficients()
{
    if (!m_SH_dirty)
        return;
    m_SH_dirty = false;
    const unsigned texture_permutation[] = { 2, 3, 0, 1, 5, 4 };

    if (SphericalHarmonicsTextures.size() == 6)
    {
        unsigned sh_w = 0, sh_h = 0;
        for (unsigned i = 0; i < 6; i++)
        {
            sh_w = MAX2(sh_w, SphericalHarmonicsTextures[i]->getOriginalSize().Width);
            sh_h = MAX2(sh_h, SphericalHarmonicsTextures[i]->getOriginalSize().Height);
        }

        unsigned char *sh_rgba[6];
        for (unsigned i = 0; i < 6; i++)
            sh_rgba[i] = new unsigned char[sh_w * sh_h * 4];
        for (unsigned i = 0; i < 6; i++)
        {
            unsigned idx = texture_permutation[i];

            video::IImage* image = getVideoDriver()->createImageFromData(
                SphericalHarmonicsTextures[idx]->getColorFormat(),
                SphericalHarmonicsTextures[idx]->getSize(),
                SphericalHarmonicsTextures[idx]->lock(),
                false
                );
            SphericalHarmonicsTextures[idx]->unlock();

            image->copyToScaling(sh_rgba[i], sh_w, sh_h);
            delete image;
        }

        testSH(sh_rgba, sh_w, sh_h, blueSHCoeff, greenSHCoeff, redSHCoeff);

        for (unsigned i = 0; i < 6; i++)
            delete[] sh_rgba[i];
    }
    else
    {
        int sh_w = 16;
        int sh_h = 16;

        const video::SColorf& ambientf = irr_driver->getSceneManager()->getAmbientLight();
        video::SColor ambient = ambientf.toSColor();

        unsigned char *sh_rgba[6];
        for (unsigned i = 0; i < 6; i++)
        {
            sh_rgba[i] = new unsigned char[sh_w * sh_h * 4];

            for (int j = 0; j < sh_w * sh_h * 4; j += 4)
            {
                sh_rgba[i][j] = ambient.getBlue();
                sh_rgba[i][j + 1] = ambient.getGreen();
                sh_rgba[i][j + 2] = ambient.getRed();
                sh_rgba[i][j + 3] = 255;
            }
        }

        testSH(sh_rgba, sh_w, sh_h, blueSHCoeff, greenSHCoeff, redSHCoeff);

        for (unsigned i = 0; i < 6; i++)
            delete[] sh_rgba[i];
    }

    /*for (unsigned i = 0; i < 6; i++)
    {
    glBindTexture(GL_TEXTURE_CUBE_MAP, ConvolutedSkyboxCubeMap);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_SRGB_ALPHA, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, (GLvoid*)rgba[i]);
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);*/

}

void IrrDriver::renderSkybox(const scene::ICameraSceneNode *camera)
{
    if (SkyboxTextures.empty())
        return;
    if (!SkyboxCubeMap)
        generateSkyboxCubemap();
    glBindVertexArray(MeshShader::SkyboxShader::cubevao);
    glDisable(GL_CULL_FACE);
    assert(SkyboxTextures.size() == 6);

    core::matrix4 translate;
    translate.setTranslation(camera->getAbsolutePosition());

    // Draw the sky box between the near and far clip plane
    const f32 viewDistance = (camera->getNearValue() + camera->getFarValue()) * 0.5f;
    core::matrix4 scale;
    scale.setScale(core::vector3df(viewDistance, viewDistance, viewDistance));
    core::matrix4 transform = translate * scale;
    core::matrix4 invtransform;
    transform.getInverse(invtransform);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, SkyboxCubeMap);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glUseProgram(MeshShader::SkyboxShader::Program);
    MeshShader::SkyboxShader::setUniforms(transform,
        core::vector2df(float(UserConfigParams::m_width),
        float(UserConfigParams::m_height)),
        0);
    glDrawElements(GL_TRIANGLES, 6 * 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}