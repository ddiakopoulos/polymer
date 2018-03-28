#ifndef arcball_h
#define arcball_h

#include "math-core.hpp"
#include "util.hpp"
#include "geometry.hpp"

namespace avl
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

		if (dot(axis, float3(0, 0, 1)) < 0.0001f) onPlane = float3(1, 0, 0);
		else onPlane = safe_normalize(float3(-axis.y, axis.x, 0));

		return onPlane;
	}

	struct ArcballCamera
	{
		float2 windowSize;
		float2 initialMousePos;
		float4 initialQuat, currentQuat;
		float3 constraintAxis = { 0, 0, 0 };

		ArcballCamera(float2 windowSize) : windowSize(windowSize) { initialQuat = currentQuat = float4(0, 0, 0, 1); }

		void mouse_down(const float2 & mousePos)
		{
			initialMousePos = mousePos;
			initialQuat = float4(0, 0, 0, 1);
		}

		void mouse_drag(const float2 & mousePos)
		{
			auto a = mouse_on_sphere(initialMousePos);
			auto b = mouse_on_sphere(mousePos);

			if (length(constraintAxis))
			{
				a = constrain_to_axis(a, constraintAxis);
				b = constrain_to_axis(b, constraintAxis);
			}

			if (distance(a, b) <= 0.0003) return;
			
			auto rotation = normalize(make_rotation_quat_between_vectors(a, b));
			auto deltaRotation = normalize(qmul(rotation, qconj(initialQuat)));
			currentQuat = deltaRotation;
			initialMousePos = mousePos; // delta rotation
		}

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

	};

}

#endif // end arcball_h
