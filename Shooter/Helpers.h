#pragma once

using namespace DirectX::SimpleMath;

namespace Helpers {
	float Lerp(float start, float end, float current, float increment);
	Vector3 LerpVector3(Vector3 start, Vector3 end, Vector3 current, float increment);
}