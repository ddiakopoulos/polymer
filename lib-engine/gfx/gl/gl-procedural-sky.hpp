#ifndef procedural_sky_h
#define procedural_sky_h

#include "math-core.hpp"
#include "util.hpp"
#include "gl-api.hpp"
#include "gl-procedural-mesh.hpp"
#include "gl-loaders.hpp"

#include "../../shader.hpp"
#include "../../asset-handle-utils.hpp"
#include "../../serialization.hpp"

#include <functional>

#if defined(POLYMER_PLATFORM_WINDOWS)
#pragma warning(push)
#pragma warning(disable : 4244)
#pragma warning(disable : 4305)
#endif

namespace
{
    #include "hosek_data_rgb.inl"
}

namespace detail
{
    using namespace polymer;

    inline double evaluate_spline(const double * spline, size_t stride, double value)
    {
        return
        1 *  pow(1 - value, 5) *                 spline[0 * stride] +
        5 *  pow(1 - value, 4) * pow(value, 1) * spline[1 * stride] +
        10 * pow(1 - value, 3) * pow(value, 2) * spline[2 * stride] +
        10 * pow(1 - value, 2) * pow(value, 3) * spline[3 * stride] +
        5 *  pow(1 - value, 1) * pow(value, 4) * spline[4 * stride] +
        1 *                      pow(value, 5) * spline[5 * stride];
    }
    
    inline double evaluate(const double * dataset, size_t stride, float turbidity, float albedo, float sunTheta)
    {
        // splines are functions of elevation^1/3
        double elevationK = pow(std::max<float>(0.f, 1.f - sunTheta / (POLYMER_PI / 2.f)), 1.f / 3.0f);
        
        // table has values for turbidity 1..10
        int turbidity0 = polymer::clamp<int>(static_cast<int>(turbidity), 1, 10);
        int turbidity1 = std::min(turbidity0 + 1, 10);
        float turbidityK = polymer::clamp(turbidity - turbidity0, 0.f, 1.f);
        
        const double * datasetA0 = dataset;
        const double * datasetA1 = dataset + stride * 6 * 10;
        
        double a0t0 = evaluate_spline(datasetA0 + stride * 6 * (turbidity0 - 1), stride, elevationK);
        double a1t0 = evaluate_spline(datasetA1 + stride * 6 * (turbidity0 - 1), stride, elevationK);
        double a0t1 = evaluate_spline(datasetA0 + stride * 6 * (turbidity1 - 1), stride, elevationK);
        double a1t1 = evaluate_spline(datasetA1 + stride * 6 * (turbidity1 - 1), stride, elevationK);
        
        return a0t0 * (1 - albedo) * (1 - turbidityK) + a1t0 * albedo * (1 - turbidityK) + a0t1 * (1 - albedo) * turbidityK + a1t1 * albedo * turbidityK;
    }
    
    inline polymer::float3 hosek_wilkie(float cos_theta, float gamma, float cos_gamma, polymer::float3 A, polymer::float3 B, polymer::float3 C, polymer::float3 D, polymer::float3 E, polymer::float3 F, polymer::float3 G, polymer::float3 H, polymer::float3 I)
    {
        polymer::float3 chi = (1.f + cos_gamma * cos_gamma) / pow(1.f + H * H - 2.f * cos_gamma * H, polymer::float3(1.5f));
        return (1.f + A * exp(B / (cos_theta + 0.01f))) * (C + D * exp(E * gamma) + F * (cos_gamma * cos_gamma) + G * chi + I * (float) sqrt(std::max(0.f, cos_theta)));
    }
    
    inline float perez(float theta, float gamma, float A, float B, float C, float D, float E)
    {
        return (1.f + A * exp(B / (cos(theta) + 0.01))) * (1.f + C * exp(D * gamma) + E * cos(gamma) * cos(gamma));
    }
    
    inline float zenith_luminance(float sunTheta, float turbidity)
    {
        float chi = (4.f / 9.f - turbidity / 120) * (POLYMER_PI - 2 * sunTheta);
        return (4.0453 * turbidity - 4.9710) * tan(chi) - 0.2155 * turbidity + 2.4192;
    }
    
    inline float zenith_chromacity(const polymer::float4 & c0, const polymer::float4 & c1, const polymer::float4 & c2, float sunTheta, float turbidity)
    {
        polymer::float4 thetav = polymer::float4(sunTheta * sunTheta * sunTheta, sunTheta * sunTheta, sunTheta, 1);
        return dot(polymer::float3(turbidity * turbidity, turbidity, 1), polymer::float3(dot(thetav, c0), dot(thetav, c1), dot(thetav, c2)));
    }

    // An Analytic Model for Full Spectral Sky-Dome Radiance (Lukas Hosek, Alexander Wilkie)
    struct HosekSkyRadianceData
    {
        float3 A, B, C, D, E, F, G, H, I;
        float3 Z;
        static HosekSkyRadianceData compute(float3 sun_direction, float turbidity, float albedo, float normalizedSunY);
    };
    
    // A Practical Analytic Model for Daylight (A. J. Preetham, Peter Shirley, Brian Smits)
    struct PreethamSkyRadianceData
    {
        float3 A, B, C, D, E;
        float3 Z;
        static PreethamSkyRadianceData compute(float3 sun_direction, float turbidity, float albedo, float normalizedSunY);
    };

    inline HosekSkyRadianceData HosekSkyRadianceData::compute(float3 sun_direction, float turbidity, float albedo, float normalizedSunY)
    {
        float3 A, B, C, D, E, F, G, H, I;
        float3 Z;
    
        const float sunTheta = std::acos(clamp(sun_direction.y, 0.f, 1.f));

        for (int i = 0; i < 3; ++i)
        {
            A[i] = evaluate(::datasetsRGB[i] + 0, 9, turbidity, albedo, sunTheta);
            B[i] = evaluate(::datasetsRGB[i] + 1, 9, turbidity, albedo, sunTheta);
            C[i] = evaluate(::datasetsRGB[i] + 2, 9, turbidity, albedo, sunTheta);
            D[i] = evaluate(::datasetsRGB[i] + 3, 9, turbidity, albedo, sunTheta);
            E[i] = evaluate(::datasetsRGB[i] + 4, 9, turbidity, albedo, sunTheta);
            F[i] = evaluate(::datasetsRGB[i] + 5, 9, turbidity, albedo, sunTheta);
            G[i] = evaluate(::datasetsRGB[i] + 6, 9, turbidity, albedo, sunTheta);
        
            // Swapped in the dataset
            H[i] = evaluate(::datasetsRGB[i] + 8, 9, turbidity, albedo, sunTheta);
            I[i] = evaluate(::datasetsRGB[i] + 7, 9, turbidity, albedo, sunTheta);
        
            Z[i] = evaluate(::datasetsRGBRad[i], 1, turbidity, albedo, sunTheta);
        }
    
        if (normalizedSunY)
        {
            float3 S = hosek_wilkie(std::cos(sunTheta), 0, 1.f, A, B, C, D, E, F, G, H, I) * Z;
            Z /= dot(S, float3(0.2126, 0.7152, 0.0722));
            Z *= normalizedSunY;
        }
    
        return {A, B, C, D, E, F, G, H, I, Z};
    }
    
    inline PreethamSkyRadianceData PreethamSkyRadianceData::compute(float3 sun_direction, float turbidity, float albedo, float normalizedSunY)
    {
        assert(turbidity >= 1);
    
        const float sunTheta = std::acos(clamp(sun_direction.y, 0.f, 1.f));

        // A.2 Skylight Distribution Coefficients and Zenith Values: compute Perez distribution coefficients
        float3 A = float3(-0.0193, -0.0167,  0.1787) * turbidity + float3(-0.2592, -0.2608, -1.4630);
        float3 B = float3(-0.0665, -0.0950, -0.3554) * turbidity + float3( 0.0008,  0.0092,  0.4275);
        float3 C = float3(-0.0004, -0.0079, -0.0227) * turbidity + float3( 0.2125,  0.2102,  5.3251);
        float3 D = float3(-0.0641, -0.0441,  0.1206) * turbidity + float3(-0.8989, -1.6537, -2.5771);
        float3 E = float3(-0.0033, -0.0109, -0.0670) * turbidity + float3( 0.0452,  0.0529,  0.3703);
    
        // A.2 Skylight Distribution Coefficients and Zenith Values: compute zenith color
        float3 Z;
        Z.x = zenith_chromacity(float4(0.00166, -0.00375, 0.00209, 0), float4(-0.02903, 0.06377, -0.03202, 0.00394), float4(0.11693, -0.21196, 0.06052, 0.25886), sunTheta, turbidity);
        Z.y = zenith_chromacity(float4(0.00275, -0.00610, 0.00317, 0), float4(-0.04214, 0.08970, -0.04153, 0.00516), float4(0.15346, -0.26756, 0.06670, 0.26688), sunTheta, turbidity);
        Z.z = zenith_luminance(sunTheta, turbidity);
        Z.z *= 1000; // conversion from kcd/m^2 to cd/m^2
    
        // 3.2 Skylight Model: pre-divide zenith color by distribution denominator
        Z.x /= perez(0, sunTheta, A.x, B.x, C.x, D.x, E.x);
        Z.y /= perez(0, sunTheta, A.y, B.y, C.y, D.y, E.y);
        Z.z /= perez(0, sunTheta, A.z, B.z, C.z, D.z, E.z);
    
        // For low dynamic range simulation, normalize luminance to have a fixed value for sun
        if (normalizedSunY) Z.z = normalizedSunY / perez(sunTheta, 0, A.z, B.z, C.z, D.z, E.z);
    
        return { A, B, C, D, E, Z };
    }

} // end namespace detail

namespace polymer
{

    class gl_procedural_sky
    {

    protected:

        gl_mesh skyMesh;
        virtual void render_internal(const float4x4 & viewProjection, const float3 & sunDir, const float4x4 & modelToWorld) {};

    public:
    
        float2 sunPosition;
        float normalizedSunY = 1.15f;
        float albedo = 0.1f;
        float turbidity = 4.f;

        std::function<void()> onParametersChanged;

        gl_procedural_sky()
        {
            skyMesh = make_sphere_mesh(1.0);
            set_sun_position(50, 110);
        }

        gl_procedural_sky & operator = (const gl_procedural_sky & other)
        {
            this->skyMesh = make_sphere_mesh(1.0);
            this->sunPosition = other.sunPosition;
            this->normalizedSunY = other.normalizedSunY;
            this->albedo = other.albedo;
            this->turbidity = other.turbidity;
            set_sun_position(sunPosition.x, sunPosition.y);
        }

        void render(const float4x4 & viewProj, const float3 & eyepoint, const float farClip)
        {
            GLboolean blendEnabled;
            glGetBooleanv(GL_BLEND, &blendEnabled);
        
            GLboolean cullFaceEnabled;
            glGetBooleanv(GL_CULL_FACE, &cullFaceEnabled);
        
            glDisable(GL_BLEND);
            glDisable(GL_CULL_FACE);
        
            // Largest non-clipped sphere
            float4x4 world = (make_translation_matrix(eyepoint) * make_scaling_matrix(farClip * .99));
        
            render_internal(viewProj, get_sun_direction(), world);
        
            if (blendEnabled) glEnable(GL_BLEND);
            if (cullFaceEnabled) glEnable(GL_CULL_FACE);
        }
    
        // Set in degrees. Theta = 0 - 90, Phi = 0 - 360
        void set_sun_position(float theta, float phi)
        {
            sunPosition = {to_radians(theta), to_radians(phi)};
        }
    
        // Get in degrees
        float2 get_sun_position() const 
        { 
            return float2(to_degrees(sunPosition.x), to_degrees(sunPosition.y)); 
        }

        float3 get_sun_direction() const
        {
            // phi, theta
            return qrot(make_rotation_quat_axis_angle(float3(0,1,0), sunPosition.y), 
                   qrot(make_rotation_quat_axis_angle(float3(-1,0,0), sunPosition.x), float3(0,0,1)));
        }

        virtual void recompute(float turbidity, float albedo, float normalizedSunY) {};
    };

    class gl_hosek_sky : public gl_procedural_sky
    {
        shader_handle sky { "sky-hosek" };

        ::detail::HosekSkyRadianceData data;
    
        virtual void render_internal(const float4x4 & viewProjection, const float3 & sunDir, const float4x4 & modelToWorld) override
        {
            gl_shader & shader = sky.get()->get();

            shader.bind();
            shader.uniform("ViewProjection", viewProjection);
            shader.uniform("World", modelToWorld);
            shader.uniform("A", data.A);
            shader.uniform("B", data.B);
            shader.uniform("C", data.C);
            shader.uniform("D", data.D);
            shader.uniform("E", data.E);
            shader.uniform("F", data.F);
            shader.uniform("G", data.G);
            shader.uniform("H", data.H);
            shader.uniform("I", data.I);
            shader.uniform("Z", data.Z);
            shader.uniform("SunDirection", sunDir);
            skyMesh.draw_elements();
            shader.unbind();
        }
    
    public:
    
        gl_hosek_sky() { recompute(turbidity, albedo, normalizedSunY); }
        gl_hosek_sky(const gl_hosek_sky & other) { recompute(turbidity, albedo, normalizedSunY); }
        gl_hosek_sky & operator = (const gl_hosek_sky & other) { recompute(turbidity, albedo, normalizedSunY); return *this; }

        virtual void recompute(float turbidity, float albedo, float normalizedSunY) override
        {
            data = ::detail::HosekSkyRadianceData::compute(get_sun_direction(), turbidity, albedo, normalizedSunY);
            if (onParametersChanged) onParametersChanged();
        }
    };

    template<class F> void visit_fields(gl_procedural_sky & o, F f)
    {
        f("sun_position_theta_phi", o.sunPosition, range_metadata<float>{ 0.f, (float)POLYMER_PI });
        f("normalized_sun_y", o.normalizedSunY, range_metadata<float>{ 0.f, (float)POLYMER_PI });
        f("albedo", o.albedo, range_metadata<float>{ 0.01f, 4.f});
        f("turbidity", o.turbidity, range_metadata<float>{ 1.f, 14.f });
        o.recompute(o.turbidity, o.albedo, o.normalizedSunY);
    }

    inline void to_json(json & j, const gl_procedural_sky & p) {
        j = json::object();
        visit_fields(const_cast<gl_procedural_sky&>(p), [&j](const char * name, auto & field, auto... metadata) 
        { 
            j.push_back({ name, field }); 
        });
    }
    
    inline void from_json(const json & archive, gl_procedural_sky & m) {
        visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) {
            field = archive.at(name).get<std::remove_reference_t<decltype(field)>>();
        });
    };

    class gl_preetham_sky : public gl_procedural_sky
    {
        shader_handle sky { "sky-preetham" };
        ::detail::PreethamSkyRadianceData data;
    
        virtual void render_internal(const float4x4 & viewProjection, const float3 & sunDir, const float4x4 & modelToWorld) override
        {
            gl_shader & shader = sky.get()->get();

            shader.bind();
            shader.uniform("ViewProjection", viewProjection);
            shader.uniform("World", modelToWorld);
            shader.uniform("A", data.A);
            shader.uniform("B", data.B);
            shader.uniform("C", data.C);
            shader.uniform("D", data.D);
            shader.uniform("E", data.E);
            shader.uniform("Z", data.Z);
            shader.uniform("SunDirection", sunDir);
            skyMesh.draw_elements();
            shader.unbind();
        }
    
    public:
    
        gl_preetham_sky() { recompute(turbidity, albedo, normalizedSunY); }
        gl_preetham_sky(const gl_preetham_sky & other) { recompute(turbidity, albedo, normalizedSunY); }
        gl_preetham_sky & operator = (const gl_preetham_sky & other) { recompute(turbidity, albedo, normalizedSunY); return *this; }

        virtual void recompute(float turbidity, float albedo, float normalizedSunY) override
        {
            data = ::detail::PreethamSkyRadianceData::compute(get_sun_direction(), turbidity, albedo, normalizedSunY);
            if (onParametersChanged) onParametersChanged();
        }
    
    };

} // end namespace polymer

#pragma warning(pop)

#endif // end procedural_sky_h