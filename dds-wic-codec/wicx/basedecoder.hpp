#pragma once

#include "../stdafx.hpp"
#include "unknownimpl.hpp"

namespace wicx
{
	class BaseFrameDecode: public IWICBitmapFrameDecode
	{
	public:
		BaseFrameDecode( IWICImagingFactory *pIFactory, UINT num );
		virtual ~BaseFrameDecode();

		// IUnknown interface

		STDMETHOD( QueryInterface )( REFIID riid, void **ppv );

		STDMETHOD_( ULONG, AddRef )();

		STDMETHOD_( ULONG, Release )();

		// IWICBitmapFrameDecode interface

		STDMETHOD( GetMetadataQueryReader )( 
			/* [out] */ IWICMetadataQueryReader **ppIMetadataQueryReader );

		STDMETHOD( GetColorContexts )(
			/* [in] */ UINT cCount,
			/* [in] [out] */ IWICColorContext **ppIColorContexts,
			/* [out] */ UINT *pcActualCount);

		STDMETHOD( GetThumbnail )( 
			/* [out] */ IWICBitmapSource **ppIThumbnail );

		// IWICBitmapSource interface

		STDMETHOD( GetSize )( 
			/* [out] */ UINT *puiWidth,
			/* [out] */ UINT *puiHeight );

		STDMETHOD( GetPixelFormat )( 
			/* [out] */ WICPixelFormatGUID *pPixelFormat );

		STDMETHOD( GetResolution )( 
			/* [out] */ double *pDpiX,
			/* [out] */ double *pDpiY );

		STDMETHOD( CopyPalette )( 
			/* [in] */ IWICPalette *pIPalette );

		STDMETHOD( CopyPixels )( 
			/* [in] */ const WICRect *prc,
			/* [in] */ UINT cbStride,
			/* [in] */ UINT cbPixelsSize,
			/* [out] */ BYTE *pbPixels );

		IWICBitmapSource *m_bitmapSource; // TODO

	protected:
		IWICImagingFactory *m_factory;
		UINT m_frameNumber;
		IWICPalette *m_palette;
		std::vector<IWICColorContext *> m_colorContexts;
		IWICBitmapSource *m_thumbnail;
		IWICBitmapSource *m_preview;

	private:
		UnknownImpl m_unknownImpl;

		void ReleaseMembers();
	};

	class BaseDecoder: public IWICBitmapDecoder
	{
	public:
		BaseDecoder( GUID Me, GUID Container );
		virtual ~BaseDecoder();

		// IUnknown interface

		STDMETHOD( QueryInterface )( REFIID riid, void **ppv );
		STDMETHOD_( ULONG, AddRef )();
		STDMETHOD_( ULONG, Release )();

		// IWICBitmapDecoder interface

		STDMETHOD( QueryCapability )( 
			/* [in] */ IStream *pIStream,
			/* [out] */ DWORD *pCapability ) = 0;

		STDMETHOD( Initialize )( 
			/* [in] */ IStream *pIStream,
			/* [in] */ WICDecodeOptions cacheOptions ) = 0;

		STDMETHOD( GetContainerFormat )( 
			/* [out] */ GUID *pguidContainerFormat );

		STDMETHOD( GetDecoderInfo )( 
			/* [out] */ IWICBitmapDecoderInfo **ppIDecoderInfo );

		STDMETHOD( CopyPalette )( 
			/* [in] */ IWICPalette *pIPalette );

		STDMETHOD( GetMetadataQueryReader )( 
			/* [out] */ IWICMetadataQueryReader **ppIMetadataQueryReader );

		STDMETHOD( GetPreview )( 
			/* [out] */ IWICBitmapSource **ppIPreview );

		STDMETHOD( GetColorContexts )(
			/* [in] */ UINT cCount,
			/* [in] [out] */ IWICColorContext **ppIColorContexts,
			/* [out] */ UINT *pcActualCount );

		STDMETHOD( GetThumbnail )( 
			/* [out] */ IWICBitmapSource **ppIThumbnail );

		STDMETHOD( GetFrameCount )( 
			/* [out] */ UINT *pCount );

		STDMETHOD( GetFrame )( 
			/* [in] */ UINT index,
			/* [out] */ IWICBitmapFrameDecode **ppIBitmapFrame );

	protected:
		IWICImagingFactory *m_factory;
		std::vector<BaseFrameDecode *> m_frames;
		IWICPalette *m_palette;
		std::vector<IWICColorContext *> m_colorContexts;
		IWICBitmapSource *m_thumbnail;
		IWICBitmapSource *m_preview;

		GUID const m_CLSID_Container;
		GUID const m_CLSID_This;

		virtual BaseFrameDecode* CreateNewDecoderFrame( IWICImagingFactory* factory, UINT i ) = 0;
		virtual void AddDecoderFrame( BaseFrameDecode* frame );
		HRESULT VerifyFactory();
		void ReleaseMembers( bool releaseFactory );

	private:
		UnknownImpl m_unknownImpl;

		BaseDecoder &operator = ( BaseDecoder const & ); // FIXME: operator couldn't be generated
	};
}
