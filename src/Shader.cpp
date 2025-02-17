#include "shader.h"
#include "scene.h"
#include "mathLib.h"

#define ONE_OVER_PI 0.318309886

/// \TODO: Simplify logic and optimization
void DrawLine(Vec2<int> Start, Vec2<int> End, TGAImage& image, const TGAColor& color)
{
    int d = 1;
    //**** Divide by 0
    if (Start.x == End.x)
    {
        if (Start.y > End.y) Start.Swap(End);
        for (int i = Start.y; i <= End.y; i++)
        {
            image.set(Start.x, i, color);
        }
        return;
    }
    // Always start from left marching toward right
    // Swap Start and End
    if (Start.x > End.x) Start.Swap(End);
    // Derive the line function
    float Slope = (float)(Start.y - End.y) / (float)(Start.x - End.x);
    float Intercept = (float)End.y - Slope * End.x;
    Vec2<int> Next(Start);
    Vec2<int> StepA(1, 0);
    Vec2<int> StepB(1, 1);

    if (Slope < 0)
    {
        d *= -1;
        StepA.Swap(StepB);
    }
    if (Slope > 1 || Slope < -1)
    {
        StepA.Transpose();
        StepB.Transpose();
    }

    StepA.y *= d;
    StepB.y *= d;

    while (Next.x < End.x || Next.y != End.y)
    {
        //**** Slope < 1
        if (Slope >= -1 && Slope <= 1)
        {
            // Eval F(x+1,y+0.5)
            if (Next.y + d * 0.5f - Slope * (Next.x + 1.0f) - Intercept >= 0)
                Next += StepA;
            else
                Next += StepB;
        }
        /// \Bug: When slope is greater than 0, can invert x, y
        else
        {
            // Eval F(x+0.5, y+1)
            if (Next.y + d * 1.0f - Slope * (Next.x + .5f) - Intercept >= 0)
                Next += StepB;
            else
                Next += StepA;
        }
        //**** Draw pixel to the buffer
        image.set(Next.x, Next.y, color);
    }
}

ShaderBase::ShaderBase() {
    vertexAttribFlag = 1;
    normalMap_ = nullptr;
    fragmentAttribBuffer = nullptr;
    lightParamsBuffer = nullptr;
}

void gammaCorrection(Vec3<float>& color, float pow) {
    color.x = std::pow(color.x, pow);
    color.y = std::pow(color.y, pow);
    color.z = std::pow(color.z, pow);
}

void ShaderBase::initFragmentAttrib(uint32_t bufferWidth, uint32_t bufferHeight) {
    bufferWidth_ = bufferWidth;
    bufferHeight_ = bufferHeight;
    fragmentAttribBuffer = new FragmentAttrib[bufferWidth * bufferHeight];
    lightParamsBuffer = new LightParams[bufferWidth * bufferHeight];
}

Vec3<float> ShaderBase::sampleTexture2D(Texture& texture, float u, float v) {
    Vec3<float> res;
    uint32_t samplePosX = fmod(u, 1.f) * texture.textureWidth;
    // 1 - v here because the texture image coord y starts at the top while
    // texture coord y starts from bottom, thus flip vertically 
    uint32_t samplePosY = (1.f - fmod(v, 1.f)) * texture.textureHeight;
    uint32_t pixelIdx = texture.textureWidth * samplePosY + samplePosX;
    res.x = (float)(texture.pixels[pixelIdx * texture.numChannels]);     // R
    res.y = (float)(texture.pixels[pixelIdx * texture.numChannels + 1]); // G
    res.z = (float)(texture.pixels[pixelIdx * texture.numChannels + 2]); // B
    return res;
}

Vec3<float> ShaderBase::sampleNormal(Texture& normalMap, float u, float v) {
    Vec3<float> res;
    uint32_t samplePosX = u * normalMap.textureWidth;
    // 1 - v here because the texture image coord y starts at the top while
    // texture coord y starts from bottom, thus flip vertically 
    uint32_t samplePosY = (1 - v) * normalMap.textureHeight;
    uint32_t pixelIdx = normalMap.textureWidth * samplePosY + samplePosX;
    res.x = normalMap.pixels[pixelIdx * normalMap.numChannels];     // R
    res.y = normalMap.pixels[pixelIdx * normalMap.numChannels + 1]; // G
    res.z = normalMap.pixels[pixelIdx * normalMap.numChannels + 2]; // B
    // normalize the component value from [0, 255] to [-1, 1]
    res.x = (float)res.x / 255.f * 2 - 1.f; 
    res.y = (float)res.y / 255.f * 2 - 1.f; 
    res.z = (float)res.z / 255.f * 2 - 1.f; 
    // Do I need to normalize the remapped result here?
    return Math::normalize(res);
}

float ShaderBase::sampleGreyScale(Texture& textureMap, float u, float v) {
    float res;
    uint32_t samplePosX = u * textureMap.textureWidth;
    uint32_t samplePosY = (1 - v) * textureMap.textureHeight;
    uint32_t pixelIdx = textureMap.textureWidth * samplePosY + samplePosX;
    res = textureMap.pixels[pixelIdx * textureMap.numChannels];
    return res / 255.f;
}

Vec3<float> ShaderBase::transformToViewSpace(Vec3<float>& v) {
    modelView_ = view_ * model_;
    Vec4<float> pos = modelView_ * Vec4<float>(v, 1.f);
    return Vec3<float>(pos.x, pos.y, pos.z);
}

void ShaderBase::set_model_matrix(Mat4x4<float>& model) {
    model_ = model;
}

void ShaderBase::set_view_matrix(Mat4x4<float>& view) {
    view_ = view;
}

void ShaderBase::set_projection_matrix(Mat4x4<float>& projection) {
    projection_ = projection;
}

Vec3<float> ShaderBase::transformNormal(Vec3<float>& normal) {
    Vec4<float> result = view_ * model_ * Vec4<float>(normal, 0.f);
    return Vec3<float>(result.x, result.y, result.z);
}

Vec3<float> ShaderBase::transformTangent(Vec3<float>& tangent) {
    Vec4<float> result = view_ * model_ * Vec4<float>(tangent, 0.f);
    return Vec3<float>(result.x, result.y, result.z);
}

void ShaderBase::bindDiffuseTexture(Texture* texture) {
    diffuseMaps.emplace_back(texture);
}

void ShaderBase::bindSpecTexture(Texture* texture) {
    specularMaps.emplace_back(texture);
}

void ShaderBase::unbindTexture() {
    diffuseMaps.clear();
}

void ShaderBase::clearFragmentAttribs() {
    #pragma omp parallel for
    for (int fragment = 0; fragment < bufferHeight_ * bufferWidth_; fragment++) {
        fragmentAttribBuffer[fragment] = { };
        // TODO: do i need to refresh lighting params as well? probably not
    }
}

// NOTES: skybox shader does not use modelView_ since the model matrix
//        was never set and it does not need model matrix
Vec4<float> SkyboxShader::vertexShader(Vec3<float>& v) {
    Vec4<float> glPosition = projection_ * view_ * Vec4<float>(v, 1.f);
    glPosition.z = glPosition.w;
    return glPosition;
}

// TODO: bug here, part of the cube is not rendered for some reason
Vec4<int> SkyboxShader::fragmentShader(int x, int y) {
    Vec4<int> sampleColor(100, 100, 100, 255);
    // sample based on direction
    return sampleColor;
}

PhongShader::PhongShader() {

}

Vec4<float> PhongShader::vertexShader(Vec3<float>& v) {
    return projection_ * modelView_ * Vec4<float>(v, 1.f);    
}

// Fragment normal
// Fragment textureUV
// Fragment lightDirection
// Fragment viewDirection
// TODO: if a given mesh does not come with vertex normal
//     : generate normals at loading and feed interpolated vn in here instead
//     : of using normal map
// TODO: still need to figure out why specular highlight looks kinda weird
// TODO: Fix the gamma
Vec4<int> PhongShader::fragmentShader(int x, int y) {
    Vec3<float> phongColor(0.f);
    FragmentAttrib& attribs = fragmentAttribBuffer[y * bufferWidth_ + x];
    LightParams& lightParams = lightParamsBuffer[y * bufferWidth_ + x];
    Vec3<float> diffuseSample = {}, specularSample = {}, diffuse = {}, specular = {};

    // default diffuse color
    diffuseSample = Vec3<float>(230.f, 154.f, 222.f);
    gammaCorrection(diffuseSample, 2.2f);
    // TODO: Handle texture blending, right now simply add
    if (diffuseMaps.size() > 0) {
        Vec3<float> diffuseSampleSum(0.f, 0.f, 0.f);
        for (auto diffuseMap : diffuseMaps) {
            diffuseSample = sampleTexture2D(*diffuseMap, attribs.textureCoord.x, attribs.textureCoord.y);
            diffuseSample /= 255.f;
            gammaCorrection(diffuseSample, 2.2);
            diffuseSampleSum += diffuseSample;
        }
        diffuseSample = diffuseSampleSum;
    }
    if (specularMaps.size() > 0) {
        // rn there is only one specular map, so
        for (auto specularMap : specularMaps) {
            specularSample = sampleTexture2D(*specularMap, attribs.textureCoord.x, attribs.textureCoord.y);
        }
    }
    Vec3<float> normal = attribs.normal; // make sure normals are normalized
    if (normalMap_) {
        Vec3<float> sampledNormal = sampleNormal(*normalMap_, attribs.textureCoord.x, attribs.textureCoord.y); 
        // TODO: Need to consider the handed-ness of the tangent space where normals are defined in
        Vec3<float> interpolatedNormal = attribs.normal;
        Vec3<float> tangent = attribs.tangent;
        // Gram-Schemidt process to re-orthoganize the tangent and fragmentNormal
        tangent = Math::normalize(tangent - interpolatedNormal * Math::dotProductVec3(tangent, interpolatedNormal));
        Vec3<float> biTangent = Math::CrossProduct(tangent, interpolatedNormal);
        normal.x = tangent.x * sampledNormal.x + biTangent.x * sampledNormal.y + interpolatedNormal.x * sampledNormal.z;
        normal.y = tangent.y * sampledNormal.x + biTangent.y * sampledNormal.y + interpolatedNormal.y * sampledNormal.z;
        normal.z = tangent.z * sampledNormal.x + biTangent.z * sampledNormal.y + interpolatedNormal.z * sampledNormal.z;
    } 

    Vec3<float> viewDirection = Math::normalize(cameraPos - lightParams.viewSpaceFragmentPos);
    for (auto light : dirLights) {
        Vec3<float> reflectRay = Math::reflect(light.direction, normal);
        float diffuseCoef = Math::clamp_f(Math::dotProductVec3(normal, light.direction), 0.f, 1.f);
        float specularCoef = Math::clamp_f(Math::dotProductVec3(viewDirection, reflectRay), 0.f, 1.f);
        if (specularMaps.size() > 0) {
            if (specularSample.x != 0.f) {
                specularCoef = std::max(pow(specularCoef, specularSample.x), 0.f);
            }  
        } else {
            specularCoef = std::max(pow(specularCoef, 4.f), 0.f);
        }  
        specular = diffuseSample * specularCoef; 
        diffuse = diffuseSample * light.color * diffuseCoef;
        phongColor += diffuse * .3 + specular * .7f;
    }
    gammaCorrection(phongColor, 0.4545);
    return Vec4<int>(Math::clampRGB(phongColor * 255.f), 255);
}

Vec4<float> DepthShader::vertexShader(Vec3<float>& v) {
    return projection_ * view_ * model_ * Vec4<float>(v, 1.f);
}

Vec4<int> DepthShader::fragmentShader(int x, int y) {
    Vec3<float> flatColor(.7f), color(0.f);
    uint32_t attribIdx = y * bufferWidth_ + x;
    FragmentAttrib fragmentAttrib = fragmentAttribBuffer[attribIdx];
    Vec3<float> normal = fragmentAttrib.normal;

    for (auto light : dirLights) {
        color += flatColor * Math::clamp_f(Math::dotProductVec3(normal, light.direction), 0.f, 1.f);
    }
    gammaCorrection(color, .4545f);
    return Vec4<int>(Math::clampRGB(color * 255.f), 255);
}

Vec4<float> PBRShader::vertexShader(Vec3<float>& v) {
    return projection_ * view_ * model_ * Vec4<float>(v, 1.f);
}

Vec3<float> lambertianBRDF(Vec3<float> albedo) {
    return albedo * ONE_OVER_PI; 
}

// GGX normal distribution function
float computeNDF(float ndoth, float roughness) {
    float alpha = roughness * roughness;
    float denominator = ndoth * ndoth * (alpha * alpha - 1.f) + 1.f;
    denominator *= denominator;
    float numerator = alpha * alpha * ONE_OVER_PI;
    return numerator / denominator;
}

// schlick GGX
float computeGTerm(float ndotw, float roughness) {
    float alpha = roughness * roughness;
    float numerator = 2 * ndotw;
    // Approximate divide by 0
    float denominator = std::max(ndotw + sqrt(alpha * alpha + (1 - alpha * alpha) * ndotw * ndotw), 0.00001f);
    float g = Math::clamp_f(numerator / denominator, 0.f, 1.f);
    return g;
}

// Geometric term is derived from normal distribution function, thus
// have to use one that's according with the normal distribution function that I use
// GGX using Smith's method  
// TODO: Research marker here
float computeGeometricShadowing(float ndotv, float ndotl, float roughness) {
    float gl, gv;
    gv = computeGTerm(ndotv, roughness);
    gl = computeGTerm(ndotl, roughness);
    return gv * gl;
}

// f0 denote reflectance at normal incidence
Vec3<float> computeFresnel(Vec3<float>& v, Vec3<float>& h, Vec3<float>& f0) {
    float vdoth = std::max(Math::dotProductVec3(v, h), 0.f);
    Vec3<float> fresnel = f0 + (Vec3<float>(1.f) - f0) * pow(1.f - vdoth, 5.f);
    return fresnel;
}

Vec3<float> cookTorranceBRDF(Vec3<float>& h, Vec3<float>& v, Vec3<float>& l, Vec3<float>& n, Vec3<float>& albedo, Vec3<float> specularColor, float roughness) {
    Vec3<float> cookTorranceSpecular;
    float ndotv = Math::clamp_f(Math::dotProductVec3(n, v), 0.f, 1.f);
    float ndotl = Math::clamp_f(Math::dotProductVec3(n, l), 0.f, 1.f);
    float ndoth = Math::clamp_f(Math::dotProductVec3(n, h), 0.f, 1.f);
    float d = computeNDF(ndoth, roughness);
    float g = computeGeometricShadowing(ndotv, ndotl, roughness); 

    cookTorranceSpecular = specularColor * (d * g); 
    // Approximate the case when denominator becomes 0
    float oneOverDenominator = 1.f / std::max(4.f * ndotv * ndotl, 0.00001f); 
    Math::clampVec3f(cookTorranceSpecular * oneOverDenominator, 0.f, 1.f);
    return cookTorranceSpecular;
}

// Two parameters that control the appearance, metal and roughness
Vec4<int> PBRShader::fragmentShader(int x, int y) {
    Vec3<float> linearColor(0.f), normal;
    Vec3<float> diffuseAlbedo(0.f, 0.f, 0.f), diffuseTerm, specularTerm; // in linear color space
    FragmentAttrib fragmentAttrib = fragmentAttribBuffer[y * bufferWidth_ + x];
    LightParams lightParams = lightParamsBuffer[y * bufferWidth_ + x];
    Vec3<float> viewDir = Math::normalize(lightParamsBuffer[y * bufferWidth_ + x].viewSpaceFragmentPos * -1.f);
    
    if (diffuseMaps.size() > 0) {
        for (auto diffuseMap : diffuseMaps) {
            Vec3<float> rgbSample = sampleTexture2D(*diffuseMap, fragmentAttrib.textureCoord.x, fragmentAttrib.textureCoord.y);
            rgbSample /= 255.f; // normalized rgb value to [0, 1]
            gammaCorrection(rgbSample, 2.2f);
            diffuseAlbedo += rgbSample;
        }
    } else {
        diffuseAlbedo = Vec3<float>(196.f, 132.f, 217.f) / 255.f;
    }

    normal = fragmentAttrib.normal;
    // Fetch normal from normal map
    if (normalMap_) {
        Vec3<float> sampledNormal = sampleNormal(*normalMap_, fragmentAttrib.textureCoord.x, fragmentAttrib.textureCoord.y); 
        // TODO: Need to consider the handed-ness of the tangent space where normals are defined in
        Vec3<float> interpolatedNormal = fragmentAttrib.normal;
        Vec3<float> tangent = fragmentAttrib.tangent;
        // Gram-Schemidt process to re-orthoganize the tangent and fragmentNormal
        tangent = Math::normalize(tangent - interpolatedNormal * Math::dotProductVec3(tangent, interpolatedNormal));
        Vec3<float> biTangent = Math::CrossProduct(tangent, interpolatedNormal);
        normal.x = tangent.x * sampledNormal.x + biTangent.x * sampledNormal.y + interpolatedNormal.x * sampledNormal.z;
        normal.y = tangent.y * sampledNormal.x + biTangent.y * sampledNormal.y + interpolatedNormal.y * sampledNormal.z;
        normal.z = tangent.z * sampledNormal.x + biTangent.z * sampledNormal.y + interpolatedNormal.z * sampledNormal.z;
    } 

    // Default roughness and metal
    float metallic = 0.2f, roughness = 1.0f, ao = 1.f;
    if (roughnessMap_) {
        roughness = sampleGreyScale(*roughnessMap_, fragmentAttrib.textureCoord.x, fragmentAttrib.textureCoord.y);
        // metallic = 1.f - roughness;
    }
    if (aoMap_) {
        ao = sampleGreyScale(*aoMap_, fragmentAttrib.textureCoord.x, fragmentAttrib.textureCoord.y);
    }
    for (auto light : dirLights) {
        Vec3<float> halfVector = Math::normalize(viewDir + light.direction);
        // TODO: if the object is dieletric then ignore diffuseAlbedo, else tint the reflection with diffuse color (one property of conductior)
        //       maybe should use index of refraction to compute f0 to be more accurate?
        Vec3<float> specularColor = Vec3<float>(.02f) * (1 - metallic) + diffuseAlbedo * metallic;
        // Diffuse term: Lambertian BRDF
        diffuseTerm = lambertianBRDF(diffuseAlbedo);
        // Specular term: Cook-Torrance microfacet specular BRDF
        Vec3<float> fresnel = computeFresnel(viewDir, halfVector, specularColor);
        specularTerm = cookTorranceBRDF(halfVector, viewDir, light.direction, normal, diffuseAlbedo, fresnel, roughness);
        // Put together diffuse and specular and attenuate it with cos
        // TODO: take the light color into consideration
        Vec3<float> kd = Vec3<float>(1.f) - fresnel;
        kd *=  1 - metallic;
        linearColor += (diffuseTerm * ao * kd + specularTerm) * light.color * Math::clamp_f(Math::dotProductVec3(normal, light.direction), 0.f, 1.f); 
    }
    gammaCorrection(linearColor, .4545f);
    return Vec4<int>(Math::clampRGB(linearColor * 255.f), 255);
}