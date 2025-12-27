// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

#include <io.h>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#pragma comment (lib, "d3d11.lib") // Maybe un-useful
#pragma comment (lib, "d3dcompiler.lib")
#pragma comment (lib, "dxgi.lib") // Maybe un-useful
#pragma comment (lib, "uuid.lib") // Maybe un-useful
#pragma comment (lib, "dxguid.lib")


#pragma intrinsic(_ReturnAddress)

#define DITHER_GAMMA 2.2
#define SCRNFLTR_REGISTRY_KEY L"Software\\Ingan121\\ScreenFilterDWM"

#define RELEASE_IF_NOT_NULL(x) { if (x != NULL) { x->Release(); } }
#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)
#define RESIZE(x, y) realloc(x, (y) * sizeof(*x));
#define LOG_FILE_PATH R"(C:\DWMLOG\dwm.log)"
#define MAX_LOG_FILE_SIZE 20 * 1024 * 1024
#ifdef _DEBUG
#define DEBUG_MODE true
#else
#define DEBUG_MODE false
#endif

#if DEBUG_MODE == true
#define __LOG_ONLY_ONCE(x, y) if (static bool first_log_##y = true) { log_to_file(x); first_log_##y = false; }
#define _LOG_ONLY_ONCE(x, y) __LOG_ONLY_ONCE(x, y)
#define LOG_ONLY_ONCE(x) _LOG_ONLY_ONCE(x, __COUNTER__)
#define MESSAGE_BOX_DBG(x, y) MessageBoxA(NULL, x, "DEBUG HOOK DWM", y);

#define EXECUTE_WITH_LOG(winapi_func_hr) \
	do { \
		HRESULT hr = (winapi_func_hr); \
		if (FAILED(hr)) \
		{ \
			std::stringstream ss; \
			ss << "ERROR AT LINE: " << __LINE__ << " HR: " << hr << " - DETAILS: "; \
			LPSTR error_message = nullptr; \
			FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, \
				NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&error_message, 0, NULL); \
			ss << error_message; \
			log_to_file(ss.str().c_str()); \
			LocalFree(error_message); \
			throw std::exception(ss.str().c_str()); \
		} \
	} while (false);

#define EXECUTE_D3DCOMPILE_WITH_LOG(winapi_func_hr, error_interface) \
	do { \
		HRESULT hr = (winapi_func_hr); \
		if (FAILED(hr)) \
		{ \
			std::stringstream ss; \
			ss << "ERROR AT LINE: " << __LINE__ << " HR: " << hr << " - DETAILS: "; \
			LPSTR error_message = nullptr; \
			FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, \
				NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&error_message, 0, NULL); \
			ss << error_message << " - DX COMPILE ERROR: " << (char*)error_interface->GetBufferPointer(); \
			error_interface->Release(); \
			log_to_file(ss.str().c_str()); \
			LocalFree(error_message); \
			throw std::exception(ss.str().c_str()); \
		} \
	} while (false);

#define LOG_ADDRESS(prefix_message, address) \
	{ \
		std::stringstream ss; \
		ss << prefix_message << " 0x" << std::setw(sizeof(address) * 2) << std::setfill('0') << std::hex << (UINT_PTR)address; \
		log_to_file(ss.str().c_str()); \
	}

#else
#define LOG_ONLY_ONCE(x) // NOP, not in debug mode
#define MESSAGE_BOX_DBG(x, y) // NOP, not in debug mode
#define EXECUTE_WITH_LOG(winapi_func_hr) winapi_func_hr;
#define EXECUTE_D3DCOMPILE_WITH_LOG(winapi_func_hr, error_interface) winapi_func_hr;
#define LOG_ADDRESS(prefix_message, address) // NOP, not in debug mode
#endif


#if DEBUG_MODE == true
void print_error(const char* prefix_message)
{
	DWORD errorCode = GetLastError();
	LPSTR errorMessage = nullptr;
	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	               nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&errorMessage, 0, nullptr);

	char message_buf[100];
	sprintf(message_buf, "%s: %s - error code: %u", prefix_message, errorMessage, errorCode);
	MESSAGE_BOX_DBG(message_buf, MB_OK | MB_ICONWARNING)
	return;
}

void log_to_file(const char* log_buf)
{
	FILE* pFile = fopen(LOG_FILE_PATH, "a");
	if (pFile == NULL)
	{
		// print_error("Error during logging"); // Comment out to prevent UI freeze when used inside hooked functions
		return;
	}
	fseek(pFile, 0, SEEK_END);
	long size = ftell(pFile);
	if (size > MAX_LOG_FILE_SIZE)
	{
		if (_chsize(_fileno(pFile), 0) == -1)
		{
			fclose(pFile);
			return;
		}
	}
	fseek(pFile, 0, SEEK_END);
	fprintf(pFile, "%s\n", log_buf);
	fclose(pFile);
}
#endif

const unsigned char COverlayContext_Present_bytes[] = {
	0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48, 0x83, 0xec, 0x40, 0x48, 0x8b, 0xb1, 0x20,
	0x2c, 0x00, 0x00, 0x45, 0x8b, 0xd0, 0x48, 0x8b, 0xfa, 0x48, 0x8b, 0xd9, 0x48, 0x85, 0xf6, 0x0f, 0x85
};
const int IOverlaySwapChain_IDXGISwapChain_offset = -0x118;

const unsigned char COverlayContext_IsCandidateDirectFlipCompatbile_bytes[] = {
	0x48, 0x89, 0x7c, 0x24, 0x20, 0x55, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8b, 0xec, 0x48, 0x83,
	0xec, 0x40
};
const unsigned char COverlayContext_OverlaysEnabled_bytes[] = {
	0x75, 0x04, 0x32, 0xc0, 0xc3, 0xcc, 0x83, 0x79, 0x30, 0x01, 0x0f, 0x97, 0xc0, 0xc3
};

const int COverlayContext_DeviceClipBox_offset = -0x120;

const int IOverlaySwapChain_HardwareProtected_offset = -0xbc;

/*
 * AOB for function: COverlayContext_Present_bytes_w11
 *
 * 40 53 55 56 57 41 56 41 57 48 81 EC 88 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 44 24 78 48
 *
 */
const unsigned char COverlayContext_Present_bytes_w11[] = {
	0x40, 0x53, 0x55, 0x56, 0x57, 0x41, 0x56, 0x41, 0x57, 0x48, 0x81, 0xEC, 0x88, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x05,
	'?', '?', '?', '?', 0x48, 0x33, 0xC4, 0x48, 0x89, 0x44, 0x24, 0x78, 0x48
};
const int IOverlaySwapChain_IDXGISwapChain_offset_w11 = 0xE0;

/*
 * AOB for function: COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11
 *
 * 40 55 53 56 57 41 54 41 55 41 56 41 57 48 8B EC 48 83 EC 68 48
 */
const unsigned char COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11[] = {
	0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8B, 0xEC, 0x48, 0x83, 0xEC,
	0x68, 0x48,
};

/*
 * AOB for function: COverlayContext_OverlaysEnabled_bytes_w11
 *
 * 83 3D ?? ?? ?? ?? ?? 75 04
 */
const unsigned char COverlayContext_OverlaysEnabled_bytes_w11[] = {
	0x83, 0x3D, '?', '?', '?', '?', '?', 0x75, 0x04
};

int COverlayContext_DeviceClipBox_offset_w11 = 0x466C;

const int IOverlaySwapChain_HardwareProtected_offset_w11 = -0x144;

bool isWindows11;

bool aob_match_inverse(const void* buf1, const void* mask, const int buf_len)
{
	for (int i = 0; i < buf_len; ++i)
	{
		if (((unsigned char*)buf1)[i] != ((unsigned char*)mask)[i] && ((unsigned char*)mask)[i] != '?')
		{
			return true;
		}
	}
	return false;
}

DWORD g_colors = 5; // 256 colors
DWORD g_flags = 0;  // no dither, no invert, use palette on low color modes
DWORD g_palette = 0; // default palette
DWORD g_monocolor = 0xFFFFFF; // white

char shaders[] = R"(
    struct VS_INPUT {
	float2 pos : POSITION;
	float2 tex : TEXCOORD;
};

struct VS_OUTPUT {
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD;
};

Texture2D backBufferTex : register(t0);
SamplerState smp : register(s0);

Texture2D noiseTex : register(t2);
SamplerState noiseSmp : register(s1);

int colors : register(b0); // enum { TrueColor = 0, 16bit, 15bit, 12bit, 10bit, 256, 64, 20, 16, 8, 4, 2 = 11 }
int flags : register(b0); // bit0: dither, bit1: invert, bit2: no palette use (colors <= 20), bit3: hdr?
int palette : register(b0); // 0: default, 1: grayscale, 2: alt pal 1 (16 - EGA, 8 - UltraVNC dark 8), 3: alt pal 2 (16: VMware/86Box VGA output), 4: grayscale (Rec. 601)
int monocolor : register(b0); // color to use in monochrome mode

float bayer4x4(int x, int y) {
    // 4x4 Bayer matrix normalized 0..1
    int idx = (y & 3) * 4 + (x & 3);
    // Values from 0..15
    int m[16] = { 0, 8, 2,10, 12,4,14,6, 3,11,1,9, 15,7,13,5 };
    return (m[idx] + 0.5) / 16.0;
}

float3 NearestPalette8(float3 col, float3 pal[8]) {
	float bestDist = 1e9;
	float3 best = pal[0];
	for (int i = 0; i < 8; ++i) {
		float3 p = pal[i];
		float dx = col.x - p.x; float dy = col.y - p.y; float dz = col.z - p.z;
		float d = dx*dx + dy*dy + dz*dz;
		if (d < bestDist) { bestDist = d; best = p; }
	}
	return best;
}

float3 NearestPalette16(float3 col, float3 pal[16]) {
    float bestDist = 1e9;
    float3 best = pal[0];
    for (int i = 0; i < 16; ++i) {
        float3 p = pal[i];
        float dx = col.x - p.x; float dy = col.y - p.y; float dz = col.z - p.z;
        float d = dx*dx + dy*dy + dz*dz;
        if (d < bestDist) { bestDist = d; best = p; }
    }
    return best;
}

float3 NearestPalette16AltOutput(float3 col, float3 pal[16], float3 pal_alt[16]) {
	float bestDist = 1e9;
	int bestIdx = 0;
	for (int i = 0; i < 16; ++i) {
		float3 p = pal[i];
		float dx = col.x - p.x; float dy = col.y - p.y; float dz = col.z - p.z;
		float d = dx*dx + dy*dy + dz*dz;
		if (d < bestDist) { bestDist = d; bestIdx = i; }
	}
	return pal_alt[bestIdx];
}

float3 NearestPalette20(float3 col, float3 pal[20]) {
	float bestDist = 1e9;
	float3 best = pal[0];
	for (int i = 0; i < 20; ++i) {
		float3 p = pal[i];
		float dx = col.x - p.x; float dy = col.y - p.y; float dz = col.z - p.z;
		float d = dx*dx + dy*dy + dz*dz;
		if (d < bestDist) { bestDist = d; best = p; }
	}
	return best;
}

float lum(float3 c) { return dot(c, float3(0.2126, 0.7152, 0.0722)); } // Rec. 709 (sRGB, HDTV)
float lum601(float3 c) { return dot(c, float3(0.299, 0.587, 0.114)); } // Rec. 601 (SDTV, ScreenFilter 1.x GDI, Windows accessibility color filters)

VS_OUTPUT VS(VS_INPUT input) {
	VS_OUTPUT output;
	output.pos = float4(input.pos, 0, 1);
	output.tex = input.tex;
	return output;
}

float4 PS(VS_OUTPUT input) : SV_TARGET{
	float3 col = backBufferTex.Sample(smp, input.tex).rgb;
	
	bool dither = (flags & 1) != 0;
	float t = 0.5;
	if (dither) {
		int ix = (int)input.pos.x;
		int iy = (int)input.pos.y;
		t = bayer4x4(ix, iy);
	}
	
	bool nopal = (flags & 4) != 0;
	
	bool gray = (palette == 1 || palette == 4 || colors >= 10);
	if (gray) {
		if (palette == 4) {
			float l = lum601(col);
			col = float3(l, l, l);
		} else {
			float l = lum(col);
			col = float3(l, l, l);
		}
	}
	
	bool invert = (flags & 2) != 0;
	if (invert) {
		col = 1.0 - col;
	}
	
	switch (colors) {
	case 0: // true color
		return float4(col, 1);
	case 1: // 16 bit color (5-6-5)
	{
		float r = floor(col.r * 31.0 + t) / 31.0;
		float g = floor(col.g * 63.0 + t) / 63.0;
		float b = floor(col.b * 31.0 + t) / 31.0;
		return float4(r, g, b, 1);
	}
	case 2: // 15 bit color (5-5-5)
	{
		float r = floor(col.r * 31.0 + t) / 31.0;
		float g = floor(col.g * 31.0 + t) / 31.0;
		float b = floor(col.b * 31.0 + t) / 31.0;
		return float4(r, g, b, 1);
	}
	case 3: // 12 bit color (4-4-4)
	{
		float r = floor(col.r * 15.0 + t) / 15.0;
		float g = floor(col.g * 15.0 + t) / 15.0;
		float b = floor(col.b * 15.0 + t) / 15.0;
		return float4(r, g, b, 1);
	}
	case 4: // 10 bit color (3-4-3)
	{
		float r = floor(col.r * 7.0 + t) / 7.0;
		float g = floor(col.g * 15.0 + t) / 15.0;
		float b = floor(col.b * 7.0 + t) / 7.0;
		return float4(r, g, b, 1);
	}
	case 5: // 256 colors
	{
		if (gray) {
			return float4(col, 1);
		}
		float r = floor(col.r * 7.0 + t) / 7.0;
		float g = floor(col.g * 7.0 + t) / 7.0;
		float b = floor(col.b * 3.0 + t) / 3.0;
		return float4(r, g, b, 1);
	}
	case 6: // 64 colors
	{
		if (gray) {
			float l = floor(col.x * 63.0 + t) / 63.0;
			return float4(l, l, l, 1);
		}
		col = floor(col * 3.0 + t) / 3.0;
		return float4(col, 1);
	}
	case 7: // 20 colors
	{
		if (gray) {
			float l = floor(col.x * 19.0 + t) / 19.0;
			return float4(l, l, l, 1);
		}
		if (dither) {
			col = floor(col * 3.0 + t) / 3.0;
		}
		if (nopal) {
			// 20-color quantization
			// Web-safe palette: R,G,B in {0, 128, 255}
			float r = col.r;
			float g = col.g;
			float b = col.b;
			if (r < 0.25) r = 0;
			else if (r < 0.75) r = 0.5;
			else r = 1.0;
			if (g < 0.25) g = 0;
			else if (g < 0.75) g = 0.5;
			else g = 1.0;
			if (b < 0.25) b = 0;
			else if (b < 0.75) b = 0.5;
			else b = 1.0;
			return float4(r, g, b, 1);
		}
		float3 pal_win[20] = {
			float3(0,0,0), // black
			float3(0.50196,0,0), // dark red
			float3(0,0.50196,0), // dark green
			float3(0.50196,0.50196,0), // dark yellow
			float3(0,0,0.50196), // dark blue
			float3(0.50196,0,0.50196), // dark magenta
			float3(0,0.50196,0.50196), // dark cyan
			float3(0.75294,0.75294,0.75294), // light gray
			float3(0.50196,0.50196,0.50196), // dark gray
			float3(1,0,0), // red
			float3(0,1,0), // green
			float3(1,1,0), // yellow
			float3(0,0,1), // blue
			float3(1,0,1), // magenta
			float3(0,1,1), // cyan
			float3(1,1,1), // white
			float3(0.75294,0.86275,0.75294), // light green
			float3(0.65098,0.79216,0.94118), // light blue
			float3(1,0.98039,0.94118), // light yellow
			float3(0.62745,0.62745,0.63922)  // light gray
		};
		float3 nc = NearestPalette20(col, pal_win);
		return float4(nc, 1);
	}
	case 8: // 16 colors
	{
		if (gray) {
			float l = floor(col.x * 15.0 + t) / 15.0;
			return float4(l, l, l, 1);
		}
		if (nopal) {
			float r = floor(col.r + t);
			float g = floor(col.g * 3.0 + t) / 3.0;
			float b = floor(col.b + t);
			return float4(r, g, b, 1);
		}
		if (dither) {
			col = floor(col * 3.0 + t) / 3.0;
		}
		float3 pal_vga[16] = {
			float3(0,0,0), // black
			float3(0.50196,0,0), // dark red
			float3(0,0.50196,0), // dark green
			float3(0.50196,0.50196,0), // dark yellow
			float3(0,0,0.50196), // dark blue
			float3(0.50196,0,0.50196), // dark magenta
			float3(0,0.50196,0.50196), // dark cyan
			float3(0.75294,0.75294,0.75294), // light gray
			float3(0.50196,0.50196,0.50196), // dark gray
			float3(1,0,0), // red
			float3(0,1,0), // green
			float3(1,1,0), // yellow
			float3(0,0,1), // blue
			float3(1,0,1), // magenta
			float3(0,1,1), // cyan
			float3(1,1,1)  // white
		};
		float3 pal_ega[16] = {
			float3(0,0,0), // black
			float3(0,0,0.66667), // blue
			float3(0,0.66667,0), // green
			float3(0,0.66667,0.66667), // cyan
			float3(0.66667,0,0), // red
			float3(0.66667,0,0.66667), // magenta
			float3(0.66667,0.33333,0), // brown
			float3(0.66667,0.66667,0.66667), // light gray
			float3(0.33333,0.33333,0.33333), // dark gray
			float3(0.33333,0.33333,1), // bright blue
			float3(0.33333,1,0.33333), // bright green
			float3(0.33333,1,1), // bright cyan
			float3(1,0.33333,0.33333), // bright red
			float3(1,0.33333,1), // bright magenta
			float3(1,1,0.33333), // yellow
			float3(1,1,1)  // white
		};
		float3 pal_vmware[16] = {
			float3(0,0,0), // black
			float3(0.66667,0,0.33333), // dark red
			float3(0,0.66667,0.33333), // dark green
			float3(0.66667,0.66667,0.33333), // dark yellow
			float3(0,0,0.66667), // dark blue
			float3(0.66667,0.33333,0.66667), // dark magenta
			float3(0.33333,0.66667,0.66667), // dark cyan
			float3(0.76471,0.78039,0.79608), // light gray
			float3(0.52949,0.54118,0.55686), // dark gray
			float3(1,0,0), // red
			float3(0,1,0), // green
			float3(1,1,0), // yellow
			float3(0,0,1), // blue
			float3(1,0,1), // magenta
			float3(0,1,1), // cyan
			float3(1,1,1)  // white
		};
		switch (palette) {
		case 2:
			return float4(NearestPalette16(col, pal_ega), 1);
		case 3:
			return float4(NearestPalette16AltOutput(col, pal_vga, pal_vmware), 1);
		}
		return float4(NearestPalette16(col, pal_vga), 1);
	}
	case 9: // 8 colors
	{
		if (gray) {
			float l = floor(col.x * 7.0 + t) / 7.0;
			return float4(l, l, l, 1);
		}
		if (nopal || dither || palette != 2) {
			col = floor(col + t);
			if (nopal || palette != 2) {
				return float4(col, 1);
			}
		}
		float3 pal_uvnc[8] = {
			float3(0,0,0), // black
			float3(1,1,1), // white
			float3(0.14118,0.14118,0.33333), // dark blue
			float3(0.28627,0.28627,0.33333), // dark cyan
			float3(0.42745,0.42745,0.33333), // dark green
			float3(0.57255,0.57255,0.66667), // light blue
			float3(0.71373,0.71373,0.66667), // light cyan
			float3(0.85882,0.85882,0.66667)  // light green
		};
		float3 nc = NearestPalette8(col, pal_uvnc);
		return float4(nc, 1);
	}
	case 10: // 4 colors
	{
		col = floor(col * 3.0 + t) / 3.0;
		return float4(col, 1);
	}
	case 11: // 2 colors
	{
		float c = step(t, col.x);
		if (c > 0.5) { // use monocolor instead of white
			int r = monocolor >> 16 & 0xFF;
			int g = monocolor >> 8 & 0xFF;
			int b = monocolor & 0xFF;
			// use white if too dark
			if (r + g + b < 72) {
				return float4(1, 1, 1, 1);
			}
			return float4(r / 255.0, g / 255.0, b / 255.0, 1);
		}
		return float4(c, c, c, 1);
	}
	default:
		return float4(col, 1);
	}
}
)";

ID3D11Device* device;
ID3D11DeviceContext* deviceContext;
ID3D11VertexShader* vertexShader;
ID3D11PixelShader* pixelShader;
ID3D11InputLayout* inputLayout;

ID3D11Buffer* vertexBuffer;
UINT numVerts;
UINT stride;
UINT offset;

D3D11_TEXTURE2D_DESC backBufferDesc;
D3D11_TEXTURE2D_DESC textureDesc[2];

ID3D11SamplerState* samplerState;
ID3D11Texture2D* texture[2];
ID3D11ShaderResourceView* textureView[2];

ID3D11SamplerState* noiseSamplerState;
ID3D11ShaderResourceView* noiseTextureView;

ID3D11Buffer* constantBuffer;

void DrawRectangle(struct tagRECT* rect, int index)
{
	float width = backBufferDesc.Width;
	float height = backBufferDesc.Height;

	float screenLeft = rect->left / width;
	float screenTop = rect->top / height;
	float screenRight = rect->right / width;
	float screenBottom = rect->bottom / height;

	float left = screenLeft * 2 - 1;
	float top = screenTop * -2 + 1;
	float right = screenRight * 2 - 1;
	float bottom = screenBottom * -2 + 1;

	width = textureDesc[index].Width;
	height = textureDesc[index].Height;
	float texLeft = rect->left / width;
	float texTop = rect->top / height;
	float texRight = rect->right / width;
	float texBottom = rect->bottom / height;

	float vertexData[] = {
		left, bottom, texLeft, texBottom,
		left, top, texLeft, texTop,
		right, bottom, texRight, texBottom,
		right, top, texRight, texTop
	};

	D3D11_MAPPED_SUBRESOURCE resource;
	EXECUTE_WITH_LOG(deviceContext->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource))
	memcpy(resource.pData, vertexData, stride * numVerts);
	deviceContext->Unmap(vertexBuffer, 0);

	deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

	deviceContext->Draw(numVerts, 0);
}

bool g_active = false;

bool IsLUTActive(void* target)
{
	return g_active;
}

void SetLUTActive(void* target)
{
	if (!IsLUTActive(target))
	{
		g_active = true;
	}
}

void UnsetLUTActive(void* target)
{
	g_active = false;
}

void InitializeStuff(IDXGISwapChain* swapChain)
{
	try
	{
		EXECUTE_WITH_LOG(swapChain->GetDevice(IID_ID3D11Device, (void**)&device))
		LOG_ADDRESS("Current swapchain address is: ", swapChain)
		LOG_ONLY_ONCE("Device successfully gathered")
		LOG_ADDRESS("The device address is: ", device)

		device->GetImmediateContext(&deviceContext);
		LOG_ONLY_ONCE("Got context after device")
		LOG_ADDRESS("The Device context is located at address: ", deviceContext)
		{
			ID3DBlob* vsBlob;
			ID3DBlob* compile_error_interface;
			LOG_ONLY_ONCE(("Trying to compile vshader with this code:\n" + std::string(shaders)).c_str())
			EXECUTE_D3DCOMPILE_WITH_LOG(
				D3DCompile(shaders, sizeof shaders, NULL, NULL, NULL, "VS", "vs_5_0", 0, 0, &vsBlob, &
					compile_error_interface), compile_error_interface)


			LOG_ONLY_ONCE("Vertex shader compiled successfully")
			EXECUTE_WITH_LOG(device->CreateVertexShader(vsBlob->GetBufferPointer(),
				vsBlob->GetBufferSize(), NULL, &vertexShader))


			LOG_ONLY_ONCE("Vertex shader created successfully")
			D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
			{
				{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{
					"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
					D3D11_INPUT_PER_VERTEX_DATA, 0
				}
			};
			EXECUTE_WITH_LOG(device->CreateInputLayout(inputElementDesc, ARRAYSIZE(inputElementDesc),
				vsBlob->GetBufferPointer(),
				vsBlob->GetBufferSize(), &inputLayout))

			vsBlob->Release();
		}
		{
			ID3DBlob* psBlob;
			ID3DBlob* compile_error_interface;
			EXECUTE_D3DCOMPILE_WITH_LOG(
				D3DCompile(shaders, sizeof shaders, NULL, NULL, NULL, "PS", "ps_5_0", 0, 0, &psBlob, &
					compile_error_interface), compile_error_interface)

			LOG_ONLY_ONCE("Pixel shader compiled successfully")
			device->CreatePixelShader(psBlob->GetBufferPointer(),
			                          psBlob->GetBufferSize(), NULL, &pixelShader);
			psBlob->Release();
		}
		{
			stride = 4 * sizeof(float);
			numVerts = 4;
			offset = 0;

			D3D11_BUFFER_DESC vertexBufferDesc = {};
			vertexBufferDesc.ByteWidth = stride * numVerts;
			vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
			vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			EXECUTE_WITH_LOG(device->CreateBuffer(&vertexBufferDesc, NULL, &vertexBuffer))
		}
		{
			D3D11_SAMPLER_DESC samplerDesc = {};
			samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

			EXECUTE_WITH_LOG(device->CreateSamplerState(&samplerDesc, &samplerState))
		}
		{
			D3D11_SAMPLER_DESC samplerDesc = {};
			samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
			samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

			EXECUTE_WITH_LOG(device->CreateSamplerState(&samplerDesc, &noiseSamplerState))
		}
		{
			D3D11_TEXTURE2D_DESC desc = {};
			desc.Width = NOISE_SIZE;
			desc.Height = NOISE_SIZE;
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.Format = DXGI_FORMAT_R32_FLOAT;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.Usage = D3D11_USAGE_IMMUTABLE;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

			float noise[NOISE_SIZE][NOISE_SIZE];

			for (int i = 0; i < NOISE_SIZE; i++)
			{
				for (int j = 0; j < NOISE_SIZE; j++)
				{
					noise[i][j] = (noiseBytes[i][j] + 0.5) / 256;
				}
			}

			D3D11_SUBRESOURCE_DATA initData;
			initData.pSysMem = noise;
			initData.SysMemPitch = sizeof(noise[0]);

			ID3D11Texture2D* tex;
			EXECUTE_WITH_LOG(device->CreateTexture2D(&desc, &initData, &tex))
			EXECUTE_WITH_LOG(device->CreateShaderResourceView((ID3D11Resource*)tex, NULL, &noiseTextureView))
			tex->Release();
		}
		{
			D3D11_BUFFER_DESC constantBufferDesc = {};
			constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			constantBufferDesc.ByteWidth = 16;
			constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
			constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			EXECUTE_WITH_LOG(device->CreateBuffer(&constantBufferDesc, NULL, &constantBuffer))
			LOG_ONLY_ONCE("Final buffer created in InitializeStuff")
		}
	}
	catch (std::exception& ex)
	{
		std::stringstream ex_message;
		ex_message << "Exception caught at line " << __LINE__ << ": " << ex.what() << std::endl;
		LOG_ONLY_ONCE(ex_message.str().c_str())
		throw;
	}
	catch (...)
	{
		std::stringstream ex_message;
		ex_message << "Exception caught at line " << __LINE__ << ": " << std::endl;
		LOG_ONLY_ONCE(ex_message.str().c_str())
		throw;
	}
}

void UninitializeStuff()
{
	RELEASE_IF_NOT_NULL(device)
	RELEASE_IF_NOT_NULL(deviceContext)
	RELEASE_IF_NOT_NULL(vertexShader)
	RELEASE_IF_NOT_NULL(pixelShader)
	RELEASE_IF_NOT_NULL(inputLayout)
	RELEASE_IF_NOT_NULL(vertexBuffer)
	RELEASE_IF_NOT_NULL(samplerState)
	for (int i = 0; i < 2; i++)
	{
		RELEASE_IF_NOT_NULL(texture[i])
		RELEASE_IF_NOT_NULL(textureView[i])
	}
	RELEASE_IF_NOT_NULL(noiseSamplerState)
	RELEASE_IF_NOT_NULL(noiseTextureView)
	RELEASE_IF_NOT_NULL(constantBuffer)
}

bool ApplyLUT(void* cOverlayContext, IDXGISwapChain* swapChain, struct tagRECT* rects, int numRects)
{
	try
	{
		if (!device)
		{
			LOG_ONLY_ONCE("Initializing stuff in ApplyLUT")
			InitializeStuff(swapChain);
		}
		LOG_ONLY_ONCE("Init done, continuing with LUT application")

		ID3D11Texture2D* backBuffer;
		ID3D11RenderTargetView* renderTargetView;


		EXECUTE_WITH_LOG(swapChain->GetBuffer(0, IID_ID3D11Texture2D, (void**)&backBuffer))

		D3D11_TEXTURE2D_DESC newBackBufferDesc;
		backBuffer->GetDesc(&newBackBufferDesc);

		int index = -1;
		if (newBackBufferDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM)
		{
			index = 0;
		}
		else if (newBackBufferDesc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT)
		{
			index = 1;
		}

		D3D11_TEXTURE2D_DESC oldTextureDesc = textureDesc[index];
		if (newBackBufferDesc.Width > oldTextureDesc.Width || newBackBufferDesc.Height > oldTextureDesc.Height)
		{
			if (texture[index] != NULL)
			{
				texture[index]->Release();
				textureView[index]->Release();
			}

			UINT newWidth = max(newBackBufferDesc.Width, oldTextureDesc.Width);
			UINT newHeight = max(newBackBufferDesc.Height, oldTextureDesc.Height);

			D3D11_TEXTURE2D_DESC newTextureDesc;

			newTextureDesc = newBackBufferDesc;
			newTextureDesc.Width = newWidth;
			newTextureDesc.Height = newHeight;
			newTextureDesc.Usage = D3D11_USAGE_DEFAULT;
			newTextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			newTextureDesc.CPUAccessFlags = 0;
			newTextureDesc.MiscFlags = 0;

			textureDesc[index] = newTextureDesc;

			EXECUTE_WITH_LOG(device->CreateTexture2D(&textureDesc[index], NULL, &texture[index]))
			EXECUTE_WITH_LOG(
				device->CreateShaderResourceView((ID3D11Resource*)texture[index], NULL, &textureView[index]))
		}

		backBufferDesc = newBackBufferDesc;

		EXECUTE_WITH_LOG(device->CreateRenderTargetView((ID3D11Resource*)backBuffer, NULL, &renderTargetView))
		const D3D11_VIEWPORT d3d11_viewport(0, 0, backBufferDesc.Width, backBufferDesc.Height, 0.0f, 1.0f);
		deviceContext->RSSetViewports(1, &d3d11_viewport);

		deviceContext->OMSetRenderTargets(1, &renderTargetView, NULL);
		renderTargetView->Release();

		deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		deviceContext->IASetInputLayout(inputLayout);

		deviceContext->VSSetShader(vertexShader, NULL, 0);
		deviceContext->PSSetShader(pixelShader, NULL, 0);

		deviceContext->PSSetShaderResources(0, 1, &textureView[index]);
		deviceContext->PSSetSamplers(0, 1, &samplerState);

		deviceContext->PSSetShaderResources(2, 1, &noiseTextureView);
		deviceContext->PSSetSamplers(1, 1, &noiseSamplerState);

		int constantData[4] = {
			g_colors,
			g_flags,
			g_palette,
			g_monocolor
		};

		D3D11_MAPPED_SUBRESOURCE resource;
		EXECUTE_WITH_LOG(deviceContext->Map((ID3D11Resource*)constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0,
			&resource))
		memcpy(resource.pData, constantData, sizeof(constantData));
		deviceContext->Unmap((ID3D11Resource*)constantBuffer, 0);

		deviceContext->PSSetConstantBuffers(0, 1, &constantBuffer);

		for (int i = 0; i < numRects; i++)
		{
			D3D11_BOX sourceRegion;
			sourceRegion.left = rects[i].left;
			sourceRegion.right = rects[i].right;
			sourceRegion.top = rects[i].top;
			sourceRegion.bottom = rects[i].bottom;
			sourceRegion.front = 0;
			sourceRegion.back = 1;

			deviceContext->CopySubresourceRegion((ID3D11Resource*)texture[index], 0, rects[i].left,
			                                     rects[i].top, 0, (ID3D11Resource*)backBuffer, 0, &sourceRegion);
			DrawRectangle(&rects[i], index);
		}

		backBuffer->Release();
		return true;
	}
	catch (std::exception& ex)
	{
		std::stringstream ex_message;
		ex_message << "Exception caught at line " << __LINE__ << ": " << ex.what() << std::endl;
		LOG_ONLY_ONCE(ex_message.str().c_str())
		return false;
	}
	catch (...)
	{
		std::stringstream ex_message;
		ex_message << "Exception caught at line " << __LINE__ << std::endl;
		LOG_ONLY_ONCE(ex_message.str().c_str())
		return false;
	}
}

typedef struct rectVec
{
	struct tagRECT* start;
	struct tagRECT* end;
	struct tagRECT* cap;
} rectVec;

typedef long (COverlayContext_Present_t)(void*, void*, unsigned int, rectVec*, unsigned int, bool);

COverlayContext_Present_t* COverlayContext_Present_orig;
COverlayContext_Present_t* COverlayContext_Present_real_orig;


long COverlayContext_Present_hook(void* self, void* overlaySwapChain, unsigned int a3, rectVec* rectVec,
                                  unsigned int a5, bool a6)
{
	if (_ReturnAddress() < (void*)COverlayContext_Present_real_orig)
	{
		LOG_ONLY_ONCE("I am inside COverlayContext::Present hook inside the main if condition")

		if (isWindows11 && *((bool*)overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset_w11) ||
			!isWindows11 && *((bool*)overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset))
		{
			std::stringstream hw_protection_message;
			hw_protection_message << "I'm inside the Hardware protection condition - 0x" << std::hex << (bool*)
				overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset_w11 << " - value: 0x" << *((bool*)
					overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset_w11);
			LOG_ONLY_ONCE(hw_protection_message.str().c_str())
			UnsetLUTActive(self);
		}
		else
		{
			std::stringstream hw_protection_message;
			hw_protection_message << "I'm outside the Hardware protection condition - 0x" << std::hex << (bool*)
				overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset_w11 << " - value: 0x" << *((bool*)
					overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset_w11);
			LOG_ONLY_ONCE(hw_protection_message.str().c_str())

			IDXGISwapChain* swapChain;
			if (isWindows11)
			{
				LOG_ONLY_ONCE("Gathering IDXGISwapChain pointer")
				int sub_from_legacy_swapchain = *(int*)((unsigned char*)overlaySwapChain - 4);
				void* real_overlay_swap_chain = (unsigned char*)overlaySwapChain - sub_from_legacy_swapchain -
					0x1b0;
				swapChain = *(IDXGISwapChain**)((unsigned char*)real_overlay_swap_chain +
					IOverlaySwapChain_IDXGISwapChain_offset_w11);
			}
			else
			{
				swapChain = *(IDXGISwapChain**)((unsigned char*)overlaySwapChain +
					IOverlaySwapChain_IDXGISwapChain_offset);
			}

			if (ApplyLUT(self, swapChain, rectVec->start, rectVec->end - rectVec->start))
			{
				LOG_ONLY_ONCE("Setting LUTactive")
				SetLUTActive(self);
			}
			else
			{
				LOG_ONLY_ONCE("Un-setting LUTactive")
				UnsetLUTActive(self);
			}
		}
	}

	return COverlayContext_Present_orig(self, overlaySwapChain, a3, rectVec, a5, a6);
}

typedef bool (COverlayContext_IsCandidateDirectFlipCompatbile_t)(void*, void*, void*, void*, int, unsigned int, bool,
                                                                 bool);

COverlayContext_IsCandidateDirectFlipCompatbile_t* COverlayContext_IsCandidateDirectFlipCompatbile_orig;

bool COverlayContext_IsCandidateDirectFlipCompatbile_hook(void* self, void* a2, void* a3, void* a4, int a5,
                                                          unsigned int a6, bool a7, bool a8)
{
	if (IsLUTActive(self))
	{
		return false;
	}
	return COverlayContext_IsCandidateDirectFlipCompatbile_orig(self, a2, a3, a4, a5, a6, a7, a8);
}

typedef bool (COverlayContext_OverlaysEnabled_t)(void*);

COverlayContext_OverlaysEnabled_t* COverlayContext_OverlaysEnabled_orig;

bool COverlayContext_OverlaysEnabled_hook(void* self)
{
	if (IsLUTActive(self))
	{
		LOG_ONLY_ONCE("LUT ACTIVE FALSE in overlaysEnabled")
		return false;
	}
	return COverlayContext_OverlaysEnabled_orig(self);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		{
			HMODULE dwmcore = GetModuleHandle(L"dwmcore.dll");
			MODULEINFO moduleInfo;
			GetModuleInformation(GetCurrentProcess(), dwmcore, &moduleInfo, sizeof moduleInfo);

			OSVERSIONINFOEX versionInfo;
			ZeroMemory(&versionInfo, sizeof OSVERSIONINFOEX);
			versionInfo.dwOSVersionInfoSize = sizeof OSVERSIONINFOEX;
			versionInfo.dwBuildNumber = 22000;

			ULONGLONG dwlConditionMask = 0;
			VER_SET_CONDITION(dwlConditionMask, VER_BUILDNUMBER, VER_GREATER_EQUAL);

			if (VerifyVersionInfo(&versionInfo, VER_BUILDNUMBER, dwlConditionMask))
			{
				isWindows11 = true;
			}
			else
			{
				isWindows11 = false;
			}

			// TODO: Remove this debug instruction
			MESSAGE_BOX_DBG("DWM LUT ATTACH", MB_OK)

			if (isWindows11)
			{
				// TODO: Remove this debug instruction
				MESSAGE_BOX_DBG("DETECTED WINDOWS 11 OS", MB_OK)

				for (size_t i = 0; i <= moduleInfo.SizeOfImage - sizeof COverlayContext_OverlaysEnabled_bytes_w11; i++)
				{
					unsigned char* address = (unsigned char*)dwmcore + i;
					if (!COverlayContext_Present_orig && sizeof COverlayContext_Present_bytes_w11 <= moduleInfo.
						SizeOfImage - i && !aob_match_inverse(address, COverlayContext_Present_bytes_w11,
						                                      sizeof COverlayContext_Present_bytes_w11))
					{
						// TODO: Remove this debug instruction
						MESSAGE_BOX_DBG("DETECTED COverlayContextPresent address", MB_OK)

						COverlayContext_Present_orig = (COverlayContext_Present_t*)address;
						COverlayContext_Present_real_orig = COverlayContext_Present_orig;
					}
					else if (!COverlayContext_IsCandidateDirectFlipCompatbile_orig && sizeof
						COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11 <= moduleInfo.SizeOfImage - i && !
						aob_match_inverse(
							address, COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11,
							sizeof COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11))
					{
						COverlayContext_IsCandidateDirectFlipCompatbile_orig = (
							COverlayContext_IsCandidateDirectFlipCompatbile_t*)address;
					}
					else if (!COverlayContext_OverlaysEnabled_orig && sizeof COverlayContext_OverlaysEnabled_bytes_w11
						<= moduleInfo.SizeOfImage - i && !aob_match_inverse(
							address, COverlayContext_OverlaysEnabled_bytes_w11,
							sizeof COverlayContext_OverlaysEnabled_bytes_w11))
					{
						COverlayContext_OverlaysEnabled_orig = (COverlayContext_OverlaysEnabled_t*)address;
					}
					if (COverlayContext_Present_orig && COverlayContext_IsCandidateDirectFlipCompatbile_orig &&
						COverlayContext_OverlaysEnabled_orig)
					{
						MESSAGE_BOX_DBG("All addresses successfully retrieved", MB_OK)

						break;
					}
				}

				DWORD rev;
				DWORD revSize = sizeof(rev);
				RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "UBR", RRF_RT_DWORD,
				             NULL, &rev, &revSize);

				if (rev >= 706)
				{
					MESSAGE_BOX_DBG("Detected recent Windows OS", MB_OK)

					// COverlayContext_DeviceClipBox_offset_w11 += 8;
				}
			}
			else
			{
				for (size_t i = 0; i <= moduleInfo.SizeOfImage - sizeof(COverlayContext_Present_bytes); i++)
				{
					unsigned char* address = (unsigned char*)dwmcore + i;
					if (!COverlayContext_Present_orig && !memcmp(address, COverlayContext_Present_bytes,
					                                             sizeof(COverlayContext_Present_bytes)))
					{
						COverlayContext_Present_orig = (COverlayContext_Present_t*)address;
						COverlayContext_Present_real_orig = COverlayContext_Present_orig;
					}
					else if (!COverlayContext_IsCandidateDirectFlipCompatbile_orig && !memcmp(
						address, COverlayContext_IsCandidateDirectFlipCompatbile_bytes,
						sizeof(COverlayContext_IsCandidateDirectFlipCompatbile_bytes)))
					{
						static int found = 0;
						found++;
						if (found == 2)
						{
							COverlayContext_IsCandidateDirectFlipCompatbile_orig = (
								COverlayContext_IsCandidateDirectFlipCompatbile_t*)(address - 0xa);
						}
					}
					else if (!COverlayContext_OverlaysEnabled_orig && !memcmp(
						address, COverlayContext_OverlaysEnabled_bytes, sizeof(COverlayContext_OverlaysEnabled_bytes)))
					{
						COverlayContext_OverlaysEnabled_orig = (COverlayContext_OverlaysEnabled_t*)(address - 0x7);
					}
					if (COverlayContext_Present_orig && COverlayContext_IsCandidateDirectFlipCompatbile_orig &&
						COverlayContext_OverlaysEnabled_orig)
					{
						break;
					}
				}
			}

			HKEY hKey;
			if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, SCRNFLTR_REGISTRY_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
			{
				DWORD dwSize = sizeof(DWORD);
				RegQueryValueExW(hKey, L"Colors", NULL, NULL, (LPBYTE)&g_colors, &dwSize);
				RegQueryValueExW(hKey, L"Flags", NULL, NULL, (LPBYTE)&g_flags, &dwSize);
				RegQueryValueExW(hKey, L"Palette", NULL, NULL, (LPBYTE)&g_palette, &dwSize);
				RegQueryValueExW(hKey, L"MonoColor", NULL, NULL, (LPBYTE)&g_monocolor, &dwSize);
				RegCloseKey(hKey);
			}
			char variable_message_states[300];
			sprintf(variable_message_states, "Current variable states: COverlayContext::Present - %p\t"
			        "COverlayContext::IsCandidateDirectFlipCompatible - %p\tCOverlayContext::OverlaysEnabled - %p",
			        COverlayContext_Present_orig,
			        COverlayContext_IsCandidateDirectFlipCompatbile_orig, COverlayContext_OverlaysEnabled_orig);

			MESSAGE_BOX_DBG(variable_message_states, MB_OK)

			if (COverlayContext_Present_orig && COverlayContext_IsCandidateDirectFlipCompatbile_orig &&
				COverlayContext_OverlaysEnabled_orig)

			{
				MH_Initialize();
				MH_CreateHook((PVOID)COverlayContext_Present_orig, (PVOID)COverlayContext_Present_hook,
				              (PVOID*)&COverlayContext_Present_orig);
				MH_CreateHook((PVOID)COverlayContext_IsCandidateDirectFlipCompatbile_orig,
				              (PVOID)COverlayContext_IsCandidateDirectFlipCompatbile_hook,
				              (PVOID*)&COverlayContext_IsCandidateDirectFlipCompatbile_orig);
				MH_CreateHook((PVOID)COverlayContext_OverlaysEnabled_orig, (PVOID)COverlayContext_OverlaysEnabled_hook,
				              (PVOID*)&COverlayContext_OverlaysEnabled_orig);
				MH_EnableHook(MH_ALL_HOOKS);
				LOG_ONLY_ONCE("DWM HOOK DLL INITIALIZATION. START LOGGING")
				MESSAGE_BOX_DBG("DWM HOOK INITIALIZATION", MB_OK)

				break;
			}
			return FALSE;
		}
	case DLL_PROCESS_DETACH:
		MH_Uninitialize();
		Sleep(100);
		UninitializeStuff();
		break;
	default:
		break;
	}
	return TRUE;
}
