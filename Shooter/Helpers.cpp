#include "pch.h"
#include "Helpers.h"
using namespace DirectX::SimpleMath;

float Helpers::Lerp(float start, float end, float current, float increment) {
    if (start < end) {
        if (current < end) current += increment;
        if (current > end) current = end;
    }
    else {
        if (current > end) current -= increment;
        if (current < end) current = end;
    }

    return current;
}

Vector3 Helpers::LerpVector3(Vector3 start, Vector3 end, Vector3 current, float increment) {
    return Vector3(Lerp(start.x, end.x, current.x, increment), Lerp(start.y, end.y, current.y, increment), Lerp(start.z, end.z, current.z, increment));
}