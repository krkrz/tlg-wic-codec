#pragma once

#include "../stdafx.hpp"
#include "../wicx/basedecoder.hpp"
#include "../wicx/regman.hpp"

extern const GUID CLSID_TLG_Container;
extern const GUID CLSID_TLG_Decoder;

namespace tlg
{
	struct TLG_HEADER;
}

namespace tlgx
{
	using namespace wicx;

	class TLG_Decoder: public BaseDecoder
	{
	public:
		static void Register( RegMan &regMan );

		TLG_Decoder();
		~TLG_Decoder();

		// IWICBitmapDecoder interface

		STDMETHOD( QueryCapability )( 
			/* [in] */ IStream *pIStream,
			/* [out] */ DWORD *pCapability );

		STDMETHOD( Initialize )( 
			/* [in] */ IStream *pIStream,
			/* [in] */ WICDecodeOptions cacheOptions );

	protected:
		virtual BaseFrameDecode* CreateNewDecoderFrame( IWICImagingFactory *factory , UINT i );
	};
}
