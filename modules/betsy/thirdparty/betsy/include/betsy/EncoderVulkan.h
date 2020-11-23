
#pragma once

#include "betsy/Encoder.h"

namespace betsy
{
	enum PixelFormat
	{
		PFG_RGBA32_UINT,
		PFG_RGBA32_FLOAT,
		PFG_RGBA16_FLOAT,
		PFG_R32_FLOAT,
		PFG_RG32_UINT,
		PFG_RGBA8_UNORM,
		PFG_RGBA8_UNORM_SRGB,
		PFG_RG8_UINT,
		PFG_BC1_UNORM,
		PFG_BC3_UNORM,
		PFG_BC4_UNORM,
		PFG_BC4_SNORM,
		PFG_BC5_UNORM,
		PFG_BC5_SNORM,
		/// BC6H format (unsigned 16 bit float)
		PFG_BC6H_UF16,
		/// ETC1 (Ericsson Texture Compression)
		PFG_ETC1_RGB8_UNORM,
		PFG_ETC2_RGBA8_UNORM,
		PFG_EAC_R11_UNORM,
		PFG_EAC_RG11_UNORM
	};

	namespace ResourceAccess
	{
		/// Enum identifying the texture access privilege
		enum ResourceAccess
		{
			Undefined = 0x00,
			Read = 0x01,
			Write = 0x02,
			ReadWrite = Read | Write
		};
	}  // namespace ResourceAccess

	namespace TextureFlags
	{
		enum TextureFlags
		{
			/// Texture can be used as an RTT (FBO in GL terms)
			RenderToTexture = 1u << 1u,
			/// Texture can be used as an UAV
			Uav = 1u << 2u
		};
	}

	struct TextureParams
	{
		uint32_t width;
		uint32_t height;
		uint32_t depthOrSlices;
		uint8_t  numMipmaps;  /// numMipmaps = 0 is invalid
		uint32_t flags;       /// See TextureFlags::TextureFlags

		PixelFormat format;

		const char *debugName;

		TextureParams( uint32_t _width, uint32_t _height, PixelFormat _format,
		               const char *_debugName = 0, uint32_t _flags = 0u, uint32_t _depthOrSlices = 1u,
		               uint8_t _numMipmaps = 1u )
		{
		}
	};

	struct StagingTexture
	{
		int32_t     bufferName;
		size_t      bytesPerRow;
		uint32_t    width;
		uint32_t    height;
		PixelFormat pixelFormat;
		void *      data;
		size_t      sizeBytes;
		StagingTexture() {}
	};

	struct StagingBuffer
	{
		int32_t bufferName;
		void *  data;
		size_t  sizeBytes;
	};

	struct ComputePso
	{
		int32_t computeShader;
		int32_t computePso;
		ComputePso() {}
	};

	class EncoderVulkan : public Encoder
	{
	public:
		static int32_t get( PixelFormat format ) { return 0; }
		static int32_t getBaseFormat( PixelFormat format ) { return 0; }
		static void    getFormatAndType( PixelFormat pixelFormat, int32_t &format, int32_t &type ) {}

	protected:
		int32_t createTexture( const TextureParams &params ) { return 0; }
		void    destroyTexture( int32_t texName ) {}

		StagingTexture createStagingTexture( uint32_t width, uint32_t height, PixelFormat format,
		                                     bool forUpload )
		{
			StagingTexture staging_texture;
			return staging_texture;
		}
		void uploadStagingTexture( const StagingTexture &stagingTex, int32_t dstTexture ) {}
		void downloadStagingTexture( int32_t srcTexture, const StagingTexture &stagingTex ) {}
		void destroyStagingTexture( const StagingTexture &stagingTex ) {}

		int32_t createUavBuffer( size_t sizeBytes, void *initialData ) { return 0; }
		void    destroyUavBuffer( int32_t bufferName ) {}

		ComputePso createComputePsoFromFile( const char *shaderFilename, const char *relativePath )
		{
			ComputePso compute_pso;
			return compute_pso;
		}
		ComputePso createComputePso( const char *csShader ) { return ComputePso();
		}
		void       destroyPso( ComputePso &pso ) {}
		void bindComputePso( const ComputePso &pso ) {}

		void bindTexture( uint32_t slot, int32_t textureSrv ) {}
		void bindUav( uint32_t slot, int32_t textureSrv, PixelFormat pixelFormat,
		              ResourceAccess::ResourceAccess access )
		{
		}
		void bindUavBuffer( uint32_t slot, int32_t buffer, size_t offset, size_t bufferSize ) {}
	};
}  // namespace betsy
