#version 150 core
// IS_GUARDRAIL=1 - use guardrail (render_gr_ps) logic instead of road (render_ps)

in vec3 vUv;
in vec4 vWp;
in vec3 vN;
in vec4 vC;

#if !IS_GUARDRAIL
uniform vec3 ambient;
uniform vec4 matDif;
#endif

out vec4 fragColor;

void main()
{
#if IS_GUARDRAIL
    float bridge = vC.w;
    fragColor = vec4(bridge * vec3(1.0, 1.0, 1.0), bridge);
#else
    float bridge = vC.x;
    float pipe   = vC.y;
    float norm   = abs(vN.y);

    float pwr = mix(mix(8.0, 8.0, bridge), 4.0, pipe);
    float ter = mix(mix(1.0, 0.0, bridge), 0.0, pipe);
    float diffuse = 1.0 - mix(1.0 - mix(pow(norm, pwr), pow(norm, pwr), pipe),
                               pow(1.0 - 2.0 * acos(norm) / 3.141592654, pwr), ter);
    vec3 clrLi = ambient + diffuse * matDif.rgb;

    vec3 clr = mix(mix(
        vec3(1.0, 1.0, 1.0) * clrLi,
        vec3(0.0, 0.8, 1.0) * (0.4 + 0.7 * clrLi), bridge),
        vec3(1.0, 0.8, 0.0) * (0.2 + 1.0 * clrLi), pipe);
    fragColor = vec4(clr, 1.0);
#endif
}
