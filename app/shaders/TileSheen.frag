#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf
{
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 startColour;
    vec4 endColour;
    vec2 itemSize;
    float cornerRadius;
};

float Hash(vec2 Point)
{
    return fract(sin(dot(Point, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
    vec2 Point = qt_TexCoord0 * itemSize;
    float Progress = clamp(dot(Point, itemSize) / dot(itemSize, itemSize), 0.0, 1.0);
    vec4 Colour = mix(startColour, endColour, Progress);

    float Noise = Hash(gl_FragCoord.xy) + Hash(gl_FragCoord.yx + vec2(17.0, 31.0)) - 1.0;
    Colour.rgb += Noise / 255.0;

    vec2 HalfSize = itemSize * 0.5;
    vec2 Inset = abs(Point - HalfSize) - (HalfSize - vec2(cornerRadius));
    float Distance = length(max(Inset, 0.0)) + min(max(Inset.x, Inset.y), 0.0) - cornerRadius;
    float Coverage = clamp(0.5 - Distance / max(fwidth(Distance), 0.0001), 0.0, 1.0);

    fragColor = Colour * Coverage * qt_Opacity;
}
