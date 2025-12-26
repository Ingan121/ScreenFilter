struct VS_INPUT
{
    float2 pos : POSITION;
    float2 tex : TEXCOORD;
};

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD;
};

Texture2D backBufferTex : register(t0);
SamplerState smp : register(s0);

Texture2D noiseTex : register(t2);
SamplerState noiseSmp : register(s1);

int colors : register(b0); // enum { TrueColor = 0, 16bit, 15bit, 256, 64, 20, 16, 8, 4, 2 = 9 }
int flags : register(b0); // bit0: dither, bit1: invert, bit2: no palette use (colors <= 20), bit3: hdr?
int palette : register(b0); // 0: default, 1: grayscale, 2: alt pal 1 (16 - EGA, 8 - UltraVNC dark 8), 3: alt pal 2 (16: VMware/86Box VGA output)

//float3 palette8[8] = { float3(0,0,0), float3(1,0,0), float3(0,1,0), float3(0,0,1), float3(1,1,0), float3(1,0,1), float3(0,1,1), float3(1,1,1) };

float bayer4x4(int x, int y)
{
    // 4x4 Bayer matrix normalized 0..1
    int idx = (y & 3) * 4 + (x & 3);
    // Values from 0..15
    int m[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };
    return (m[idx] + 0.5) / 16.0;
}

float3 QuantizeLevels(float3 c, int levels)
{
    if (levels <= 1)
        return float3(0, 0, 0);
    float denom = (float) (levels - 1);
    c = floor(c * denom + 0.5) / denom;
    return c;
}

float3 NearestPalette8(float3 col, float3 pal[8])
{
    float bestDist = 1e9;
    float3 best = pal[0];
    for (int i = 0; i < 8; ++i)
    {
        float3 p = pal[i];
        float dx = col.x - p.x;
        float dy = col.y - p.y;
        float dz = col.z - p.z;
        float d = dx * dx + dy * dy + dz * dz;
        if (d < bestDist)
        {
            bestDist = d;
            best = p;
        }
    }
    return best;
}

float3 NearestPalette16(float3 col, float3 pal[16])
{
    float bestDist = 1e9;
    float3 best = pal[0];
    for (int i = 0; i < 16; ++i)
    {
        float3 p = pal[i];
        float dx = col.x - p.x;
        float dy = col.y - p.y;
        float dz = col.z - p.z;
        float d = dx * dx + dy * dy + dz * dz;
        if (d < bestDist)
        {
            bestDist = d;
            best = p;
        }
    }
    return best;
}

float3 NearestPalette20(float3 col, float3 pal[20])
{
    float bestDist = 1e9;
    float3 best = pal[0];
    for (int i = 0; i < 20; ++i)
    {
        float3 p = pal[i];
        float dx = col.x - p.x;
        float dy = col.y - p.y;
        float dz = col.z - p.z;
        float d = dx * dx + dy * dy + dz * dz;
        if (d < bestDist)
        {
            bestDist = d;
            best = p;
        }
    }
    return best;
}

float lum(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output;
    output.pos = float4(input.pos, 0, 1);
    output.tex = input.tex;
    return output;
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    float3 col = backBufferTex.Sample(smp, input.tex).rgb;
	
    bool dither = (flags & 1) != 0;
    float t = 0.5;
    if (dither)
    {
        int ix = (int) input.pos.x;
        int iy = (int) input.pos.y;
        t = bayer4x4(ix, iy);
    }
	
    bool nopal = (flags & 4) != 0;
	
    bool gray = (palette == 1 || colors >= 8);
    if (gray)
    {
        float l = lum(col);
        col = float3(l, l, l);
    }
	
    bool invert = (flags & 2) != 0;
    if (invert)
    {
        col = 1.0 - col;
    }
	
    switch (colors)
    {
        case 0: // true color
            return float4(col, 1);
        case 1: // 16 bit color (5-6-5)
	{
                float r = floor(col.r * 31.0 + 0.5) / 31.0;
                float g = floor(col.g * 63.0 + 0.5) / 63.0;
                float b = floor(col.b * 31.0 + 0.5) / 31.0;
                return float4(r, g, b, 1);
            }
        case 2: // 15 bit color (5-5-5)
	{
                float r = floor(col.r * 31.0 + 0.5) / 31.0;
                float g = floor(col.g * 31.0 + 0.5) / 31.0;
                float b = floor(col.b * 31.0 + 0.5) / 31.0;
                return float4(r, g, b, 1);
            }
        case 3: // 256 colors
	{
                if (dither)
                {
                    float r = floor(col.r * 8.0 + t) / 7.0;
                    float g = floor(col.g * 8.0 + t) / 7.0;
                    float b = floor(col.b * 4.0 + t) / 3.0;
                    return float4(r, g, b, 1);
                }
                else
                {
                    float r = floor(col.r * 7.0 + 0.5) / 7.0;
                    float g = floor(col.g * 7.0 + 0.5) / 7.0;
                    float b = floor(col.b * 3.0 + 0.5) / 3.0;
                    return float4(r, g, b, 1);
                }
            }
        default:
            return float4(col, 1);
    }
}