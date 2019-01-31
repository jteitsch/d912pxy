/*
MIT License

Copyright(c) 2018-2019 megai2

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/
#include "stdafx.h"

d912pxy_vdecl::d912pxy_vdecl(d912pxy_device * dev, const D3DVERTEXELEMENT9 * data) : d912pxy_comhandler(L"vdecl")
{
	m_dev = dev;
	mHash = 0;

	instancedDecl = 0;

	for (int i = 0; i != PXY_INNER_MAX_VDECL_LEN; ++i)
	{
		if (data[i].Stream == 0xFF)//megai2: end marker
		{
			declLen = i + 1;
			declData[i] = D3DDECL_END();
			break;
		}

		declData[i] = data[i];

		declData12[i].InputSlot = data[i].Stream;

		switch (data[i].Usage)
		{
			case D3DDECLUSAGE_POSITION:
				sprintf(semantics[i].s, "POSN%uE", data[i].UsageIndex);
				break;
			case D3DDECLUSAGE_BLENDWEIGHT:
				sprintf(semantics[i].s, "BLWE%uE", data[i].UsageIndex);
				break;
			case D3DDECLUSAGE_BLENDINDICES:
				sprintf(semantics[i].s, "BLIN%uE", data[i].UsageIndex);
				break;
			case D3DDECLUSAGE_NORMAL:
				sprintf(semantics[i].s, "NORM%uE", data[i].UsageIndex);
				break;
			case D3DDECLUSAGE_PSIZE:
				sprintf(semantics[i].s, "PSIZ%uE", data[i].UsageIndex);
				break;
			case D3DDECLUSAGE_TEXCOORD:
				sprintf(semantics[i].s, "TEXC%uE", data[i].UsageIndex);
				break;
			case D3DDECLUSAGE_TANGENT:
				sprintf(semantics[i].s, "TANG%uE", data[i].UsageIndex);
				break;
			case D3DDECLUSAGE_BINORMAL:
				sprintf(semantics[i].s, "BINO%uE", data[i].UsageIndex);//dcl_binormal v5 
				break;
			case D3DDECLUSAGE_TESSFACTOR:
				sprintf(semantics[i].s, "TESF%uE", data[i].UsageIndex);
				break;
			case D3DDECLUSAGE_POSITIONT:
				sprintf(semantics[i].s, "POST%uE", data[i].UsageIndex);
				break;
			case D3DDECLUSAGE_COLOR:
				sprintf(semantics[i].s, "COLR%uE", data[i].UsageIndex);//dcl_color 
				break;
			case D3DDECLUSAGE_FOG:
				sprintf(semantics[i].s, "FOGG%uE", data[i].UsageIndex);
				break;
			case D3DDECLUSAGE_DEPTH:
				sprintf(semantics[i].s, "DEPH%uE", data[i].UsageIndex);
				break;
			case D3DDECLUSAGE_SAMPLE:
				sprintf(semantics[i].s, "SAPL%uE", data[i].UsageIndex);
				break;
			default:
				LOG_ERR_THROW(-1/*vdecl usage unk*/);
		}
		switch (data[i].Type)
		{
			case D3DDECLTYPE_FLOAT1: //0,
				declData12[i].Format = DXGI_FORMAT_R32_FLOAT;
				break;
			case D3DDECLTYPE_FLOAT2: //1,
				declData12[i].Format = DXGI_FORMAT_R32G32_FLOAT;
				break;
			case D3DDECLTYPE_FLOAT3: //2,
				declData12[i].Format = DXGI_FORMAT_R32G32B32_FLOAT;
				break;
			case D3DDECLTYPE_FLOAT4: //3,
				declData12[i].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
				break;
			case D3DDECLTYPE_D3DCOLOR: //4,
				declData12[i].Format = DXGI_FORMAT_B8G8R8A8_UNORM;
				break;
			case D3DDECLTYPE_UBYTE4: //5,
				declData12[i].Format = DXGI_FORMAT_R8G8B8A8_UINT;
				break;
			case D3DDECLTYPE_SHORT2: //6,
				declData12[i].Format = DXGI_FORMAT_R16G16_SINT;
				break;
			case D3DDECLTYPE_SHORT4: //7,
				declData12[i].Format = DXGI_FORMAT_R16G16B16A16_SINT;
				break;
			case D3DDECLTYPE_UBYTE4N: //8,
				declData12[i].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				break;
			case D3DDECLTYPE_SHORT2N: //9,
				declData12[i].Format = DXGI_FORMAT_R16G16_SNORM;
				break;
			case D3DDECLTYPE_SHORT4N: //10,
				declData12[i].Format = DXGI_FORMAT_R16G16B16A16_SNORM;
				break;
			case D3DDECLTYPE_USHORT2N: //11,
				declData12[i].Format = DXGI_FORMAT_R16G16_UNORM;
				break;
			case D3DDECLTYPE_USHORT4N: //12,
				declData12[i].Format = DXGI_FORMAT_R16G16B16A16_UNORM;
				break;

			case D3DDECLTYPE_FLOAT16_2: //15,
				declData12[i].Format = DXGI_FORMAT_R16G16_FLOAT;
				break;
			case D3DDECLTYPE_FLOAT16_4: //16,
				declData12[i].Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
				break;

			case D3DDECLTYPE_UDEC3: //13,								
			case D3DDECLTYPE_DEC3N: //14,
			default:
				LOG_ERR_THROW2(-1, "vdecl type err");
		}
			
	
		declData12[i].SemanticName = semantics[i].s; 		
		declData12[i].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		declData12[i].InstanceDataStepRate = 0;

		if (data[i].Method == D3DDECLMETHOD_PER_VERTEX_CONSTANT)
		{
			declData12[i].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
			declData12[i].InstanceDataStepRate = -1;
		}

		//doubt are here
		declData12[i].AlignedByteOffset = data[i].Offset;
	}

}

d912pxy_vdecl::~d912pxy_vdecl()
{
	if (instancedDecl)
		instancedDecl->Release();
}

D3DVERTEXELEMENT9 * d912pxy_vdecl::GetDeclarationPtr(UINT * pNumElements)
{
	*pNumElements = declLen;

	return declData;
}

void d912pxy_vdecl::ModifyStreamElementType(UINT stream, D3D12_INPUT_CLASSIFICATION newMode)
{
	for (int i = 0; i != declLen; ++i)
	{
		if (declData12[i].InputSlot == stream)
		{
			if (newMode == declData12[i].InputSlotClass)
				break;
			else {
				declData12[i].InputSlotClass = newMode;
				declData12[i].InstanceDataStepRate = 1;
			}
		}
	}
}

d912pxy_vdecl * d912pxy_vdecl::GetInstancedModification()
{
	if (!instancedDecl)
	{
		instancedDecl = new d912pxy_vdecl(m_dev, declData);
	}

	return instancedDecl;
}

UINT32 d912pxy_vdecl::GetHash()
{
	if (!mHash)
	{
		void* mem = declData;
		mHash = d912pxy_memtree2::memHash32s(mem, declLen * sizeof(D3DVERTEXELEMENT9));		
	}
	return mHash;
}

#define D912PXY_METHOD_IMPL_CN d912pxy_vdecl

D912PXY_IUNK_IMPL

D912PXY_METHOD_IMPL(GetDevice)(THIS_ IDirect3DDevice9** ppDevice)
{
	*ppDevice = (IDirect3DDevice9*)m_dev;

	return D3D_OK;
}

D912PXY_METHOD_IMPL(GetDeclaration)(THIS_ D3DVERTEXELEMENT9* pElement, UINT* pNumElements)
{
	*pNumElements = declLen;

	if (pElement)
		for (int i = 0; i != declLen; ++i)
			pElement[i] = declData[i];		

	return D3D_OK;
}

#undef D912PXY_METHOD_IMPL_CN