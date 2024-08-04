#include "ppsspp_config.h"

#include <cstdint>
#include <cfloat>
#include <vector>
#include <string>
#include <d3d11.h>
#include <D3Dcompiler.h>

#if PPSSPP_PLATFORM(UWP)
#define ptr_D3DCompile D3DCompile
#else
#include "Common/GPU/D3D11/D3D11Loader.h"
#endif

#include "Common/CommonFuncs.h"
#include "Common/Log.h"
#include "Common/StringUtils.h"
#include "Core/Config.h"
#include "UWP/UWPHelpers/StorageManager.h"

#include "D3D11Util.h"

#include <vector>
#include <string>
#include <algorithm>
#include <regex>
#include <fstream>
#include <filesystem>

extern bool D3DFeatureLevelGlobal;

std::string CalculateHash(const std::string& input) {
	std::hash<std::string> hasher;
	size_t hashValue = hasher(input);
	return std::to_string(hashValue);
}

std::vector<uint8_t> ReadFile(const std::string& filepath) {
	std::ifstream file(filepath, std::ios::binary);
	if (!file) {
		return {};
	}

	return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

bool WriteFile(const std::string& filepath, const std::vector<uint8_t>& data) {
	std::ofstream file(filepath, std::ios::binary);
	if (!file) {
		return false;
	}

	file.write(reinterpret_cast<const char*>(data.data()), data.size());
	return true;
}

void EnsureDirectoryExists(const std::string& dirPath) {
	std::filesystem::create_directories(dirPath);
}

std::vector<uint8_t> CompileShaderWithCache(const std::string& modifiedCode, const std::string& target, UINT flags) {
	// Calculate hash of the shader code
	std::string checksum = CalculateHash((modifiedCode + target));
	std::string cacheFolderPath = GetLocalFolder() + "\\shader_cache";
	EnsureDirectoryExists(cacheFolderPath);
	std::string cacheFilePath = cacheFolderPath + "\\" + checksum + ".bin";

	// Attempt to read the compiled shader from cache
	if (g_Config.bShaderDiskCache) {
		std::vector<uint8_t> cachedShader = ReadFile(cacheFilePath);
		if (!cachedShader.empty()) {
			return cachedShader;
		}
	}

	// Compile the shader
	ID3DBlob* compiledCode = nullptr;
	ID3DBlob* errorMsgs = nullptr;

	HRESULT result = ptr_D3DCompile(
		modifiedCode.c_str(),
		modifiedCode.size(),
		nullptr,
		nullptr,
		nullptr,
		"main",
		target.c_str(),
		flags,
		0,
		&compiledCode,
		&errorMsgs
	);

	std::string errors;
	if (errorMsgs) {
		errors = std::string((const char*)errorMsgs->GetBufferPointer(), errorMsgs->GetBufferSize());
		std::string numberedCode = LineNumberString(modifiedCode); // Assuming LineNumberString function exists
		if (SUCCEEDED(result)) {
			WARN_LOG(G3D, "%s: %s\n\n%s", "warnings", errors.c_str(), numberedCode.c_str());
		}
		else {
			ERROR_LOG(G3D, "%s: %s\n\n%s", "errors", errors.c_str(), numberedCode.c_str());
		}
		OutputDebugStringA(errors.c_str());
		OutputDebugStringA(numberedCode.c_str());
		errorMsgs->Release();
	}

	if (compiledCode) {
		const uint8_t* buf = (const uint8_t*)compiledCode->GetBufferPointer();
		std::vector<uint8_t> compiled(buf, buf + compiledCode->GetBufferSize());
		_assert_(compiled.size() != 0);

		if (g_Config.bShaderDiskCache) {
			// Save the compiled shader to cache
			WriteFile(cacheFilePath, compiled);
		}

		compiledCode->Release();
		return compiled;
	}

	return std::vector<uint8_t>();
}

std::vector<uint8_t> CompileShaderToBytecodeD3D11(const char* code, size_t codeSize, const char* target, UINT flags) {
	// Convert code to a string for easier manipulation
	std::string shaderCode(code, codeSize);
	std::string processedCode;

	if (g_Config.bForceLowPrecision) {
		// Define regexes to find float and vec4 declarations within functions
		std::regex floatDeclRegex(R"((\bfloat\s+)([a-zA-Z_]\w*\s*(\[\s*\d*\s*\])?\s*(=|;|,|\))))");
		std::regex vec2DeclRegex(R"((\bvec2\s+)([a-zA-Z_]\w*\s*(\[\s*\d*\s*\])?\s*(=|;|,|\))))");
		std::regex vec3DeclRegex(R"((\bvec3\s+)([a-zA-Z_]\w*\s*(\[\s*\d*\s*\])?\s*(=|;|,|\))))");
		std::regex vec4DeclRegex(R"((\bvec4\s+)([a-zA-Z_]\w*\s*(\[\s*\d*\s*\])?\s*(=|;|,|\))))");
		std::regex medDeclRegex(R"(\bmediump\b)");
		std::regex highDeclRegex(R"(\bhighp\b)");
		std::regex cbufferRegex(R"(cbuffer\s+\w+\s*:\s*register\s*\(\s*\w+\s*\)\s*\{[^}]*\}|#define\s\w+)");

		// Process the code to exclude uniform buffers and global scope
		auto excludeBuffers = shaderCode;
		std::sregex_iterator begin(excludeBuffers.begin(), excludeBuffers.end(), cbufferRegex);
		std::sregex_iterator end;

		// Replace float and vec4 with half and half4 within functions but not in cbuffer
		size_t lastPos = 0;
		for (std::sregex_iterator i = begin; i != end; ++i) {
			std::smatch match = *i;
			size_t matchPos = match.position();
			size_t matchLen = match.length();

			// Add the code before the cbuffer
			std::string segment = shaderCode.substr(lastPos, matchPos - lastPos);
			segment = std::regex_replace(segment, floatDeclRegex, "half $2");
			segment = std::regex_replace(segment, vec2DeclRegex, "half2 $2");
			segment = std::regex_replace(segment, vec3DeclRegex, "half3 $2");
			segment = std::regex_replace(segment, vec4DeclRegex, "half4 $2");
			segment = std::regex_replace(segment, medDeclRegex, "lowp");
			segment = std::regex_replace(segment, highDeclRegex, "lowp");
			processedCode += segment;

			// Add the cbuffer itself without modification
			processedCode += match.str();

			// Move the position forward
			lastPos = matchPos + matchLen;
		}

		// Add the remaining code after the last cbuffer
		std::string remainingCode = shaderCode.substr(lastPos);
		remainingCode = std::regex_replace(remainingCode, floatDeclRegex, "half $2");
		remainingCode = std::regex_replace(remainingCode, vec2DeclRegex, "half2 $2");
		remainingCode = std::regex_replace(remainingCode, vec3DeclRegex, "half3 $2");
		remainingCode = std::regex_replace(remainingCode, vec4DeclRegex, "half4 $2");
		remainingCode = std::regex_replace(remainingCode, medDeclRegex, "lowp");
		remainingCode = std::regex_replace(remainingCode, highDeclRegex, "lowp");
		processedCode += remainingCode;
	}
	else {
		processedCode = shaderCode;
	}
	// Convert the modified shader code back to a const char* for compilation
	const char* modifiedCode = processedCode.c_str();
	size_t modifiedCodeSize = processedCode.size();

	// Compile the modified shader code
	ID3DBlob* compiledCode = nullptr;
	ID3DBlob* errorMsgs = nullptr;

	flags |= D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY;
	flags |= D3DCOMPILE_OPTIMIZATION_LEVEL0;
	return CompileShaderWithCache(modifiedCode, target, flags);
}


ID3D11VertexShader* CreateVertexShaderD3D11(ID3D11Device* device, const char* code, size_t codeSize, std::vector<uint8_t>* byteCodeOut, D3D_FEATURE_LEVEL featureLevel, UINT flags) {
#if defined(BUILD14393)
	const char* profile = featureLevel <= D3D_FEATURE_LEVEL_9_3 ? (featureLevel <= D3D_FEATURE_LEVEL_9_1 ? "vs_4_0_level_9_1" : "vs_4_0_level_9_3") : "vs_4_0";
#else
	char* profile = "vs_4_0";
	if (g_Config.bBackwardCompatibility) {
		profile = featureLevel <= D3D_FEATURE_LEVEL_9_3 ? (featureLevel <= D3D_FEATURE_LEVEL_9_1 ? "vs_4_0_level_9_1" : "vs_4_0_level_9_3") : "vs_4_0";
	}
#endif
	std::vector<uint8_t> byteCode = CompileShaderToBytecodeD3D11(code, codeSize, profile, flags);
	if (byteCode.empty())
		return nullptr;

	ID3D11VertexShader* vs;
	device->CreateVertexShader(byteCode.data(), byteCode.size(), nullptr, &vs);
	if (byteCodeOut)
		*byteCodeOut = byteCode;
	return vs;
	}

ID3D11PixelShader* CreatePixelShaderD3D11(ID3D11Device* device, const char* code, size_t codeSize, D3D_FEATURE_LEVEL featureLevel, UINT flags) {
#if defined(BUILD14393)
	const char* profile = featureLevel <= D3D_FEATURE_LEVEL_9_3 ? (featureLevel <= D3D_FEATURE_LEVEL_9_1 ? "ps_4_0_level_9_1" : "ps_4_0_level_9_3") : "ps_4_0";
#else
	char* profile = "ps_4_0";
	if (g_Config.bBackwardCompatibility) {
		profile = featureLevel <= D3D_FEATURE_LEVEL_9_3 ? (featureLevel <= D3D_FEATURE_LEVEL_9_1 ? "ps_4_0_level_9_1" : "ps_4_0_level_9_3") : "ps_4_0";
	}
#endif
	std::vector<uint8_t> byteCode = CompileShaderToBytecodeD3D11(code, codeSize, profile, flags);

	if (byteCode.empty())
		return nullptr;

	ID3D11PixelShader* ps;
	device->CreatePixelShader(byteCode.data(), byteCode.size(), nullptr, &ps);
	return ps;
	}

ID3D11ComputeShader* CreateComputeShaderD3D11(ID3D11Device* device, const char* code, size_t codeSize, D3D_FEATURE_LEVEL featureLevel, UINT flags) {
	if (featureLevel <= D3D_FEATURE_LEVEL_9_3)
		return nullptr;
	std::vector<uint8_t> byteCode = CompileShaderToBytecodeD3D11(code, codeSize, "cs_4_0", flags);
	if (byteCode.empty())
		return nullptr;

	ID3D11ComputeShader* cs;
	device->CreateComputeShader(byteCode.data(), byteCode.size(), nullptr, &cs);
	return cs;
}

ID3D11GeometryShader* CreateGeometryShaderD3D11(ID3D11Device* device, const char* code, size_t codeSize, D3D_FEATURE_LEVEL featureLevel, UINT flags) {
	if (featureLevel <= D3D_FEATURE_LEVEL_9_3)
		return nullptr;
	std::vector<uint8_t> byteCode = CompileShaderToBytecodeD3D11(code, codeSize, "gs_5_0", flags);
	if (byteCode.empty())
		return nullptr;

	ID3D11GeometryShader* gs;
	device->CreateGeometryShader(byteCode.data(), byteCode.size(), nullptr, &gs);
	return gs;
}

void StockObjectsD3D11::Create(ID3D11Device* device) {
	D3D11_BLEND_DESC blend_desc{};
	blend_desc.RenderTarget[0].BlendEnable = false;
	blend_desc.IndependentBlendEnable = false;
	for (int i = 0; i < 16; i++) {
		blend_desc.RenderTarget[0].RenderTargetWriteMask = i;
		ASSERT_SUCCESS(device->CreateBlendState(&blend_desc, &blendStateDisabledWithColorMask[i]));
	}

	D3D11_DEPTH_STENCIL_DESC depth_desc{};
	depth_desc.DepthEnable = FALSE;
	ASSERT_SUCCESS(device->CreateDepthStencilState(&depth_desc, &depthStencilDisabled));
	depth_desc.StencilEnable = TRUE;
	depth_desc.StencilReadMask = 0xFF;
	depth_desc.StencilWriteMask = 0xFF;
	depth_desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
	depth_desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_REPLACE;
	depth_desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_REPLACE;
	depth_desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	depth_desc.BackFace = depth_desc.FrontFace;
	ASSERT_SUCCESS(device->CreateDepthStencilState(&depth_desc, &depthDisabledStencilWrite));

	D3D11_RASTERIZER_DESC raster_desc{};
	raster_desc.FillMode = D3D11_FILL_SOLID;
	raster_desc.CullMode = D3D11_CULL_NONE;
	raster_desc.ScissorEnable = FALSE;
	raster_desc.DepthClipEnable = TRUE;  // the default! FALSE is unsupported on D3D11 level 9
	ASSERT_SUCCESS(device->CreateRasterizerState(&raster_desc, &rasterStateNoCull));

	D3D11_SAMPLER_DESC sampler_desc{};
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	for (int i = 0; i < 4; i++)
		sampler_desc.BorderColor[i] = 1.0f;
	sampler_desc.MinLOD = -FLT_MAX;
	sampler_desc.MaxLOD = FLT_MAX;
	sampler_desc.MipLODBias = 0.0f;
	sampler_desc.MaxAnisotropy = 1;
	ASSERT_SUCCESS(device->CreateSamplerState(&sampler_desc, &samplerPoint2DWrap));
	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	ASSERT_SUCCESS(device->CreateSamplerState(&sampler_desc, &samplerLinear2DWrap));
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	ASSERT_SUCCESS(device->CreateSamplerState(&sampler_desc, &samplerPoint2DClamp));
	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	ASSERT_SUCCESS(device->CreateSamplerState(&sampler_desc, &samplerLinear2DClamp));
}

void StockObjectsD3D11::Destroy() {
	for (int i = 0; i < 16; i++) {
		blendStateDisabledWithColorMask[i]->Release();
	}
	depthStencilDisabled->Release();
	depthDisabledStencilWrite->Release();
	rasterStateNoCull->Release();
	samplerPoint2DWrap->Release();
	samplerLinear2DWrap->Release();
	samplerPoint2DClamp->Release();
	samplerLinear2DClamp->Release();
}

StockObjectsD3D11 stockD3D11;
