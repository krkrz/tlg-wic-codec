#include "ddsdecoder.hpp"
#include "../squish/squish.h"

// 67B4FB3B-2125-4E2A-A99E-EAA01B6CD235
const GUID CLSID_DDS_Container = 
{ 0x67B4FB3B, 0x2125, 0x4E2A, { 0xA9, 0x9E, 0xEA, 0xA0, 0x1B, 0x6C, 0xD2, 0x35 } };

// EBF7CAF8-290F-459D-997A-7131AE4ECB1B
const GUID CLSID_DDS_Decoder = 
{ 0xEBF7CAF8, 0x290F, 0x459D, { 0x99, 0x7A, 0x71, 0x31, 0xAE, 0x4E, 0xCB, 0x1B} };

#define WICX_RELEASE(X) if ( NULL != X ) { X->Release(); X = NULL; }

namespace dds
{
	const unsigned int DDSD_CAPS = 0x00000001;
	const unsigned int DDSD_HEIGHT = 0x00000002;
	const unsigned int DDSD_WIDTH = 0x00000004;
	const unsigned int DDSD_PITCH = 0x00000008;
	const unsigned int DDSD_PIXELFORMAT = 0x00001000;
	const unsigned int DDSD_MIPMAPCOUNT = 0x00020000;
	const unsigned int DDSD_LINEARSIZE = 0x00080000;
	const unsigned int DDSD_DEPTH = 0x00800000;

	const unsigned int DDPF_ALPHAPIXELS = 0x00000001;
	const unsigned int DDPF_FOURCC = 0x00000004;
	const unsigned int DDPF_RGB = 0x00000040;
	const unsigned int DDPF_RGBA = 0x00000041;

	const unsigned int DDSCAPS_COMPLEX = 0x00000008;
	const unsigned int DDSCAPS_TEXTURE = 0x00001000;
	const unsigned int DDSCAPS_MIPMAP = 0x00400000;

	const unsigned int DDSCAPS2_CUBEMAP = 0x00000200;
	const unsigned int DDSCAPS2_CUBEMAP_POSITIVEX = 0x00000400;
	const unsigned int DDSCAPS2_CUBEMAP_NEGATIVEX = 0x00000800;
	const unsigned int DDSCAPS2_CUBEMAP_POSITIVEY = 0x00001000;
	const unsigned int DDSCAPS2_CUBEMAP_NEGATIVEY = 0x00002000;
	const unsigned int DDSCAPS2_CUBEMAP_POSITIVEZ = 0x00004000;
	const unsigned int DDSCAPS2_CUBEMAP_NEGATIVEZ = 0x00008000;
	const unsigned int DDSCAPS2_VOLUME = 0x00200000;

	const unsigned int DDPF_FOURCC_DXT1 = 0x31545844;
	const unsigned int DDPF_FOURCC_DXT2 = 0x32545844;
	const unsigned int DDPF_FOURCC_DXT3 = 0x33545844;
	const unsigned int DDPF_FOURCC_DXT4 = 0x34545844;
	const unsigned int DDPF_FOURCC_DXT5 = 0x35545844;

	struct DDS_PIXELFORMAT
	{
		unsigned int dwSize;
		unsigned int dwFlags;
		unsigned int dwFourCC;
		unsigned int dwRGBBitCount;
		unsigned int dwRBitMask;
		unsigned int dwGBitMask;
		unsigned int dwBBitMask;
		unsigned int dwRGBAlphaBitMask;
	};

	struct DDS_CAPS
	{
		unsigned int dwCaps1;
		unsigned int dwCaps2;
		unsigned int Reserved[2];
	};

	struct DDS_HEADER
	{
		unsigned int dwMagic;

		unsigned int dwSize;
		unsigned int dwFlags;
		unsigned int dwHeight;
		unsigned int dwWidth;
		unsigned int dwPitchOrLinearSize;
		unsigned int dwDepth;
		unsigned int dwMipMapCount;

		unsigned int dwReserved1[11];

		DDS_PIXELFORMAT ddpfPixelFormat;
		DDS_CAPS ddsCaps;

		unsigned int dwReserved2;
	};
}

namespace ddsx
{
	template< typename T >
	static HRESULT ReadFromIStream( IStream *stream, T &val )
	{
		HRESULT result = S_OK;
		if ( NULL == stream ) result = E_INVALIDARG;
		ULONG numRead = 0;
		if ( SUCCEEDED( result )) result = stream->Read( &val, sizeof(T), &numRead );
		if ( SUCCEEDED( result )) result = ( sizeof(T) == numRead ) ? S_OK : E_UNEXPECTED;
		return result;
	}

	static HRESULT ReadBytesFromIStream( IStream *stream, unsigned char *bytes, unsigned count )
	{
		HRESULT result = S_OK;
		if ( NULL == stream ) result = E_INVALIDARG;
		ULONG numRead = 0;
		if ( SUCCEEDED( result )) result = stream->Read( bytes, count, &numRead );
		if ( SUCCEEDED( result )) result = ( count == numRead ) ? S_OK : E_UNEXPECTED;
		return result;
	}

	// Convert component @a c having @a inbits to the returned value having @a outbits.
	// Source: nvidia-texture-tools
	inline unsigned PixelConvert( unsigned c, unsigned inbits, unsigned outbits )
	{
		if ( inbits == 0 )
		{
			return 0;
		}
		else if (inbits >= outbits)
		{
			// truncate
			return c >> (inbits - outbits);
		}
		else
		{
			// bitexpand
			return (c << (outbits - inbits)) | PixelConvert(c, inbits, outbits - inbits);
		}
	}

	// Get pixel component shift and size given its mask.
	// Source: nvidia-texture-tools
	inline void MaskShiftAndSize( unsigned mask, unsigned *shift, unsigned *size )
	{
		if ( !mask ) { *shift = 0; *size = 0; return; }

		*shift = 0;
		while(( mask & 1 ) == 0 ) { ++(*shift); mask >>= 1; }

		*size = 0;
		while(( mask & 1 ) == 1 ) { ++(*size); mask >>= 1; }
	}

	//----------------------------------------------------------------------------------------
	// DDS_FrameDecode implementation
	//----------------------------------------------------------------------------------------

	DDS_FrameDecode::DDS_FrameDecode( IWICImagingFactory *pIFactory, UINT num )
		: BaseFrameDecode( pIFactory, num )
	{
	}

	HRESULT DDS_FrameDecode::Load_DXT_Image( dds::DDS_HEADER &ddsHeader, IStream *pIStream )
	{
		HRESULT result = S_OK;

		unsigned int squishFlags = 0;

		switch ( ddsHeader.ddpfPixelFormat.dwFourCC )
		{
			case dds::DDPF_FOURCC_DXT1:
				squishFlags = squish::kDxt1;
				break;
			case dds::DDPF_FOURCC_DXT3:
				squishFlags = squish::kDxt3;
				break;
			case dds::DDPF_FOURCC_DXT5:
				squishFlags = squish::kDxt5;
				break;
			default:
				result = E_INVALIDARG;
		}

		if ( SUCCEEDED( result ))
		{
			unsigned int width = ddsHeader.dwWidth;
			unsigned int height = ddsHeader.dwHeight;

			int blockCount = ( ( width + 3 )/4 ) * ( ( height + 3 )/4 );
			int blockSize = ( ( squishFlags & squish::kDxt1 ) != 0 ) ? 8 : 16;

			unsigned char *compressedBlocks = new unsigned char[ blockCount*blockSize ];
			result = ReadBytesFromIStream( pIStream, compressedBlocks, blockCount*blockSize );

			unsigned char *data = new unsigned char[ width*height*4 ];

			squish::DecompressImage( data, width, height, compressedBlocks, squishFlags );

			result = FillBitmapSource( width, height, 72, 72, GUID_WICPixelFormat32bpp3ChannelsAlpha, width*4,
				width*4*height, data );

			delete[] data;
			delete[] compressedBlocks;
		}

		return result;
	}

	HRESULT DDS_FrameDecode::LoadLinearImage( dds::DDS_HEADER &ddsHeader, IStream *pIStream )
	{
		struct rgba
		{
			unsigned char r, g, b, a;
		};

		HRESULT result = S_OK;

		unsigned int width = ddsHeader.dwWidth;
		unsigned int height = ddsHeader.dwHeight;

		unsigned rshift, rsize, gshift, gsize, bshift, bsize, ashift, asize;

		MaskShiftAndSize( ddsHeader.ddpfPixelFormat.dwRBitMask, &rshift, &rsize );
		MaskShiftAndSize( ddsHeader.ddpfPixelFormat.dwGBitMask, &gshift, &gsize );
		MaskShiftAndSize( ddsHeader.ddpfPixelFormat.dwBBitMask, &bshift, &bsize );
		MaskShiftAndSize( ddsHeader.ddpfPixelFormat.dwRGBAlphaBitMask, &ashift, &asize );

		unsigned byteCount = (ddsHeader.ddpfPixelFormat.dwRGBBitCount + 7) / 8;

		unsigned char *inData = new unsigned char[ width*byteCount*height ];

		result = ReadBytesFromIStream( pIStream, inData, width*byteCount*height );

		if ( SUCCEEDED( result ))
		{
			unsigned char *outData = new unsigned char[ width*height*4 ];
			unsigned char *inDataP = inData, *outDataP = outData;

			for ( unsigned y = 0; y < height; y++ )
			{
				for ( unsigned x = 0; x < width; x++ )
				{
					unsigned c = *reinterpret_cast<unsigned *>(inDataP);
					rgba *cc = reinterpret_cast<rgba *>(outDataP);
					cc->r = (unsigned char)PixelConvert( (c & ddsHeader.ddpfPixelFormat.dwRBitMask) >> rshift, rsize, 8 );
					cc->g = (unsigned char)PixelConvert( (c & ddsHeader.ddpfPixelFormat.dwGBitMask) >> gshift, gsize, 8 );
					cc->b = (unsigned char)PixelConvert( (c & ddsHeader.ddpfPixelFormat.dwBBitMask) >> bshift, bsize, 8 );

					if ( ddsHeader.ddpfPixelFormat.dwRGBAlphaBitMask )
						cc->a = (unsigned char)PixelConvert( (c & ddsHeader.ddpfPixelFormat.dwRGBAlphaBitMask) >> ashift, asize, 8 );
					else
						cc->a = 255;

					inDataP += byteCount; outDataP += 4;
				}
			}

			result = FillBitmapSource( width, height, 72, 72, GUID_WICPixelFormat32bpp3ChannelsAlpha, width*4, width*4*height, outData );

			delete[] outData;
		}

		delete[] inData;

		return result;
	}

	HRESULT DDS_FrameDecode::FillBitmapSource( UINT width, UINT height, UINT dpiX, UINT dpiY,
		REFWICPixelFormatGUID pixelFormat, UINT cbStride, UINT cbBufferSize, BYTE *pbBuffer )
	{
		UNREFERENCED_PARAMETER( dpiX );
		UNREFERENCED_PARAMETER( dpiY );

		HRESULT result = S_OK;
		IWICImagingFactory *codecFactory = NULL;

		if ( SUCCEEDED( result ))
			result = CoCreateInstance( CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory,
				reinterpret_cast<LPVOID *>( &codecFactory ));

		if ( SUCCEEDED( result ))
			result = codecFactory->CreateBitmapFromMemory ( width, height, pixelFormat, cbStride, cbBufferSize,
				pbBuffer, reinterpret_cast<IWICBitmap **>( &( m_bitmapSource )));

		if ( codecFactory ) codecFactory->Release();

		return result;
	}


	//----------------------------------------------------------------------------------------
	// DDS_Decoder implementation
	//----------------------------------------------------------------------------------------

	DDS_Decoder::DDS_Decoder()
		: BaseDecoder( CLSID_DDS_Decoder, CLSID_DDS_Container )
	{
	}

	DDS_Decoder::~DDS_Decoder()
	{
	}

	// TODO: implement real query capability
	STDMETHODIMP DDS_Decoder::QueryCapability( IStream *pIStream, DWORD *pCapability )
	{
		UNREFERENCED_PARAMETER( pIStream );

		HRESULT result = S_OK;

		*pCapability =
			WICBitmapDecoderCapabilityCanDecodeSomeImages;

		return result;
	}

	STDMETHODIMP DDS_Decoder::Initialize( IStream *pIStream, WICDecodeOptions cacheOptions )
	{
		UNREFERENCED_PARAMETER( cacheOptions );

		HRESULT result = E_INVALIDARG;

		ReleaseMembers( true );

		if ( pIStream )
		{
			dds::DDS_HEADER ddsHeader;
			result = ReadFromIStream( pIStream, ddsHeader ); // read header

			if ( SUCCEEDED( result )) result = VerifyFactory();

			if ( SUCCEEDED( result ))
			{
				DDS_FrameDecode* frame = CreateNewDecoderFrame( m_factory, 0 ); 

				if ( ddsHeader.ddpfPixelFormat.dwFlags & dds::DDPF_FOURCC )
					result = frame->Load_DXT_Image( ddsHeader, pIStream );
				else
					result = frame->LoadLinearImage( ddsHeader, pIStream );

				if ( SUCCEEDED( result ))
					AddDecoderFrame( frame );
				else
					delete frame;
			}
		}
		else
			result = E_INVALIDARG;

		return result;
	}

	DDS_FrameDecode* DDS_Decoder::CreateNewDecoderFrame( IWICImagingFactory* factory , UINT i )
	{
		return new DDS_FrameDecode( factory, i );
	}

	void DDS_Decoder::Register( RegMan &regMan )
	{
		HMODULE curModule = GetModuleHandle( L"dds-wic-codec.dll" );
		wchar_t tempFileName[MAX_PATH];
		if ( curModule != NULL ) GetModuleFileName( curModule, tempFileName, MAX_PATH );

		regMan.SetSZ( L"CLSID\\{7ED96837-96F0-4812-B211-F13C24117ED3}\\Instance\\{EBF7CAF8-290F-459D-997A-7131AE4ECB1B}", L"CLSID", L"{EBF7CAF8-290F-459D-997A-7131AE4ECB1B}" );
		regMan.SetSZ( L"CLSID\\{7ED96837-96F0-4812-B211-F13C24117ED3}\\Instance\\{EBF7CAF8-290F-459D-997A-7131AE4ECB1B}", L"FriendlyName", L"DDS Decoder" );
		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-459D-997A-7131AE4ECB1B}", L"Version", L"1.0.0.1" );
		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-459D-997A-7131AE4ECB1B}", L"Date", _T(__DATE__) );
		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-459D-997A-7131AE4ECB1B}", L"SpecVersion", L"1.0.0.0" );
		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-459D-997A-7131AE4ECB1B}", L"ColorManagementVersion", L"1.0.0.0" );
		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-459D-997A-7131AE4ECB1B}", L"MimeTypes", L"x-image/dds" );
		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-459D-997A-7131AE4ECB1B}", L"FileExtensions", L".dds" );
		regMan.SetDW( L"CLSID\\{EBF7CAF8-290F-459D-997A-7131AE4ECB1B}", L"SupportsAnimation", 0 );
		regMan.SetDW( L"CLSID\\{EBF7CAF8-290F-459D-997A-7131AE4ECB1B}", L"SupportChromakey", 1 );
		regMan.SetDW( L"CLSID\\{EBF7CAF8-290F-459D-997A-7131AE4ECB1B}", L"SupportLossless", 1 );
		regMan.SetDW( L"CLSID\\{EBF7CAF8-290F-459D-997A-7131AE4ECB1B}", L"SupportMultiframe", 1 );
		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-459D-997A-7131AE4ECB1B}", L"ContainerFormat", L"{67B4FB3B-2125-4E2A-A99E-EAA01B6CD235}" );
		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-459D-997A-7131AE4ECB1B}", L"Author", L"Ilya Zaytsev, r2vb.com" );
		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-459D-997A-7131AE4ECB1B}", L"Description", L"DirectDraw Surface Format Decoder" );
		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-459D-997A-7131AE4ECB1B}", L"FriendlyName", L"DDS Decoder" );

		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-459D-997A-7131AE4ECB1B}\\Formats", L"", L"" );
		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-459D-997A-7131AE4ECB1B}\\Formats\\{6FDDC324-4E03-4BFE-B185-3D77768DC90F}", L"", L"" );

		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-459D-997A-7131AE4ECB1B}\\InprocServer32", L"", tempFileName );
		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-459D-997A-7131AE4ECB1B}\\InprocServer32", L"ThreadingModel", L"Apartment" );
		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-459D-997A-7131AE4ECB1B}\\Patterns", L"", L"" );
		regMan.SetDW( L"CLSID\\{EBF7CAF8-290F-459D-997A-7131AE4ECB1B}\\Patterns\\0", L"Position", 0 );
		regMan.SetDW( L"CLSID\\{EBF7CAF8-290F-459D-997A-7131AE4ECB1B}\\Patterns\\0", L"Length", 4 );

		BYTE bytes[8] = { 0 };
		bytes[0] = 0x44; bytes[1] = 0x44; bytes[2] = 0x53; bytes[3] = 0x20;
		regMan.SetBytes( L"CLSID\\{EBF7CAF8-290F-459D-997A-7131AE4ECB1B}\\Patterns\\0", L"Pattern", bytes, 4 );
		bytes[0] = bytes[1] = bytes[2] = bytes[3] = 0xFF;
		regMan.SetBytes( L"CLSID\\{EBF7CAF8-290F-459D-997A-7131AE4ECB1B}\\Patterns\\0", L"Mask", bytes, 4 );
	}
}
