#include "tlgdecoder.hpp"
#include "../squish/squish.h"

// {280BF6EC-0A7B-4870-8AB0-FC4DE12D0B7B}
const GUID CLSID_TLG_Container = 
{ 0x280bf6ec, 0xa7b, 0x4870, { 0x8a, 0xb0, 0xfc, 0x4d, 0xe1, 0x2d, 0xb, 0x7b } };

// {05103AD4-28F3-4229-A9A3-2928A8CE5E9A}
const GUID CLSID_TLG_Decoder = 
{ 0x5103ad4, 0x28f3, 0x4229, { 0xa9, 0xa3, 0x29, 0x28, 0xa8, 0xce, 0x5e, 0x9a } };


#define WICX_RELEASE(X) if ( NULL != X ) { X->Release(); X = NULL; }

namespace tlg
{
	const unsigned int TLGD_CAPS = 0x00000001;
	const unsigned int TLGD_HEIGHT = 0x00000002;
	const unsigned int TLGD_WIDTH = 0x00000004;
	const unsigned int TLGD_PITCH = 0x00000008;
	const unsigned int TLGD_PIXELFORMAT = 0x00001000;
	const unsigned int TLGD_MIPMAPCOUNT = 0x00020000;
	const unsigned int TLGD_LINEARSIZE = 0x00080000;
	const unsigned int TLGD_DEPTH = 0x00800000;

	const unsigned int DDPF_ALPHAPIXELS = 0x00000001;
	const unsigned int DDPF_FOURCC = 0x00000004;
	const unsigned int DDPF_RGB = 0x00000040;
	const unsigned int DDPF_RGBA = 0x00000041;

	const unsigned int TLGCAPS_COMPLEX = 0x00000008;
	const unsigned int TLGCAPS_TEXTURE = 0x00001000;
	const unsigned int TLGCAPS_MIPMAP = 0x00400000;

	const unsigned int TLGCAPS2_CUBEMAP = 0x00000200;
	const unsigned int TLGCAPS2_CUBEMAP_POSITIVEX = 0x00000400;
	const unsigned int TLGCAPS2_CUBEMAP_NEGATIVEX = 0x00000800;
	const unsigned int TLGCAPS2_CUBEMAP_POSITIVEY = 0x00001000;
	const unsigned int TLGCAPS2_CUBEMAP_NEGATIVEY = 0x00002000;
	const unsigned int TLGCAPS2_CUBEMAP_POSITIVEZ = 0x00004000;
	const unsigned int TLGCAPS2_CUBEMAP_NEGATIVEZ = 0x00008000;
	const unsigned int TLGCAPS2_VOLUME = 0x00200000;

	const unsigned int DDPF_FOURCC_DXT1 = 0x31545844;
	const unsigned int DDPF_FOURCC_DXT2 = 0x32545844;
	const unsigned int DDPF_FOURCC_DXT3 = 0x33545844;
	const unsigned int DDPF_FOURCC_DXT4 = 0x34545844;
	const unsigned int DDPF_FOURCC_DXT5 = 0x35545844;

	struct TLG_PIXELFORMAT
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

	struct TLG_CAPS
	{
		unsigned int dwCaps1;
		unsigned int dwCaps2;
		unsigned int Reserved[2];
	};

	struct TLG_HEADER
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

		TLG_PIXELFORMAT ddpfPixelFormat;
		TLG_CAPS tlgCaps;

		unsigned int dwReserved2;
	};
}

namespace tlgx
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
	// TLG_FrameDecode implementation
	//----------------------------------------------------------------------------------------

	TLG_FrameDecode::TLG_FrameDecode( IWICImagingFactory *pIFactory, UINT num )
		: BaseFrameDecode( pIFactory, num )
	{
	}

	HRESULT TLG_FrameDecode::Load_DXT_Image( tlg::TLG_HEADER &tlgHeader, IStream *pIStream )
	{
		HRESULT result = S_OK;

		unsigned int squishFlags = 0;

		switch ( tlgHeader.ddpfPixelFormat.dwFourCC )
		{
			case tlg::DDPF_FOURCC_DXT1:
				squishFlags = squish::kDxt1;
				break;
			case tlg::DDPF_FOURCC_DXT3:
				squishFlags = squish::kDxt3;
				break;
			case tlg::DDPF_FOURCC_DXT5:
				squishFlags = squish::kDxt5;
				break;
			default:
				result = E_INVALIDARG;
		}

		if ( SUCCEEDED( result ))
		{
			unsigned int width = tlgHeader.dwWidth;
			unsigned int height = tlgHeader.dwHeight;

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

	HRESULT TLG_FrameDecode::LoadLinearImage( tlg::TLG_HEADER &tlgHeader, IStream *pIStream )
	{
		struct rgba
		{
			unsigned char r, g, b, a;
		};

		HRESULT result = S_OK;

		unsigned int width = tlgHeader.dwWidth;
		unsigned int height = tlgHeader.dwHeight;

		unsigned rshift, rsize, gshift, gsize, bshift, bsize, ashift, asize;

		MaskShiftAndSize( tlgHeader.ddpfPixelFormat.dwRBitMask, &rshift, &rsize );
		MaskShiftAndSize( tlgHeader.ddpfPixelFormat.dwGBitMask, &gshift, &gsize );
		MaskShiftAndSize( tlgHeader.ddpfPixelFormat.dwBBitMask, &bshift, &bsize );
		MaskShiftAndSize( tlgHeader.ddpfPixelFormat.dwRGBAlphaBitMask, &ashift, &asize );

		unsigned byteCount = (tlgHeader.ddpfPixelFormat.dwRGBBitCount + 7) / 8;

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
					cc->r = (unsigned char)PixelConvert( (c & tlgHeader.ddpfPixelFormat.dwRBitMask) >> rshift, rsize, 8 );
					cc->g = (unsigned char)PixelConvert( (c & tlgHeader.ddpfPixelFormat.dwGBitMask) >> gshift, gsize, 8 );
					cc->b = (unsigned char)PixelConvert( (c & tlgHeader.ddpfPixelFormat.dwBBitMask) >> bshift, bsize, 8 );

					if ( tlgHeader.ddpfPixelFormat.dwRGBAlphaBitMask )
						cc->a = (unsigned char)PixelConvert( (c & tlgHeader.ddpfPixelFormat.dwRGBAlphaBitMask) >> ashift, asize, 8 );
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

	HRESULT TLG_FrameDecode::FillBitmapSource( UINT width, UINT height, UINT dpiX, UINT dpiY,
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
	// TLG_Decoder implementation
	//----------------------------------------------------------------------------------------

	TLG_Decoder::TLG_Decoder()
		: BaseDecoder( CLSID_TLG_Decoder, CLSID_TLG_Container )
	{
	}

	TLG_Decoder::~TLG_Decoder()
	{
	}

	// TODO: implement real query capability
	STDMETHODIMP TLG_Decoder::QueryCapability( IStream *pIStream, DWORD *pCapability )
	{
		UNREFERENCED_PARAMETER( pIStream );

		HRESULT result = S_OK;

		*pCapability =
			WICBitmapDecoderCapabilityCanDecodeSomeImages;

		return result;
	}

	STDMETHODIMP TLG_Decoder::Initialize( IStream *pIStream, WICDecodeOptions cacheOptions )
	{
		UNREFERENCED_PARAMETER( cacheOptions );

		HRESULT result = E_INVALIDARG;

		ReleaseMembers( true );

		if ( pIStream )
		{
			tlg::TLG_HEADER tlgHeader;
			result = ReadFromIStream( pIStream, tlgHeader ); // read header

			if ( SUCCEEDED( result )) result = VerifyFactory();

			if ( SUCCEEDED( result ))
			{
				TLG_FrameDecode* frame = CreateNewDecoderFrame( m_factory, 0 ); 

				if ( tlgHeader.ddpfPixelFormat.dwFlags & tlg::DDPF_FOURCC )
					result = frame->Load_DXT_Image( tlgHeader, pIStream );
				else
					result = frame->LoadLinearImage( tlgHeader, pIStream );

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

	TLG_FrameDecode* TLG_Decoder::CreateNewDecoderFrame( IWICImagingFactory* factory , UINT i )
	{
		return new TLG_FrameDecode( factory, i );
	}

	void TLG_Decoder::Register( RegMan &regMan )
	{
		HMODULE curModule = GetModuleHandle( L"tlg-wic-codec.dll" );
		wchar_t tempFileName[MAX_PATH];
		if ( curModule != NULL ) GetModuleFileName( curModule, tempFileName, MAX_PATH );

		regMan.SetSZ( L"CLSID\\{7ED96837-96F0-4812-B211-F13C24117ED3}\\Instance\\{// {05103AD4-28F3-4229-A9A3-2928A8CE5E9A}}", L"CLSID", L"{// {05103AD4-28F3-4229-A9A3-2928A8CE5E9A}}" );
		regMan.SetSZ( L"CLSID\\{7ED96837-96F0-4812-B211-F13C24117ED3}\\Instance\\{// {05103AD4-28F3-4229-A9A3-2928A8CE5E9A}}", L"FriendlyName", L"TLG Decoder" );
		regMan.SetSZ( L"CLSID\\{// {05103AD4-28F3-4229-A9A3-2928A8CE5E9A}}", L"Version", L"1.0.0.1" );
		regMan.SetSZ( L"CLSID\\{// {05103AD4-28F3-4229-A9A3-2928A8CE5E9A}}", L"Date", _T(__DATE__) );
		regMan.SetSZ( L"CLSID\\{// {05103AD4-28F3-4229-A9A3-2928A8CE5E9A}}", L"SpecVersion", L"1.0.0.0" );
		regMan.SetSZ( L"CLSID\\{// {05103AD4-28F3-4229-A9A3-2928A8CE5E9A}}", L"ColorManagementVersion", L"1.0.0.0" );
		regMan.SetSZ( L"CLSID\\{// {05103AD4-28F3-4229-A9A3-2928A8CE5E9A}}", L"MimeTypes", L"x-image/tlg" );
		regMan.SetSZ( L"CLSID\\{// {05103AD4-28F3-4229-A9A3-2928A8CE5E9A}}", L"FileExtensions", L".tlg" );
		regMan.SetDW( L"CLSID\\{// {05103AD4-28F3-4229-A9A3-2928A8CE5E9A}}", L"SupportsAnimation", 0 );
		regMan.SetDW( L"CLSID\\{// {05103AD4-28F3-4229-A9A3-2928A8CE5E9A}}", L"SupportChromakey", 1 );
		regMan.SetDW( L"CLSID\\{// {05103AD4-28F3-4229-A9A3-2928A8CE5E9A}}", L"SupportLossless", 1 );
		regMan.SetDW( L"CLSID\\{// {05103AD4-28F3-4229-A9A3-2928A8CE5E9A}}", L"SupportMultiframe", 1 );
		regMan.SetSZ( L"CLSID\\{// {05103AD4-28F3-4229-A9A3-2928A8CE5E9A}}", L"ContainerFormat", L"{280BF6EC-0A7B-4870-8AB0-FC4DE12D0B7B}" );
		regMan.SetSZ( L"CLSID\\{// {05103AD4-28F3-4229-A9A3-2928A8CE5E9A}}", L"Author", L"Go Watanabe" );
		regMan.SetSZ( L"CLSID\\{// {05103AD4-28F3-4229-A9A3-2928A8CE5E9A}}", L"Description", L"TLG(kirikiri) Format Decoder" );
		regMan.SetSZ( L"CLSID\\{// {05103AD4-28F3-4229-A9A3-2928A8CE5E9A}}", L"FriendlyName", L"TLG Decoder" );

		regMan.SetSZ( L"CLSID\\{// {05103AD4-28F3-4229-A9A3-2928A8CE5E9A}}\\Formats", L"", L"" );
		regMan.SetSZ( L"CLSID\\{// {05103AD4-28F3-4229-A9A3-2928A8CE5E9A}}\\Formats\\{6FDDC324-4E03-4BFE-B185-3D77768DC90F}", L"", L"" );

		regMan.SetSZ( L"CLSID\\{// {05103AD4-28F3-4229-A9A3-2928A8CE5E9A}}\\InprocServer32", L"", tempFileName );
		regMan.SetSZ( L"CLSID\\{// {05103AD4-28F3-4229-A9A3-2928A8CE5E9A}}\\InprocServer32", L"ThreadingModel", L"Apartment" );
		regMan.SetSZ( L"CLSID\\{// {05103AD4-28F3-4229-A9A3-2928A8CE5E9A}}\\Patterns", L"", L"" );
		regMan.SetDW( L"CLSID\\{// {05103AD4-28F3-4229-A9A3-2928A8CE5E9A}}\\Patterns\\0", L"Position", 0 );
		regMan.SetDW( L"CLSID\\{// {05103AD4-28F3-4229-A9A3-2928A8CE5E9A}}\\Patterns\\0", L"Length", 4 );

		BYTE bytes[8] = { 0 };
		bytes[0] = 0x44; bytes[1] = 0x44; bytes[2] = 0x53; bytes[3] = 0x20;
		regMan.SetBytes( L"CLSID\\{// {05103AD4-28F3-4229-A9A3-2928A8CE5E9A}}\\Patterns\\0", L"Pattern", bytes, 4 );
		bytes[0] = bytes[1] = bytes[2] = bytes[3] = 0xFF;
		regMan.SetBytes( L"CLSID\\{// {05103AD4-28F3-4229-A9A3-2928A8CE5E9A}}\\Patterns\\0", L"Mask", bytes, 4 );
	}
}
