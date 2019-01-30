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

d912pxy_shader_replacer::d912pxy_shader_replacer(DWORD * fun, UINT len, d912pxy_shader_uid UID) : d912pxy_noncom(NULL, L"shader replacer")
{
	mUID = UID;
	oCode = fun;
	oLen = len;

	if (fun)
		vsSig = CheckTypeSignature();
}

d912pxy_shader_replacer::~d912pxy_shader_replacer()
{

}

d912pxy_shader_code d912pxy_shader_replacer::CompileFromHLSL_CS(const wchar_t* bfolder)
{
	wchar_t replFn[1024];

	//megai2: %016llX bugged out
	wsprintf(replFn, L"%s/%08lX%08lX.hlsl", bfolder, mUID >> 32, mUID & 0xFFFFFFFF);

	char targetCompiler[] = "cs_5_1";

	ComPtr<ID3DBlob> ret, eret;

#ifdef _DEBUG
	HRESULT compRet = D3DCompileFromFile(replFn, 0, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", targetCompiler, D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES | D3DCOMPILE_DEBUG, 0, &ret, &eret);
#else
	HRESULT compRet = D3DCompileFromFile(replFn, 0, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", targetCompiler, D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0, &ret, &eret);
#endif

	if ((compRet != S_OK) && (eret == NULL))
	{
		

		d912pxy_shader_code ret2;

		ret2.code = 0;
		ret2.sz = 0;
		ret2.blob = nullptr;

		return ret2;

	}
	else if (eret != NULL && ret == NULL)
	{
		

		d912pxy_shader_code ret2;

		ret2.code = 0;
		ret2.sz = 0;
		ret2.blob = nullptr;

		return ret2;
	}
	else {

		if (eret != NULL)
		{
			
		}

		d912pxy_shader_code ret2;

		ret2.code = ret->GetBufferPointer();
		ret2.sz = ret->GetBufferSize();
		ret2.blob = ret;
		
		return ret2;
	}
}

d912pxy_shader_code d912pxy_shader_replacer::CompileFromHLSL(const wchar_t* bfolder)
{
	wchar_t replFn[1024];

	//megai2: %016llX bugged out
	wsprintf(replFn, L"%s/%08lX%08lX.hlsl", bfolder, mUID >> 32, mUID & 0xFFFFFFFF);

	char targetCompiler[] = "ps_5_1";

	if (vsSig)
		targetCompiler[0] = L'v';

	ComPtr<ID3DBlob> ret, eret;
	
#ifdef _DEBUG
	HRESULT compRet = D3DCompileFromFile(replFn, 0, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", targetCompiler, D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES | D3DCOMPILE_DEBUG, 0, &ret, &eret);
#else
 	HRESULT compRet = D3DCompileFromFile(replFn, 0, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", targetCompiler, D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0, &ret, &eret);
#endif

	if ((compRet != S_OK) && (eret == NULL))
	{
		

		d912pxy_shader_code ret2;

		ret2.code = 0;
		ret2.sz = 0;
		ret2.blob = nullptr;
		
		return ret2;

	} else if (eret != NULL && ret == NULL)
	{
		

		d912pxy_shader_code ret2;

		ret2.code = 0;
		ret2.sz = 0;
		ret2.blob = nullptr;

		return ret2;
	}
	else {

		if (eret != NULL)
		{
			
		}

		d912pxy_shader_code ret2;

		ret2.code = ret->GetBufferPointer();
		ret2.sz = ret->GetBufferSize();
		ret2.blob = ret;

		DeleteFile(replFn);

		return ret2;
	}
}

d912pxy_shader_code d912pxy_shader_replacer::LoadFromCSO(const char* bfolder)
{
	d912pxy_shader_code ret;

	ret.code = 0;
	ret.sz = 0;
	ret.blob = nullptr;

	char replFn[1024];
	sprintf(replFn, "%s/%08lX%08lX.cso", bfolder, (UINT32)(mUID >> 32), (UINT32)(mUID & 0xFFFFFFFF));
		
	ret.code = d912pxy_s(vfs)->LoadFile(replFn, (UINT*)&ret.sz, PXY_VFS_BID_CSO);

	/*FILE* f = _wfopen(replFn, L"rb");

	if (f)
	{
		fseek(f, 0, SEEK_END);
		size_t fsz = ftell(f);
		fseek(f, 0, SEEK_SET);

		if (fsz)
		{
			void* otherData = malloc(fsz);

			fread((void*)otherData, 1, fsz, f);

			fclose(f);

			ret.code = otherData;
			ret.sz = fsz;
		} else
			fclose(f);
	}*/

	return ret;

}

void d912pxy_shader_replacer::SaveCSO(d912pxy_shader_code code, const char * bfolder)
{
	char replFn[1024];
	sprintf(replFn, "%s/%08lX%08lX.cso", bfolder, (UINT32)(mUID >> 32), (UINT32)(mUID & 0xFFFFFFFF));

	d912pxy_s(vfs)->WriteFile(replFn, code.code, (UINT32)code.sz, PXY_VFS_BID_CSO);

	/*FILE* f = _wfopen(replFn, L"wb");

	if (f)
	{
		fwrite(code.code, 1, code.sz, f);
		fflush(f);
		fclose(f);
	}*/
}

void d912pxy_shader_replacer::GenerateHLSL(const wchar_t * bfolder)
{
	wchar_t replFn[4096];
	wsprintf(replFn, L"%s/%08lX%08lX.hlsl", bfolder, (UINT32)(mUID >> 32), (UINT32)(mUID & 0xFFFFFFFF));

	d912pxy_hlsl_generator* gen = new d912pxy_hlsl_generator(oCode, oLen, replFn, mUID);
	gen->Process();
	/*
	wchar_t replFn2[4096];
	wsprintf(replFn2, L"%s_v/%08lX%08lX.vf", bfolder, mUID >> 32, mUID & 0xFFFFFFFF);
	FILE* f = _wfopen(replFn2, L"wb");

	UINT32 maxVars = gen->GetMaxShaderPassedVars();

	if (f)
	{		
		fwrite(&maxVars, 4, 1, f);		
		fflush(f);
		fclose(f);
	}
	
*/

	delete gen;
}

d912pxy_shader_code d912pxy_shader_replacer::GetCode()
{
	d912pxy_shader_code ret = LoadFromCSO(d912pxy_shader_db_cso_dir);

	if (!ret.code)
	{
		ret = CompileFromHLSL(d912pxy_shader_db_hlsl_dir);
		if (!ret.code)
		{
			GenerateHLSL(d912pxy_shader_db_hlsl_dir);
			ret = CompileFromHLSL(d912pxy_shader_db_hlsl_dir);
			if (!ret.code)
			{
				
				
				for (UINT i = 0; i != oLen; ++i)
				{
					
				}
				LOG_ERR_THROW2(-1, "shader replace error");
			}
			else {
				SaveCSO(ret, d912pxy_shader_db_cso_dir);
			}
		} else 
			SaveCSO(ret, d912pxy_shader_db_cso_dir);
	}

	return ret;
}

d912pxy_shader_code d912pxy_shader_replacer::GetCodeCS()
{
	d912pxy_shader_code ret = LoadFromCSO(d912pxy_cs_cso_dir);

	if (!ret.code)
	{
		ret = CompileFromHLSL_CS(d912pxy_cs_hlsl_dir);
		if (!ret.code)
			LOG_ERR_THROW2(-1, "cs code error");
		else
			SaveCSO(ret, d912pxy_cs_cso_dir);
	}

	return ret;
}

UINT d912pxy_shader_replacer::GetMaxVars()
{
/*	wchar_t replFn[1024];
	wsprintf(replFn, L"%s_v/%08lX%08lX.vf", d912pxy_shader_db_hlsl_dir, mUID >> 32, mUID & 0xFFFFFFFF);

	FILE* f = _wfopen(replFn, L"rb");

	UINT maxVars = 256;

	if (f)
	{		
		fread(&maxVars, 4, 1, f);
		fclose(f);

		maxVars = ((((maxVars & 0xF) != 0) * 0x10) + (maxVars & (~0xF)));

		if (maxVars > 256)
			maxVars = 256;
		
	}
		
	return maxVars*16;*/
	return 4096;
}

UINT d912pxy_shader_replacer::CheckTypeSignature()
{
	//Each individual shader code is formatted with a general token layout. The first token must be a version token. 
	//[31:16] Bits 16 through 31 specify whether the code is for a pixel or vertex shader. For a pixel shader, the value is 0xFFFF. For a vertex shader, the value is 0xFFFE.
	
	return ((oCode[0] & (1 << 16)) == 0);
}