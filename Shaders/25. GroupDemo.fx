ByteAddressBuffer Input; // SRV
RWByteAddressBuffer Output; // UAV

struct ComputeInput
{
	uint3 groupID : SV_GroupID;
	uint3 groupThreadID : SV_GroupThreadID;
	uint3 dispatchThreadID : SV_DispatchThreadID;
	uint groupIndex : SV_GroupIndex;
};

[numthreads(10, 8, 3)]
void CS(ComputeInput input)
{
	uint index = input.groupID.x * (10 * 8 * 3) + input.groupIndex;
	uint outAddress = index * 11 * 4;

	uint inAddress = index * 4;
	float value = (float)Input.Load(inAddress);

	Output.Store3(outAddress + 0, input.groupID);
	Output.Store3(outAddress + 12, input.groupThreadID);
	Output.Store3(outAddress + 24, input.dispatchThreadID);
	Output.Store(outAddress + 36, input.groupIndex);
	Output.Store(outAddress + 40, (uint)value);
}

technique11 T0
{
	pass P0
	{
		SetVertexShader(NULL);
		SetPixelShader(NULL);
		SetComputeShader(CompileShader(cs_5_0, CS()));
	}
};
