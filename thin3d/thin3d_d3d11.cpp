#include <vector>
#include <stdio.h>
#include <inttypes.h>

#ifdef _DEBUG
#define D3D_DEBUG_INFO
#endif

#include <d3d11.h>
#include <d3dcompiler.h>
#include <Windows.h> 

#include "base/logging.h"
#include "math/lin/matrix4x4.h"
#include "thin3d/thin3d.h"

// Could be declared as u8
static const D3D11_COMPARISON_FUNC compareToD3D11[] = {
	D3D11_COMPARISON_NEVER,
	D3D11_COMPARISON_LESS,
	D3D11_COMPARISON_EQUAL,
	D3D11_COMPARISON_LESS_EQUAL,
	D3D11_COMPARISON_GREATER,
	D3D11_COMPARISON_NOT_EQUAL,
	D3D11_COMPARISON_GREATER_EQUAL,
	D3D11_COMPARISON_ALWAYS
};

// Could be declared as u8
static const D3D11_BLEND_OP blendEqToD3D11[] = {
	D3D11_BLEND_OP_ADD,
	D3D11_BLEND_OP_SUBTRACT,
	D3D11_BLEND_OP_REV_SUBTRACT,
	D3D11_BLEND_OP_MIN,
	D3D11_BLEND_OP_MAX,
};

// Could be declared as u8
static const D3D11_BLEND blendFactorToD3D11[] = {
	D3D11_BLEND_ZERO,
	D3D11_BLEND_ONE,
	D3D11_BLEND_SRC_COLOR,
	D3D11_BLEND_SRC_ALPHA,
	D3D11_BLEND_INV_SRC_COLOR,
	D3D11_BLEND_INV_SRC_ALPHA,
	D3D11_BLEND_DEST_COLOR,
	D3D11_BLEND_DEST_ALPHA,
	D3D11_BLEND_INV_DEST_COLOR,
	D3D11_BLEND_INV_DEST_ALPHA,
	D3D11_BLEND_BLEND_FACTOR,
};

static const D3D11_PRIMITIVE primToD3D11[] = {
	D3D11_PRIMITIVE_POINT,
	D3D11_PRIMITIVE_LINE,
	D3D11_PRIMITIVE_TRIANGLE,
};

static inline void Uint32ToFloat4(uint32_t u, float f[4]) {
	f[0] = ((u >> 0) & 0xFF) * (1.0f / 255.0f);
	f[1] = ((u >> 8) & 0xFF) * (1.0f / 255.0f);
	f[2] = ((u >> 16) & 0xFF) * (1.0f / 255.0f);
	f[3] = ((u >> 24) & 0xFF) * (1.0f / 255.0f);
}

class Thin3DDX11DepthStencilState : public Thin3DDepthStencilState {
public:
	Thin3DDX11DepthStencilState(ID3D11DepthStencilState *state, int stencilRef) : state_(state), stencilRef_(stencilRef) {}
	~Thin3DDX11DepthStencilState() {
		state_->Release();
	}
	void Apply(ID3D11DeviceContext *context) {
		context->OMSetDepthStencilState(state_, stencilRef_);
	}
private:
	int stencilRef_;
	ID3D11DepthStencilState *state_;
};

class Thin3DDX11BlendState : public Thin3DBlendState {
public:
	Thin3DDX11BlendState(ID3D11BlendState *state, uint32_t blendFactor) : state_(state), blendFactor_(blendFactor) {}
	~Thin3DDX11BlendState() {
		state_->Release();
	}

	void Apply(ID3D11DeviceContext *context) {
		float bf[4];
		Uint32ToFloat4(blendFactor_, bf);
		context->OMSetBlendState(state_, bf, 0xffffffff);
	}

private:
	ID3D11BlendState *state_;
	uint32_t blendFactor_;
};

class Thin3DDX11Buffer : public Thin3DBuffer {
public:
	Thin3DDX11Buffer(ID3D11Device *device, ID3D11DeviceContext *context, size_t size, uint32_t flags) : context_(context), buffer_(nullptr), maxSize_(size) {
		D3D11_BUFFER_DESC desc;
		memset(&desc, 0, sizeof(desc));
		desc.Usage = D3D11_USAGE_DEFAULT;
		if (flags & T3DBufferUsage::INDEXDATA) {
			desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		} else {
			desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		}
		device->CreateBuffer(&desc, NULL, &buffer_);
	}
	void SetData(const uint8_t *data, size_t size) override {
		if (!size)
			return;
		if (size > maxSize_) {
			ELOG("Can't SetData with bigger size than buffer was created with on D3D");
			return;
		}
		if (buffer_) {
			D3D11_BOX box;
			box.left = 0;
			box.right = size;
			box.front = 0;
			box.back = 1;
			box.top = 0;
			box.bottom = 1;
			context_->UpdateSubresource(buffer_, 0, &box, data, 0, 0);
		}
	}
	void SubData(const uint8_t *data, size_t offset, size_t size) override {
		if (!size)
			return;
		if (offset + size > maxSize_) {
			ELOG("Can't SubData with bigger size than buffer was created with on D3D");
			return;
		}
		if (buffer_) {
			D3D11_BOX box;
			box.left = offset;
			box.right = offset + size;
			box.front = 0;
			box.back = 1;
			box.top = 0;
			box.bottom = 1;
			context_->UpdateSubresource(buffer_, 0, &box, data, 0, 0);
		}
	}
	void BindAsVertexBuf(ID3D11DeviceContext *context, int vertexSize, int offset = 0) {
		if (buffer_) {
			context->IASetVertexBuffers(0, 1, &buffer_, (const UINT*)&vertexSize, (const UINT*)&offset);
		}
	}

	void BindAsIndexBuf(ID3D11DeviceContext *context) {
		if (buffer_) {
			context->IASetIndexBuffer(buffer_, DXGI_FORMAT_R16_UINT, 0);
		}
	}

private:
	ID3D11Buffer *buffer_;
	ID3D11DeviceContext *context_;
	size_t maxSize_;
};

class Thin3DDX11Shader;

class Thin3DDX11VertexFormat : public Thin3DVertexFormat {
public:
	Thin3DDX11VertexFormat(ID3D11Device *device, const std::vector<Thin3DVertexComponent> &components, int stride, Thin3DDX11Shader *vshader);
	~Thin3DDX11VertexFormat() {
		if (layout_) {
			layout_->Release();
		}
	}
	int GetStride() const { return stride_; }
	void Apply(ID3D11DeviceContext *context) {
		context->IASetInputLayout(layout_);
	}

private:
	ID3D11InputLayout *layout_;
	int stride_;
};

class Thin3DDX11Shader : public Thin3DShader {
public:
	Thin3DDX11Shader(bool isPixelShader) : isPixelShader_(isPixelShader), vshader_(nullptr), pshader_(nullptr) {}
	~Thin3DDX11Shader() {
		if (vshader_)
			vshader_->Release();
		if (pshader_)
			pshader_->Release();
		//if (constantTable_)
	//		constantTable_->Release();
	}
	bool Compile(ID3D11Device *device, const char *source, const char *profile);
	void Apply(ID3D11DeviceContext *context) {
		if (isPixelShader_) {
			context->PSSetShader(pshader_, nullptr, 0);
		} else {
			context->VSSetShader(vshader_, nullptr, 0);
		}
	}
	void SetVector(ID3D11DeviceContext *device, const char *name, float *value, int n);
	void SetMatrix4x4(ID3D11DeviceContext *device, const char *name, const Matrix4x4 &value);

	uint8_t *GetByteCode();
	size_t GetByteCodeSize();

private:
	bool isPixelShader_;
	ID3D11VertexShader *vshader_;
	ID3D11PixelShader *pshader_;
	
	uint8_t *byteCode_;
	size_t byteCodeSize_;
};

class Thin3DDX11ShaderSet : public Thin3DShaderSet {
public:
	Thin3DDX11ShaderSet() {}
	Thin3DDX11Shader *vshader;
	Thin3DDX11Shader *pshader;
	void Apply(ID3D11DeviceContext *context);
	void SetVector(const char *name, float *value, int n) {
		//vshader->SetVector(device_, name, value, n);
		//pshader->SetVector(device_, name, value, n);
	}
	void SetMatrix4x4(const char *name, const Matrix4x4 &value) {
		//vshader->SetMatrix4x4(device_, name, value);
	}
};

class Thin3DDX11Texture : public Thin3DTexture {
public:
	Thin3DDX11Texture(ID3D11Device *device, ID3D11DeviceContext *context) 
		: device_(device), context_(context), type_(T3DTextureType::UNKNOWN), fmt_(DXGI_FORMAT_UNKNOWN), t3dFmt_(T3DImageFormat::IMG_UNKNOWN), tex_(NULL), volTex_(NULL), cubeTex_(NULL) { }
	Thin3DDX11Texture(ID3D11Device *device, ID3D11DeviceContext *context, T3DTextureType type, T3DImageFormat format, int width, int height, int depth, int mipLevels);
	~Thin3DDX11Texture();

	bool Create(T3DTextureType type, T3DImageFormat format, int width, int height, int depth, int mipLevels) override;
	void SetImageData(int x, int y, int z, int width, int height, int depth, int level, int stride, const uint8_t *data) override;
	void AutoGenMipmaps() override {}
	void Finalize(int zim_flags) override {}
	ID3D11ShaderResourceView *GetResourceView() { return resourceView_; }

private:
	ID3D11Device *device_;
	ID3D11DeviceContext *context_;
	D3D11_TEXTURE2D_DESC desc_;
	T3DImageFormat t3dFmt_;
	DXGI_FORMAT fmt_;
	T3DTextureType type_;
	ID3D11Texture2D *tex_;
	ID3D11Texture3D *volTex_;
	ID3D11Texture2D *cubeTex_;
	ID3D11ShaderResourceView *resourceView_;
};

DXGI_FORMAT FormatToD3D(T3DImageFormat fmt) {
	switch (fmt) {
	case RGBA8888: return DXGI_FORMAT_R8G8B8A8_UNORM;
	case RGBA4444: return DXGI_FORMAT_R8G8B8A8_UNORM; // Unsupported
	case D24S8: return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case D16: return DXGI_FORMAT_D16_UNORM;
	default: return DXGI_FORMAT_UNKNOWN;
	}
}

Thin3DDX11Texture::Thin3DDX11Texture(ID3D11Device *device, ID3D11DeviceContext *context, T3DTextureType type, T3DImageFormat format, int width, int height, int depth, int mipLevels)
	: device_(device), context_(context), type_(type), tex_(NULL), volTex_(NULL), cubeTex_(NULL) {
	Create(type, format, width, height, depth, mipLevels);
}

Thin3DDX11Texture::~Thin3DDX11Texture() {
	if (resourceView_) {
		resourceView_->Release();
	}
	switch (type_) {
	case LINEAR2D:
		if (tex_)
			tex_->Release();
		break;
	default:
		break;
	}
}

bool Thin3DDX11Texture::Create(T3DTextureType type, T3DImageFormat format, int width, int height, int depth, int mipLevels) {
	HRESULT hr = E_FAIL;
	switch (type) {
	case LINEAR2D:
	{
		desc_.Width = width;
		desc_.Height = height;
		desc_.Format = fmt_;
		type_ = type;
		tex_ = NULL;
		fmt_ = FormatToD3D(format);
		hr = device_->CreateTexture2D(&desc_, NULL, &tex_);
		// A shader resource view that just views the entire resource can pass NULL for the desc.
		hr = device_->CreateShaderResourceView(tex_, NULL, &resourceView_);
		break;
	}
	case LINEAR3D:
		//hr = device_->CreateVolumeTexture(width, height, depth, mipLevels, 0, fmt_, D3DPOOL_MANAGED, &volTex_, NULL);
		break;
	case CUBE:
		//hr = device_->CreateCubeTexture(width, mipLevels, 0, fmt_, D3DPOOL_MANAGED, &cubeTex_, NULL);
		break;
	case LINEAR1D:
		break;
	}

	if (FAILED(hr)) {
		ELOG("Texture creation failed");
		return false;
	}
	return true;
}

inline uint16_t Shuffle4444(uint16_t x) {
	return (x << 12) | (x >> 4);
}

// Just switches R and G.
inline uint32_t Shuffle8888(uint32_t x) {
	return (x & 0xFF00FF00) | ((x >> 16) & 0xFF) | ((x << 16) & 0xFF0000);
}

void Thin3DDX11Texture::SetImageData(int x, int y, int z, int width, int height, int depth, int level, int stride, const uint8_t *data) {
	if (!tex_) {
		return;
	}
	if (level == 0) {
		width_ = width;
		height_ = height;
		depth_ = depth;
	}

	switch (type_) {
	case LINEAR2D:
	{
		if (x == 0 && y == 0) {
			D3D11_BOX box;
			box.left = x;
			box.top = y;
			box.front = z;
			box.right = x + width;
			box.bottom = y + height;
			box.back = z + depth;
			context_->UpdateSubresource(tex_, level, &box, data, stride, 0);

			/*
			for (int i = 0; i < height; i++) {
				uint8_t *dest = (uint8_t *)rect.pBits + rect.Pitch * i;
				const uint8_t *source = data + stride * i;
				int j;
				switch (t3dFmt_) {
				case T3DImageFormat::RGBA4444:
					// TODO: Expand to 8888
					// This is backwards from OpenGL so we need to shuffle around a bit.
					for (j = 0; j < width; j++) {
						((uint32_t *)dest)[j] = Shuffle4444(((uint16_t *)source)[j]);
					}
					break;

				case T3DImageFormat::RGBA8888:
					for (j = 0; j < width; j++) {
						((uint32_t *)dest)[j] = Shuffle8888(((uint32_t *)source)[j]);
					}
					break;
				}
			}
			*/
		}
		break;
	}

	default:
		ELOG("Non-LINEAR2D textures not yet supported");
		break;
	}
}

class Thin3DDX11Context : public Thin3DContext {
public:
	Thin3DDX11Context(ID3D11Device *d3d, ID3D11DeviceContext *ctx);
	~Thin3DDX11Context();

	Thin3DDepthStencilState *CreateDepthStencilState(bool depthTestEnabled, bool depthWriteEnabled, T3DComparison depthCompare);
	Thin3DBlendState *CreateBlendState(const T3DBlendStateDesc &desc) override;
	Thin3DBuffer *CreateBuffer(size_t size, uint32_t usageFlags) override;
	Thin3DShaderSet *CreateShaderSet(Thin3DShader *vshader, Thin3DShader *fshader) override;
	Thin3DVertexFormat *CreateVertexFormat(const std::vector<Thin3DVertexComponent> &components, int stride, Thin3DShader *vshader) override;
	Thin3DTexture *CreateTexture() override;
	Thin3DTexture *CreateTexture(T3DTextureType type, T3DImageFormat format, int width, int height, int depth, int mipLevels) override;

	Thin3DShader *CreateVertexShader(const char *glsl_source, const char *hlsl_source) override;
	Thin3DShader *CreateFragmentShader(const char *glsl_source, const char *hlsl_source) override;

	// Bound state objects. Too cumbersome to add them all as parameters to Draw.
	void SetBlendState(Thin3DBlendState *state) {
		Thin3DDX11BlendState *bs = static_cast<Thin3DDX11BlendState *>(state);
		bs->Apply(context_);
	}
	void SetDepthStencilState(Thin3DDepthStencilState *state) {
		Thin3DDX11DepthStencilState *bs = static_cast<Thin3DDX11DepthStencilState *>(state);
		bs->Apply(context_);
	}

	void SetTextures(int start, int count, Thin3DTexture **textures);

	// Raster state
	void SetScissorEnabled(bool enable);
	void SetScissorRect(int left, int top, int width, int height);
	void SetViewports(int count, T3DViewport *viewports);

	void SetRenderState(T3DRenderState rs, uint32_t value) override;

	void Draw(T3DPrimitive prim, Thin3DShaderSet *pipeline, Thin3DVertexFormat *format, Thin3DBuffer *vdata, int vertexCount, int offset) override;
	void DrawIndexed(T3DPrimitive prim, Thin3DShaderSet *pipeline, Thin3DVertexFormat *format, Thin3DBuffer *vdata, Thin3DBuffer *idata, int vertexCount, int offset) override;
	void Clear(int mask, uint32_t colorval, float depthVal, int stencilVal);

	const char *GetInfoString(T3DInfo info) const override {
		switch (info) {
		case APIVERSION: return "DirectX 11.0";
		case VENDOR: return "Description";
		case RENDERER: return "Driver";  // eh, sort of
		case SHADELANGVERSION: return "shadeLangVersion_";
		case APINAME: return "Direct3D 9";
		default: return "?";
		}
	}

private:
	ID3D11Device *device_;
	ID3D11DeviceContext *context_;

	ID3D11RenderTargetView *renderTargetView_;
	ID3D11DepthStencilView *depthStencilView_;

	// Rasterizer state emulation by precomputing six states.
	// Might change this to yet another state object instead...
	ID3D11RasterizerState *rastStates_[3][2];
	T3DCullMode cullMode_;
	bool scissorEnabled_;
};

Thin3DDX11Context::Thin3DDX11Context(ID3D11Device *device, ID3D11DeviceContext *context) : device_(device), context_(context) {
	CreatePresets();

	D3D11_RASTERIZER_DESC rastDesc;
	memset(&rastDesc, 0, sizeof(rastDesc));
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 2; j++) {
			switch (i) {
			case T3DCullMode::NO_CULL: rastDesc.CullMode = D3D11_CULL_NONE; break;
			case T3DCullMode::CCW: rastDesc.CullMode = D3D11_CULL_FRONT; break;
			case T3DCullMode::CW: rastDesc.CullMode = D3D11_CULL_BACK; break;
			}
			rastDesc.ScissorEnable = j;
			device->CreateRasterizerState(&rastDesc, &rastStates_[i][j]);
		}
	}
}

Thin3DDX11Context::~Thin3DDX11Context() {
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 2; j++) {
			rastStates_[i][j]->Release();
		}
	}
}

Thin3DShader *Thin3DDX11Context::CreateVertexShader(const char *glsl_source, const char *hlsl_source) {
	Thin3DDX11Shader *shader = new Thin3DDX11Shader(false);
	if (shader->Compile(device_, hlsl_source, "vs_2_0")) {
		return shader;
	} else {
		delete shader;
		return NULL;
	}
}

Thin3DShader *Thin3DDX11Context::CreateFragmentShader(const char *glsl_source, const char *hlsl_source) {
	Thin3DDX11Shader *shader = new Thin3DDX11Shader(true);
	if (shader->Compile(device_, hlsl_source, "ps_2_0")) {
		return shader;
	} else {
		delete shader;
		return NULL;
	}
}

Thin3DShaderSet *Thin3DDX11Context::CreateShaderSet(Thin3DShader *vshader, Thin3DShader *fshader) {
	if (!vshader || !fshader) {
		ELOG("ShaderSet requires both a valid vertex and a fragment shader: %p %p", vshader, fshader);
		return NULL;
	}
	Thin3DDX11ShaderSet *shaderSet = new Thin3DDX11ShaderSet();
	shaderSet->vshader = static_cast<Thin3DDX11Shader *>(vshader);
	shaderSet->pshader = static_cast<Thin3DDX11Shader *>(fshader);
	return shaderSet;
}

Thin3DDepthStencilState *Thin3DDX11Context::CreateDepthStencilState(bool depthTestEnabled, bool depthWriteEnabled, T3DComparison depthCompare) {
	D3D11_DEPTH_STENCIL_DESC dssDesc;
	ZeroMemory(&dssDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));
	dssDesc.DepthEnable = depthTestEnabled;
	dssDesc.DepthWriteMask = depthWriteEnabled ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
	dssDesc.DepthFunc = compareToD3D11[depthCompare];
	ID3D11DepthStencilState *dss;
	if (!FAILED(device_->CreateDepthStencilState(&dssDesc, &dss))) {
		Thin3DDX11DepthStencilState *ds = new Thin3DDX11DepthStencilState(dss, 255);
		return ds;
	} else {
		return nullptr;
	}
}

Thin3DVertexFormat *Thin3DDX11Context::CreateVertexFormat(const std::vector<Thin3DVertexComponent> &components, int stride, Thin3DShader *vshader) {
	Thin3DDX11VertexFormat *fmt = new Thin3DDX11VertexFormat(device_, components, stride, static_cast<Thin3DDX11Shader *>(vshader));
	return fmt;
}

Thin3DBlendState *Thin3DDX11Context::CreateBlendState(const T3DBlendStateDesc &desc) {
	D3D11_BLEND_DESC desc11;
	desc11.RenderTarget[0].BlendEnable = desc.enabled;
	desc11.RenderTarget[0].BlendOp = blendEqToD3D11[desc.eqCol];
	desc11.RenderTarget[0].BlendOpAlpha = blendEqToD3D11[desc.eqAlpha];
	desc11.RenderTarget[0].SrcBlend = blendFactorToD3D11[desc.srcCol];
	desc11.RenderTarget[0].DestBlend = blendFactorToD3D11[desc.dstCol];
	desc11.RenderTarget[0].SrcBlendAlpha = blendFactorToD3D11[desc.srcAlpha];
	desc11.RenderTarget[0].DestBlendAlpha = blendFactorToD3D11[desc.dstAlpha];
	desc11.AlphaToCoverageEnable = false;

	ID3D11BlendState *blendState;
	if (!FAILED(device_->CreateBlendState(&desc11, &blendState))) {
		return new Thin3DDX11BlendState(blendState, 0);
	} else {
		return nullptr;
	}
}

Thin3DTexture *Thin3DDX11Context::CreateTexture() {
	Thin3DDX11Texture *tex = new Thin3DDX11Texture(device_, context_);
	return tex;
}

Thin3DTexture *Thin3DDX11Context::CreateTexture(T3DTextureType type, T3DImageFormat format, int width, int height, int depth, int mipLevels) {
	Thin3DDX11Texture *tex = new Thin3DDX11Texture(device_, context_, type, format, width, height, depth, mipLevels);
	return tex;
}

void Thin3DDX11Context::SetTextures(int start, int count, Thin3DTexture **textures) {
	ID3D11ShaderResourceView *resViews[16];
	if (count > 16) count = 16;
	for (int i = 0; i < count; i++) {
		Thin3DDX11Texture *tex = static_cast<Thin3DDX11Texture *>(textures[i]);
		resViews[i] = tex->GetResourceView();
	}
	context_->PSSetShaderResources(start, count, resViews);
}

void SemanticToD3D11UsageAndIndex(int semantic, LPCSTR *usage, UINT *index) {
	*index = 0;
	switch (semantic) {
	case SEM_POSITION:
		*usage = "Position";
		break;
	case SEM_NORMAL:
		*usage = "Normal";
		break;
	case SEM_TANGENT:
		*usage = "Tangent";
		break;
	case SEM_BINORMAL:
		*usage = "Binormal";
		break;
	case SEM_COLOR0:
		*usage = "Color0";
		break;
	case SEM_TEXCOORD0:
		*usage = "TexCoord0";
		break;
	case SEM_TEXCOORD1:
		*usage = "TexCoord1";
		*index = 1;
		break;
	}
}

static DXGI_FORMAT VertexDataTypeToD3D11Format(T3DVertexDataType type) {
	switch (type) {
	case T3DVertexDataType::FLOATx2: return DXGI_FORMAT_R32G32_FLOAT;
	case T3DVertexDataType::FLOATx3: return DXGI_FORMAT_R32G32B32_FLOAT;
	case T3DVertexDataType::FLOATx4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case T3DVertexDataType::UNORM8x4: return DXGI_FORMAT_R8G8B8A8_UNORM;  // D3DCOLOR?
	default: return DXGI_FORMAT_UNKNOWN;
	}
}

Thin3DDX11VertexFormat::Thin3DDX11VertexFormat(ID3D11Device *device, const std::vector<Thin3DVertexComponent> &components, int stride, Thin3DDX11Shader *vshader) : layout_(nullptr) {
	D3D11_INPUT_ELEMENT_DESC *elements = new D3D11_INPUT_ELEMENT_DESC[components.size()];
	size_t i;
	for (i = 0; i < components.size(); i++) {
		elements[i].InputSlot = 0;
		elements[i].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;  // TODO: Translate offset
		elements[i].Format = VertexDataTypeToD3D11Format(components[i].type);
		SemanticToD3D11UsageAndIndex(components[i].semantic, &elements[i].SemanticName, &elements[i].SemanticIndex);
		elements[i].InstanceDataStepRate = 0;
	}

	HRESULT hr = device->CreateInputLayout(elements, (UINT)components.size(), vshader->GetByteCode(), vshader->GetByteCodeSize(), &layout_);
	delete[] elements;
	if (FAILED(hr)) {
		ELOG("Error creating vertex decl");
	}
	stride_ = stride;
}

Thin3DBuffer *Thin3DDX11Context::CreateBuffer(size_t size, uint32_t usageFlags) {
	return new Thin3DDX11Buffer(device_, context_, size, usageFlags);
}

void Thin3DDX11ShaderSet::Apply(ID3D11DeviceContext *context) {
	vshader->Apply(context);
	pshader->Apply(context);
}

void Thin3DDX11Context::SetRenderState(T3DRenderState rs, uint32_t value) {
	switch (rs) {
	case T3DRenderState::CULL_MODE:
		cullMode_ = (T3DCullMode)value;
		break;
	}
}

D3D11_PRIMITIVE_TOPOLOGY GetPrimitiveTopology(T3DPrimitive prim) {
	switch (prim) {
	case PRIM_LINES: return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	case PRIM_TRIANGLES: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	case PRIM_POINTS: return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
	default: return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
	}
}

void Thin3DDX11Context::Draw(T3DPrimitive prim, Thin3DShaderSet *shaderSet, Thin3DVertexFormat *format, Thin3DBuffer *vdata, int vertexCount, int offset) {
	Thin3DDX11Buffer *vbuf = static_cast<Thin3DDX11Buffer *>(vdata);
	Thin3DDX11VertexFormat *fmt = static_cast<Thin3DDX11VertexFormat *>(format);
	Thin3DDX11ShaderSet *ss = static_cast<Thin3DDX11ShaderSet*>(shaderSet);

	context_->RSSetState(rastStates_[cullMode_][scissorEnabled_]);

	/*
	device_->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	device_->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	device_->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	device_->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	device_->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
	*/

	context_->IASetPrimitiveTopology(GetPrimitiveTopology(prim));

	vbuf->BindAsVertexBuf(context_, fmt->GetStride(), offset);
	ss->Apply(context_);
	fmt->Apply(context_);
	context_->Draw(vertexCount, offset);
}

void Thin3DDX11Context::DrawIndexed(T3DPrimitive prim, Thin3DShaderSet *shaderSet, Thin3DVertexFormat *format, Thin3DBuffer *vdata, Thin3DBuffer *idata, int vertexCount, int offset) {
	Thin3DDX11Buffer *vbuf = static_cast<Thin3DDX11Buffer *>(vdata);
	Thin3DDX11Buffer *ibuf = static_cast<Thin3DDX11Buffer *>(idata);
	Thin3DDX11VertexFormat *fmt = static_cast<Thin3DDX11VertexFormat *>(format);
	Thin3DDX11ShaderSet *ss = static_cast<Thin3DDX11ShaderSet*>(shaderSet);

	ss->Apply(context_);
	fmt->Apply(context_);
	vbuf->BindAsVertexBuf(context_, fmt->GetStride(), offset);
	ibuf->BindAsIndexBuf(context_);

	context_->IASetPrimitiveTopology(GetPrimitiveTopology(prim));
	context_->DrawIndexed(vertexCount, 0, offset);
}

void Thin3DDX11Context::Clear(int mask, uint32_t colorval, float depthVal, int stencilVal) {
	if (mask & T3DClear::COLOR) {
		float colorVal[4];
		Uint32ToFloat4(colorval, colorVal);
		context_->ClearRenderTargetView(renderTargetView_, colorVal);
	}
	if (mask & (T3DClear::DEPTH | T3DClear::STENCIL)) {
		UINT d3dMask = 0;
		if (mask & T3DClear::DEPTH) d3dMask |= D3D11_CLEAR_DEPTH;
		if (mask & T3DClear::STENCIL) d3dMask |= D3D11_CLEAR_STENCIL;
		context_->ClearDepthStencilView(depthStencilView_, d3dMask, depthVal, stencilVal);
	}
}

void Thin3DDX11Context::SetScissorEnabled(bool enable) {
	scissorEnabled_ = enable;
}

void Thin3DDX11Context::SetScissorRect(int left, int top, int width, int height) {
	D3D11_RECT rc;
	rc.left = left;
	rc.top = top;
	rc.right = left + width;
	rc.bottom = top + height;
	context_->RSSetScissorRects(1, &rc);
}

void Thin3DDX11Context::SetViewports(int count, T3DViewport *viewports) {
	D3D11_VIEWPORT vp;
	vp.TopLeftX = viewports[0].TopLeftX;
	vp.TopLeftY = viewports[0].TopLeftY;
	vp.Width = viewports[0].Width;
	vp.Height = viewports[0].Height;
	vp.MinDepth = viewports[0].MinDepth;
	vp.MaxDepth = viewports[0].MaxDepth;
	context_->RSSetViewports(1, &vp);
}

bool Thin3DDX11Shader::Compile(ID3D11Device *device, const char *source, const char *profile) {
	DWORD flags = 0;
	D3D_SHADER_MACRO *shaderMacro = nullptr;
	ID3DInclude *shaderInclude = nullptr;
	const char *target = profile;
	ID3DBlob *codeBuffer;
	ID3DBlob *errorBuffer;

	// TODO: Make a cache for compiled code (hash_of_source->bytecode).
	HRESULT hr = D3DCompile(source, strlen(source), NULL, shaderMacro, shaderInclude, "main", profile, flags, 0, &codeBuffer, &errorBuffer);

	if (FAILED(hr)) {
		const char *error = (const char *)errorBuffer->GetBufferPointer();
		OutputDebugStringA(source);
		OutputDebugStringA(error);
		errorBuffer->Release();
		if (codeBuffer)
			codeBuffer->Release();
		//if (constantTable_)
		//	constantTable_->Release();
		return false;
	}

	byteCode_ = new uint8_t[codeBuffer->GetBufferSize()];
	memcpy(byteCode_, codeBuffer->GetBufferPointer(), byteCodeSize_);
	codeBuffer->Release();

	HRESULT result;
	if (isPixelShader_) {
		result = device->CreatePixelShader(byteCode_, byteCodeSize_, NULL, &pshader_);
	} else {
		result = device->CreateVertexShader(byteCode_, byteCodeSize_, NULL, &vshader_);
	}

	return SUCCEEDED(result);
}

void Thin3DDX11Shader::SetVector(ID3D11DeviceContext *context, const char *name, float *value, int n) {
	//D3DXHANDLE handle = constantTable_->GetConstantByName(NULL, name);
	//if (handle) {
	//	constantTable_->SetFloatArray(device, handle, value, n);
	//}
}

void Thin3DDX11Shader::SetMatrix4x4(ID3D11DeviceContext *context, const char *name, const Matrix4x4 &value) {
	//D3DXHANDLE handle = constantTable_->GetConstantByName(NULL, name);
	//if (handle) {
	//	constantTable_->SetFloatArray(device, handle, value.getReadPtr(), 16);
	//}
}

Thin3DContext *T3DCreateDX11Context(ID3D11Device *d3d, ID3D11DeviceContext *ctx) {
	return new Thin3DDX11Context(d3d, ctx);
}
