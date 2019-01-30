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
#pragma once

#define PXY_INNER_MAX_SWAP_CHAINS 4
#define PXY_INNER_MAX_RENDER_TARGETS 8
#define PXY_INNER_MAX_TEXTURE_STAGES 32
#define PXY_INNER_MAX_API_SAMPLERS 512
#define PXY_INNER_MAX_SHADER_SAMPLERS 32
#define PXY_INNER_MAX_SHADER_SAMPLERS_SZ 1664
#define PXY_INNER_MAX_SHADER_CONSTS_IDX 256
#define PXY_INNER_MAX_SHADER_CONSTS 256*4
#define PXY_INNER_MAX_VBUF_STREAMS 10
#define PXY_INNER_MAX_IFRAME_CLEANUPS 1024*1024
#define PXY_INNER_MAX_CLEANUPS_PER_SYNC 128
#define PXY_INNER_MAX_IFRAME_BATCH_COUNT 1024*8
#define PXY_INNER_MAX_IFRAME_BATCH_REPLAY PXY_INNER_MAX_IFRAME_BATCH_COUNT*100
#define PXY_INNER_MAX_IFRAME_PSO_CACHES 4096
#define PXY_INNER_MAX_CACHE_NODES_SAMPLERS 4096
#define PXY_INNER_MAX_TEXSTATE_CACHE_NODES 0xFFFFF
#define PXY_INNER_MAX_PSO_CACHE_NODES 0xFFFF
#define PXY_INNER_MAX_DHEAP_CLEANUP_PER_SYNC 16
#define PXY_INNER_MAX_ASYNC_TEXLOADS 5120
#define PXY_INNER_MAX_ASYNC_BUFFERLOADS 5120
#define PXY_INNER_UPLOAD_POOL_RESERVE (1ULL << 29)
#define PXY_INNER_VSTREAM_POOL_RESERVE (1ULL << 29)
#define PXY_INNER_SURFACE_POOL_RESERVE 0

#define PXY_INNER_TST_SAMPLER_OFFSET 32
#define PXY_INNER_BATCH_CONSTANT_OFFSET_IDX 16
#define PXY_INNER_BATCH_CONSTANT_OFFSET 256
//this is 256 + shaderConsts*4*4*2
#define PXY_INNER_BATCH_VAR_SZ_VS_MAX 4096
#define PXY_INNER_BATCH_VAR_SZ_PS_MAX 4096
#define PXY_INNER_BATCH_VAR_SZ 8192
#define PXY_INNER_BATCH_BUFSZ 8448
#define PXY_INNER_BATCH_BUFSZ_D64 132
#define PXY_INNER_BATCH_BUFSZ_D32 264
#define PXY_INNER_BATCH_BUFSZ_D16 528
#define PXY_INNER_BATCH_BUFSZ_D256 33

//megai2: hold pooled objects alive for 10s
#define PXY_INNER_GC_LIFETIME 10000
#define PXY_INNER_SHADER_PAIR_LIFETIME 10000

#define PXY_INNER_MAX_DSC_HEAPS 4
#define PXY_INNER_HEAP_RTV 0
#define PXY_INNER_HEAP_DSV 1
#define PXY_INNER_HEAP_SRV 2
#define PXY_INNER_HEAP_CBV 2
#define PXY_INNER_HEAP_SPL 3

#define PXY_INNER_THREADID_TEX_LOADER 0
#define PXY_INNER_THREADID_BUF_LOADER 1
#define PXY_INNER_THREADID_RPL_THRD0 2
//#define PXY_INNER_THREADID_PSO_COMPILER 2
#define PXY_INNER_THREADID_MAX 3

#define PXY_INNER_REPLAY_THREADS 1

#define PXY_INNER_SHDR_BUG_PCF_SAMPLER 0
#define PXY_INNER_SHDR_BUG_ALPHA_TEST 1
#define PXY_INNER_SHDR_BUG_SRGB_READ 2
#define PXY_INNER_SHDR_BUG_SRGB_WRITE 3
#define PXY_INNER_SHDR_BUG_CLIPPLANE0 4
#define PXY_INNER_SHDR_BUG_RESERVED0 5
#define PXY_INNER_SHDR_BUG_RESERVED1 6
#define PXY_INNER_SHDR_BUG_RESERVED2 7
#define PXY_INNER_SHDR_BUG_RESERVED3 8
#define PXY_INNER_SHDR_BUG_COUNT 9
#define PXY_INNER_SHDR_BUG_FILE_SIZE PXY_INNER_SHDR_BUG_COUNT * 4

//#define PERFORMANCE_GRAPH_WRITE_DX9
//#define PERFORMANCE_GRAPH_WRITE
//#define PER_BATCH_FLUSH_DEBUG 1

#ifdef PERFORMANCE_GRAPH_WRITE_DX9
	#define PERFORMANCE_GRAPH_WRITE
#endif

#ifdef _DEBUG
	//#define TRACK_SHADER_BUGS_PROFILE
	#define FRAME_METRICS
#endif

#ifdef FRAME_METRICS
	#define FRAME_METRIC_CLEANUPS
	#define FRAME_METRIC_DHEAP
	#define FRAME_METRIC_SYNC
	#define FRAME_METRIC_PRESENT
	#define FRAME_METRIC_API_OVERHEAD
#endif

#ifdef FRAME_METRIC_API_OVERHEAD
	#define API_OVERHEAD_TRACK_START(a) UINT64 apioh_local_start = 0; QueryPerformanceCounter((LARGE_INTEGER*)&apioh_local_start);
	#define API_OVERHEAD_TRACK_END(a) UINT64 apioh_local_end = 0; QueryPerformanceCounter((LARGE_INTEGER*)&apioh_local_end); d912pxy_device::apiOverhead += apioh_local_end - apioh_local_start;
#else 
	#define API_OVERHEAD_TRACK_START(a)
	#define API_OVERHEAD_TRACK_END(a)
#endif

#define UPLOAD_POOL_USE_AND_DISCARD 

#define DX9_FRAME_SHADER_STATE_PERSISTENCY_GPUGPU_COPY
//#define DX9_FRAME_SHADER_STATE_PERSISTENCY_CPU_TRACKING

#define d912pxy_shader_uid UINT64


#ifdef _DEBUG
#define ENABLE_DEBUG_LOGGING
#else 
//#define ENABLE_DEBUG_LOGGING
#endif

//#define TRACK_SHADER_BUGS_PROFILE

#define LOG_ERR_THROW(hr) LOG_ERR_THROW2(hr, hr)
#define LOG_ERR_THROW2(hr, hr2) ThrowErrorDbg(hr, #hr2 )

#define D912PXY_METHOD(meth) HRESULT WINAPI meth
#define D912PXY_METHOD_(a,b) a WINAPI b

#define D912PXY_METHOD_IMPL_(a,b) a D912PXY_METHOD_IMPL_CN :: b
#define D912PXY_METHOD_IMPL(a) HRESULT D912PXY_METHOD_IMPL_CN :: a

#define d912pxy_cs_hlsl_dir L"./d912pxy/shaders/cs"
#define d912pxy_cs_cso_dir "cs/cso"
#define d912pxy_shader_db_hlsl_dir L"./d912pxy/shaders/hlsl"
#define d912pxy_shader_db_cso_dir "shaders/cso"
#define d912pxy_shader_db_bugs_dir "shaders/bugs"

class d912pxy_vbuf;
class d912pxy_ibuf;
class d912pxy_basetexture;
class d912pxy_vtexture;
class d912pxy_texture;
class d912pxy_ctexture;
class d912pxy_surface;
class d912pxy_device;
class d912pxy_dheap;
class d912pxy_vdecl;
class d912pxy_gpu_cmd_list;
class d912pxy_replay;
class d912pxy_batch;
class d912pxy_shader_db;
class d912pxy_shader_pair;
class d912pxy_iframe;
class d912pxy_upload_pool;
class d912pxy_upload_item;
class d912pxy_vstream_pool;
class d912pxy_surface_pool;
class d912pxy_cleanup_thread;
class d912pxy_gpu_que;
class d912pxy_gpu_cmd_list;
class d912pxy_texstage_cache;
class d912pxy_surface_layer;
class d912pxy_texture_loader;
class d912pxy_buffer_loader;
class d912pxy_sampler_cache;
class d912pxy_pso_cache;
class d912pxy_batch;
class d912pxy_pso_cache_item;
class d912pxy_vfs;
struct d912pxy_trimmed_dx12_pso;

typedef struct d912pxy_device_texture_state {
	UINT32 dirty;
	UINT texHeapID[PXY_INNER_MAX_TEXTURE_STAGES];
	UINT splHeapID[PXY_INNER_MAX_SHADER_SAMPLERS];
} d912pxy_device_texture_state;

typedef struct d912pxy_device_streamsrc {
	d912pxy_vbuf* buffer;
	UINT offset;
	UINT stride;
	UINT divider;
} d912pxy_device_streamsrc;

typedef enum d912pxy_gpu_cmd_list_group {
	CLG_TOP = 0,
	CLG_TEX = 1,
	CLG_BUF = 2,
	CLG_RP1 = 3,
	/*CLG_RP2 = 4,
	CLG_RP3 = 5,
	CLG_RP4 = 6,*/
	CLG_SEQ = 4
} d912pxy_gpu_cmd_list_group;

#define PXY_INNER_MAX_GPU_CMD_LIST_GROUPS 5

class d912pxy_global_objects {
public:
	d912pxy_global_objects() {  };
	~d912pxy_global_objects() {  };

	static d912pxy_shader_db* sdb;
	static d912pxy_vstream_pool* pool_vstream;
	static d912pxy_surface_pool* pool_surface;
	static d912pxy_upload_pool* pool_upload;
	static d912pxy_cleanup_thread* thread_cleanup;
	static d912pxy_iframe* iframe;
	static d912pxy_gpu_que* GPUque;
	static d912pxy_gpu_cmd_list* GPUcl;
	static ID3D12Device* DXDev;
	static d912pxy_replay* CMDReplay;
	static d912pxy_texstage_cache* textureState;
	static d912pxy_sampler_cache* samplerState;
	static d912pxy_pso_cache* psoCache;
	static d912pxy_texture_loader* texloadThread;
	static d912pxy_buffer_loader* bufloadThread;
	static d912pxy_batch* batch;	
	static d912pxy_vfs* vfs;
	static d912pxy_device* dev;
};

#define d912pxy_s(a) d912pxy_global_objects::a
#define d912pxy_s_dcl(a,t) t* d912pxy_global_objects::a = 0;
