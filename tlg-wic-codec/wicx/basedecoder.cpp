#include "basedecoder.hpp"

#define WICX_RELEASE(X) if ( NULL != X ) { X->Release(); X = NULL; }

namespace wicx
{
	//----------------------------------------------------------------------------------------
	// BaseFrameDecode implementation
	//----------------------------------------------------------------------------------------

	BaseFrameDecode::BaseFrameDecode( IWICImagingFactory *pIFactory, UINT num )
		: m_factory(pIFactory), m_frameNumber(num), m_bitmapSource(NULL), m_palette(NULL), m_thumbnail(NULL), m_preview(NULL)
	{
		if ( NULL != m_factory ) m_factory->AddRef();
	}

	BaseFrameDecode::~BaseFrameDecode()
	{
		ReleaseMembers();
	}

	void BaseFrameDecode::ReleaseMembers()
	{
		WICX_RELEASE( m_factory );
		WICX_RELEASE( m_bitmapSource );
		WICX_RELEASE( m_palette );
		for ( size_t i = 0; i < m_colorContexts.size(); i++ ) WICX_RELEASE( m_colorContexts[i] );
		WICX_RELEASE( m_thumbnail );
		WICX_RELEASE( m_preview );
	}

	// ----- IUnknown interface ---------------------------------------------------------------------------

	STDMETHODIMP BaseFrameDecode::QueryInterface( REFIID iid, void **ppvObject )
	{
		HRESULT result = E_INVALIDARG;

		if ( ppvObject )
		{
			if ( iid == IID_IUnknown )
			{
				*ppvObject = static_cast<IUnknown *>(this);
				AddRef();
				result = S_OK;
			}
			else if ( iid == IID_IWICBitmapFrameDecode )
			{
				*ppvObject = static_cast<IWICBitmapFrameDecode *>( this );
				AddRef();
				result = S_OK;
			}
			else if ( iid == IID_IWICBitmapSource )
			{
				*ppvObject = m_bitmapSource;
				if ( NULL != m_bitmapSource ) m_bitmapSource->AddRef();
				result = S_OK;
			}
			else
				result = E_NOINTERFACE;
		}

		return result;
	}

	STDMETHODIMP_(ULONG) BaseFrameDecode::AddRef()
	{
		return m_unknownImpl.AddRef();
	}

	STDMETHODIMP_(ULONG) BaseFrameDecode::Release()
	{
		ULONG result = m_unknownImpl.Release();
		if ( 0 == result ) delete this;

		return result;
	}

	// ----- IWICBitmapFrameDecode interface --------------------------------------------------------------

	STDMETHODIMP BaseFrameDecode::GetMetadataQueryReader( IWICMetadataQueryReader **ppIMetadataQueryReader )
	{
		UNREFERENCED_PARAMETER( ppIMetadataQueryReader );
		return WINCODEC_ERR_UNSUPPORTEDOPERATION;
	}

	STDMETHODIMP BaseFrameDecode::GetColorContexts( UINT cCount, IWICColorContext **ppIColorContexts, UINT *pcActualCount )
	{
		UNREFERENCED_PARAMETER( cCount );

		HRESULT result = S_OK;

		if ( ppIColorContexts == NULL )
		{
			// return the number of color contexts
			if ( pcActualCount != NULL )
				*pcActualCount = (UINT) m_colorContexts.size();
			else
				result = E_INVALIDARG;
		}
		else
		{
			// return the actual color contexts
			for ( size_t i = 0; i < m_colorContexts.size(); i++ )
			{
				ppIColorContexts[i] = m_colorContexts[i];
				m_colorContexts[i]->AddRef();            
			}        
		}

		return result;
	}

	STDMETHODIMP BaseFrameDecode::GetThumbnail( IWICBitmapSource **ppIThumbnail )
	{
		HRESULT result = S_OK;

		if ( NULL == ppIThumbnail ) result = E_INVALIDARG;

		if ( SUCCEEDED( result ))
		{
			*ppIThumbnail = m_thumbnail;

			if ( NULL != m_thumbnail )
				m_thumbnail->AddRef();
			else
				result = WINCODEC_ERR_CODECNOTHUMBNAIL;
		}

		return result;
	}

	// ----- IWICBitmapSource interface -------------------------------------------------------------------
	
	STDMETHODIMP BaseFrameDecode::GetSize( UINT *puiWidth, UINT *puiHeight )
	{
		HRESULT result = E_UNEXPECTED;
		if ( m_bitmapSource ) result = m_bitmapSource->GetSize( puiWidth, puiHeight );
		return result;
	}

	STDMETHODIMP BaseFrameDecode::GetPixelFormat( WICPixelFormatGUID *pPixelFormat )
	{
		HRESULT result = E_UNEXPECTED;
		if ( m_bitmapSource ) result = m_bitmapSource->GetPixelFormat( pPixelFormat );
		return result;
	}

	STDMETHODIMP BaseFrameDecode::GetResolution( double *pDpiX, double *pDpiY )
	{
		HRESULT result = E_UNEXPECTED;
		if ( m_bitmapSource ) result = m_bitmapSource->GetResolution( pDpiX, pDpiY );
		return result;
	}

	STDMETHODIMP BaseFrameDecode::CopyPalette( IWICPalette *pIPalette )
	{
		HRESULT result = S_OK;

		if ( NULL == pIPalette ) result = E_INVALIDARG;

		if ( SUCCEEDED( result ))
		{
			if ( NULL != m_palette )
				pIPalette->InitializeFromPalette( m_palette );
			else
				result = E_UNEXPECTED;
		}

		return result;
	}

	STDMETHODIMP BaseFrameDecode::CopyPixels( WICRect const *prc, UINT cbStride, UINT cbPixelsSize, BYTE *pbPixels )
	{
		HRESULT result = E_UNEXPECTED;
		if ( m_bitmapSource ) result = m_bitmapSource->CopyPixels( prc, cbStride, cbPixelsSize, pbPixels );
		return result;
	}

	//----------------------------------------------------------------------------------------
	// BaseDecoder implementation
	//----------------------------------------------------------------------------------------

	BaseDecoder::BaseDecoder( GUID Me, GUID Container )
		: m_factory(NULL), m_palette(NULL), m_thumbnail(NULL), m_preview(NULL), m_CLSID_This(Me), m_CLSID_Container(Container)
	{
//		MessageBox( NULL, L"Decoder()", L"dds_wic_codec", MB_OK);
	}

	BaseDecoder::~BaseDecoder()
	{
//		MessageBox( NULL, L"~Decoder()", L"dds_wic_codec", MB_OK);
		ReleaseMembers( true );
	}

	void BaseDecoder::ReleaseMembers( bool releaseFactory )
	{
		if ( releaseFactory ) WICX_RELEASE( m_factory );

		for ( size_t i = 0; i < m_frames.size(); ++i ) WICX_RELEASE( m_frames[i] );
		m_frames.clear();

		WICX_RELEASE( m_palette );

		for ( size_t i = 0; i < m_colorContexts.size(); ++i ) WICX_RELEASE( m_colorContexts[i] );
		m_colorContexts.clear();

		WICX_RELEASE( m_thumbnail );

		WICX_RELEASE( m_preview );
	}

	HRESULT BaseDecoder::VerifyFactory()
	{
		if ( NULL == m_factory )
			return CoCreateInstance( CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory,
				reinterpret_cast<LPVOID *>( &m_factory ));
		else
			return S_OK;
	}

	void BaseDecoder::AddDecoderFrame( BaseFrameDecode *frame )
	{
		m_frames.push_back( frame );
		m_frames.back()->AddRef();
	}

	// ----- IUnknown interface ---------------------------------------------------------------------------

	STDMETHODIMP BaseDecoder::QueryInterface( REFIID iid, void **ppvObject )
	{
		HRESULT result = E_INVALIDARG;

		if ( ppvObject )
		{
			if ( iid == IID_IUnknown )
			{
				*ppvObject = static_cast<IUnknown*>(this);
				AddRef();

				result = S_OK;
			}
			else if ( iid == IID_IWICBitmapDecoder )
			{
				*ppvObject = static_cast<IWICBitmapDecoder *>(this);
				AddRef();

				result = S_OK;
			}
			else
				result = E_NOINTERFACE;
		}

		return result;
	}

	STDMETHODIMP_(ULONG) BaseDecoder::AddRef()
	{
		return m_unknownImpl.AddRef();
	}

	STDMETHODIMP_(ULONG) BaseDecoder::Release()
	{
		ULONG result = m_unknownImpl.Release();
		if ( 0 == result ) delete this;

		return result;
	}

	// ----- IWICBitmapDecoder interface ------------------------------------------------------------------

	STDMETHODIMP BaseDecoder::GetContainerFormat( GUID *pguidContainerFormat )
	{
		HRESULT result = E_INVALIDARG;

		if ( NULL != pguidContainerFormat )
		{
			result = S_OK;
			*pguidContainerFormat = m_CLSID_Container;
		}

		return result;
	}

	STDMETHODIMP BaseDecoder::GetDecoderInfo( IWICBitmapDecoderInfo **ppIDecoderInfo )
	{
		HRESULT result = S_OK;

		IWICComponentInfo *compInfo = NULL;

		if ( SUCCEEDED( result )) result = VerifyFactory();

		if ( SUCCEEDED( result )) result = m_factory->CreateComponentInfo( m_CLSID_This, &compInfo );

		if ( SUCCEEDED( result )) result = compInfo->QueryInterface( IID_IWICBitmapDecoderInfo, reinterpret_cast<void **>( ppIDecoderInfo ));

		WICX_RELEASE( compInfo );

		return result;
	}

	STDMETHODIMP BaseDecoder::CopyPalette( IWICPalette *pIPalette )
	{
		HRESULT result = S_OK;

		if ( NULL == pIPalette ) result = E_INVALIDARG;

		if ( SUCCEEDED( result ))
		{
			if ( NULL != m_palette )
				pIPalette->InitializeFromPalette( m_palette );
			else
				result = E_UNEXPECTED;
		}

		return result;
	}

	STDMETHODIMP BaseDecoder::GetMetadataQueryReader( IWICMetadataQueryReader **ppIMetadataQueryReader )
	{
		UNREFERENCED_PARAMETER( ppIMetadataQueryReader );
		return E_NOTIMPL;
	}

	STDMETHODIMP BaseDecoder::GetPreview( IWICBitmapSource **ppIPreview )
	{
		HRESULT result = S_OK;

		if ( NULL == ppIPreview ) result = E_INVALIDARG;

		if ( SUCCEEDED( result ))
		{
			if ( NULL != m_preview )
				result = m_preview->QueryInterface( IID_IWICBitmapSource, reinterpret_cast<void **>( ppIPreview ));
			else
				result = E_UNEXPECTED;
		}

		return result;
	}

	STDMETHODIMP BaseDecoder::GetColorContexts( UINT cCount, IWICColorContext **ppIColorContexts, UINT *pcActualCount )
	{
		UNREFERENCED_PARAMETER( cCount );

		HRESULT result = S_OK;

		if ( ppIColorContexts == NULL )
		{
			// return the number of color contexts
			if ( pcActualCount != NULL )
				*pcActualCount = (UINT) m_colorContexts.size();        
			else
				result = E_INVALIDARG;
		}
		else
		{
			// return the actual color contexts
			for ( size_t i = 0; i < m_colorContexts.size(); i++ )
			{
				ppIColorContexts[i] = m_colorContexts[i];
				m_colorContexts[i]->AddRef();            
			}        
		}

		return result;
	}

	STDMETHODIMP BaseDecoder::GetThumbnail( IWICBitmapSource **ppIThumbnail )
	{
		HRESULT result = S_OK;

		if ( NULL == ppIThumbnail ) result = E_INVALIDARG;

		if ( SUCCEEDED( result ))
		{
			if ( NULL != m_thumbnail )
				result = m_thumbnail->QueryInterface( IID_IWICBitmapSource, (void**)ppIThumbnail );
			else
				result = WINCODEC_ERR_CODECNOTHUMBNAIL;
		}

		return result;
	}

	STDMETHODIMP BaseDecoder::GetFrameCount( UINT *pCount )
	{
		HRESULT result = S_OK;

		if ( NULL == pCount ) result = E_INVALIDARG;

		if ( SUCCEEDED( result )) *pCount = (UINT) m_frames.size();

		return result;
	}

	STDMETHODIMP BaseDecoder::GetFrame( UINT index, IWICBitmapFrameDecode **ppIBitmapFrame )
	{
		HRESULT result = S_OK;

		if (( index >= m_frames.size() ) || ( NULL == ppIBitmapFrame )) result = E_INVALIDARG;

		if ( SUCCEEDED( result ))
			result = m_frames[index]->QueryInterface( IID_IWICBitmapFrameDecode, reinterpret_cast<void **>( ppIBitmapFrame ));

		return result;
	}
}
