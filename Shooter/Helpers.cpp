#include "pch.h"
#include "Helpers.h"

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