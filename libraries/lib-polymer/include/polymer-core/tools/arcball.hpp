#pragma once

#ifndef polymer_arcball_controller_hpp
#define polymer_arcball_controller_hpp

#include "polymer-core/math/math-core.hpp"
#include "polymer-core/util/util.hpp"
#include "polymer-core/tools/geometry.hpp"

namespace polymer
{

	// Force sphere point to be perpendicular to axis
	inline float3 constrain_to_axis(const float3 & loose, const float3 & axis)
	{
		float norm;
		float3 onPlane = loose - axis * dot(axis, loose);
		norm = length2(onPlane);

		if (norm > 0.0f)
		{
			if (onPlane.z < 0.0f) onPlane = -onPlane;
			return (onPlane * (1.0f / std::sqrt(norm)));
		}

		if (std::abs(dot(axis, float3(0, 0, 1))) > 0.9999f) onPlane = float3(1, 0, 0);
		else onPlane = safe_normalize(float3(-axis.y, axis.x, 0));

		return onPlane;
	}

	class arcball_controller
	{
		float2 windowSize;
		float2 initialMousePos;

        float3 mouse_on_sphere(const float2 & mouse)
        {
            float3 result = { 0, 0, 0 };
            result.x = (mouse.x - (0.5f * windowSize.x)) / (0.5f * windowSize.x);
            result.y = -(mouse.y - (0.5f * windowSize.y)) / (0.5f * windowSize.y);

            float mag = length2(result);

            if (mag > 1.0f) result = safe_normalize(result);
            else
            {
                result.z = std::sqrt(1.f - mag);
                result = safe_normalize(result);
            }

            return result;
        }

    public:

        quatf currentQuat;
        float3 constraintAxis = { 0, 0, 0 };

        arcball_controller(float2 windowSize) : windowSize(windowSize) { currentQuat = linalg::identity; }

		void mouse_down(const float2 & mousePos)
		{
			initialMousePos = mousePos;
		}

		void mouse_drag(const float2 & mousePos)
		{
			float3 a = mouse_on_sphere(initialMousePos);
			float3 b = mouse_on_sphere(mousePos);

			if (length(constraintAxis) > 0.0f)
			{
				a = constrain_to_axis(a, constraintAxis);
				b = constrain_to_axis(b, constraintAxis);
			}

			if (distance(a, b) <= 0.0005f) return;

			currentQuat = safe_normalize(make_rotation_quat_between_vectors(a, b));
			initialMousePos = mousePos;
		}

	};

}

#endif // end polymer_arcball_controller_hpp
