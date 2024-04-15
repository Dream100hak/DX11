#include "pch.h"
#include "TextureRenderer.h"
#include "GeometryHelper.h"

void TextureRenderer::SetShader(shared_ptr<Shader> shader)
{
	_shader = shader;

	_diffuseEffectBuffer = shader->GetSRV("DiffuseMap");
	_wvpEffectBuffer = shader->GetMatrix("WVP");

	CreateBuffer();
}

void TextureRenderer::CreateBuffer()
{
	_geometry = make_shared<Geometry<VertexTextureNormalData>>();
	GeometryHelper::CreateQuad(_geometry);

	_vertexBuffer = make_shared<VertexBuffer>();
	_vertexBuffer->Create(_geometry->GetVertices());
	_indexBuffer = make_shared<IndexBuffer>();
	_indexBuffer->Create(_geometry->GetIndices());
}

void TextureRenderer::Update(Matrix W)
{
	if (_shader == nullptr)
		return;

	if (_diffuseMap)
		//_diffuseEffectBuffer->SetResource(_diffuseMap->GetComPtr().Get());
		_diffuseEffectBuffer->SetResource(_diffuseMap.Get());

	// IA
	_vertexBuffer->PushData();
	_indexBuffer->PushData();

	_wvpEffectBuffer->SetMatrix(reinterpret_cast<const float*>(&W));
	_shader->DrawIndexed(1, 0 , 6, 0 , 0);

}

