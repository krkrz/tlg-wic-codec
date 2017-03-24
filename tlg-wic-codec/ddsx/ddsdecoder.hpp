#pragma once

#include "../stdafx.hpp"
#include "../wicx/basedecoder.hpp"
#include "../wicx/regman.hpp"

extern const GUID CLSID_DDS_Container;
extern const GUID CLSID_DDS_Decoder;

namespace dds
{
	struct DDS_HEADER;
}

namespace ddsx
{
	using namespace wicx;

	class DDS_FrameDecode: public BaseFrameDecode
	{
	public:
		DDS_FrameDecode( IWICImagingFactory *pIFactory, UINT num );

		HRESULT Load_DXT_Image( dds::DDS_HEADER &ddsHeader, IStream *pIStream );
		HRESULT LoadLinearImage( dds::DDS_HEADER &ddsHeader, IStream *pIStream );

	private:
		HRESULT FillBitmapSource( UINT width, UINT height, UINT dpiX, UINT dpiY, REFWICPixelFormatGUID pixelFormat,
			UINT cbStride, UINT cbBufferSize, BYTE *pbBuffer );
	};

	class DDS_Decoder: public BaseDecoder
	{
	public:
		static void Register( RegMan &regMan );

		DDS_Decoder();
		~DDS_Decoder();

		// IWICBitmapDecoder interface

		STDMETHOD( QueryCapability )( 
			/* [in] */ IStream *pIStream,
			/* [out] */ DWORD *pCapability );

		STDMETHOD( Initialize )( 
			/* [in] */ IStream *pIStream,
			/* [in] */ WICDecodeOptions cacheOptions );

	protected:
		virtual DDS_FrameDecode* CreateNewDecoderFrame( IWICImagingFactory *factory , UINT i );
	};
}
