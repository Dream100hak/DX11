#include "pch.h"
#include "Pass.h"

void Pass::Draw(UINT vertexCount, UINT startVertexLocation)
{
	BeginDraw();
	{
		DCT->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		DCT->Draw(vertexCount, startVertexLocation);
	}
	EndDraw();
}


void Pass::DrawIndexed(UINT indexCount, UINT startIndexLocation, INT baseVertexLocation)
{
	BeginDraw();
	{
		DCT->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		DCT->DrawIndexed(indexCount, startIndexLocation, baseVertexLocation);
	}
	EndDraw();
}

void Pass::DrawInstanced(UINT vertexCountPerInstance, UINT instanceCount, UINT startVertexLocation, UINT startInstanceLocation)
{
	BeginDraw();
	{
		DCT->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		DCT->DrawInstanced(vertexCountPerInstance, instanceCount, startVertexLocation, startInstanceLocation);
	}
	EndDraw();
}

void Pass::DrawIndexedInstanced(UINT indexCountPerInstance, UINT instanceCount, UINT startIndexLocation, INT baseVertexLocation, UINT startInstanceLocation)
{
	BeginDraw();
	{
		DCT->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		DCT->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndexLocation, baseVertexLocation, startIndexLocation);
	}
	EndDraw();
}


void Pass::DrawTerrainIndexed(UINT indexCount, UINT startIndexLocation /*= 0*/, INT baseVertexLocation /*= 0*/)
{
	BeginDraw();
	{
		DCT->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
		DCT->DrawIndexed(indexCount, startIndexLocation, baseVertexLocation);
	}
	EndDraw();
}

void Pass::DrawLineIndexed(UINT indexCount, UINT startIndexLocation /*= 0*/, INT baseVertexLocation /*= 0*/)
{
	BeginDraw();
	{
		DCT->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
		DCT->DrawIndexed(indexCount, startIndexLocation, baseVertexLocation);
	}
	EndDraw();
}

void Pass::DrawTess(UINT vertexCount, UINT startVertexLocation /*= 0*/)
{
	BeginDraw();
	{
		DCT->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
		DCT->Draw(vertexCount, startVertexLocation);
	}
	EndDraw();
}

void Pass::DrawParticle(UINT vertexCount, UINT startVertexLocation /*= 0*/)
{
	BeginDraw();
	{
		DCT->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
		DCT->Draw(vertexCount, startVertexLocation);
	}
	EndDraw();
}

void Pass::DrawParticleAuto()
{
	BeginDraw();
	{
		DCT->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
		DCT->DrawAuto();
	}
	EndDraw();
}

void Pass::BeginDraw()
{
	pass->ComputeStateBlockMask(&stateblockMask);

	DCT->IASetInputLayout(inputLayout.Get());
	pass->Apply(0, DCT.Get());

}

void Pass::EndDraw()
{
	if (stateblockMask.RSRasterizerState == 1)
		DCT->RSSetState(stateBlock->RSRasterizerState.Get());

	if (stateblockMask.OMDepthStencilState == 1)
		DCT->OMSetDepthStencilState(stateBlock->OMDepthStencilState.Get(), stateBlock->OMStencilRef);

	if (stateblockMask.OMBlendState == 1)
		DCT->OMSetBlendState(stateBlock->OMBlendState.Get(), stateBlock->OMBlendFactor, stateBlock->OMSampleMask);

	DCT->HSSetShader(NULL, NULL, 0);
	DCT->DSSetShader(NULL, NULL, 0);
	DCT->GSSetShader(NULL, NULL, 0);
}


void Pass::Dispatch(UINT x, UINT y, UINT z)
{
	pass->Apply(0, DCT.Get());
	DCT->Dispatch(x, y, z);

	ID3D11ShaderResourceView* null[1] = { 0 };
	DCT->CSSetShaderResources(0, 1, null);

	ID3D11UnorderedAccessView* nullUav[1] = { 0 };
	DCT->CSSetUnorderedAccessViews(0, 1, nullUav, NULL);

	DCT->CSSetShader(NULL, NULL, 0);
}