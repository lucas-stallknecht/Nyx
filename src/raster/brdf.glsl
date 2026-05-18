#ifndef PI
#define PI 3.14159
#endif

float luma(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

float distribution_ggx(float n_dot_h, float alpha) {
    float a2 = alpha * alpha;
    float ndh2 = n_dot_h * n_dot_h;
    float denom = (ndh2 * (a2 - 1.0)) + 1.0;

    return a2 / (PI * denom * denom);
}

float geometry_shlick_ggx(float n_dot_w, float k) {
    return n_dot_w / (n_dot_w * (1.0 - k) + k);
}

float geometry_smith(vec3 n, vec3 l, vec3 v, float k) {
    float n_dot_l = max(dot(n, l), 0.0);
    float n_dot_v = max(dot(n, v), 0.0);

    return geometry_shlick_ggx(n_dot_l, k) * geometry_shlick_ggx(n_dot_v, k);
}

vec3 fresnel_schlick(float cos_theta, vec3 f0) {
    return f0 + (1.0 - f0) * pow(1.0 - cos_theta, 5.0);
}
