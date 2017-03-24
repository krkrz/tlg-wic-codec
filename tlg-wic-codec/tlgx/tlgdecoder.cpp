#include "tlgdecoder.hpp"
#include "../libtlg/tlg.h"

// {280BF6EC-0A7B-4870-8AB0-FC4DE12D0B7B}
const GUID CLSID_TLG_Container = 
{ 0x280bf6ec, 0xa7b, 0x4870, { 0x8a, 0xb0, 0xfc, 0x4d, 0xe1, 0x2d, 0xb, 0x7b } };

// {05103AD4-28F3-4229-A9A3-2928A8CE5E9A}
const GUID CLSID_TLG_Decoder = 
{ 0x5103ad4, 0x28f3, 0x4229, { 0xa9, 0xa3, 0x29, 0x28, 0xa8, 0xce, 0x5e, 0x9a } };


/**
 * TLGLoader用
 */
class tMyStream : public tTJSBinaryStream
{
public:
	// コンストラクタ
	tMyStream(IStream *stream) : stream(stream) {
		stream->AddRef();
	}

	// デストラクタ
	~tMyStream() {
		if (stream) {
			stream->Release();
			stream = 0;
		}
	}

	virtual tjs_uint64 Seek(tjs_int64 offset, tjs_int whence) {
		if (stream) {
			DWORD origin;
			switch(whence) {
			case TJS_BS_SEEK_SET:			origin = STREAM_SEEK_SET;		break;
			case TJS_BS_SEEK_CUR:			origin = STREAM_SEEK_CUR;		break;
			case TJS_BS_SEEK_END:			origin = STREAM_SEEK_END;		break;
			default:						origin = STREAM_SEEK_SET;		break;
			}
			
			LARGE_INTEGER ofs;
			ULARGE_INTEGER newpos;
			
			ofs.QuadPart = offset;
			
			if (SUCCEEDED(stream->Seek(ofs, origin, &newpos))) {
				return newpos.QuadPart;
			}
		}
		return 0;
	}

	virtual tjs_uint Read(void *buffer, tjs_uint read_size) {
		if (stream) {
			ULONG cb = read_size;
			ULONG read;
			if (SUCCEEDED(stream->Read(buffer, cb, &read))) {
				return read;
			}
		}
		return 0;
	}

	virtual tjs_uint Write(const void *buffer, tjs_uint write_size) {
		if (stream) {
			ULONG cb = write_size;
			ULONG written;
			if (SUCCEEDED(stream->Write(buffer, cb, &written))) {
				return written;
			}
		}
		return 0;
	}

private:
	IStream *stream;
};



namespace tlgx
{

	//----------------------------------------------------------------------------------------
	// TLG_FrameDecode implementation
	//----------------------------------------------------------------------------------------
	
	class TLG_FrameDecode: public BaseFrameDecode
	{
	public:
		TLG_FrameDecode( IWICImagingFactory *pIFactory, UINT num ) : BaseFrameDecode( pIFactory, num ), width(0), height(0), pitch(0), outData(0) {
		}

		~TLG_FrameDecode() {
		}

		void clear() {
			if (outData) {
				delete[] outData;
				outData = 0;
			}
			width=0;
			height=0;
			pitch = 0;
		}

		bool setSize(int w, int h) {
			clear();
			width = w;
			height = h;
			pitch = (width * (32 / 8) +3) & ~3; /*4byte境界*/
			outData = new unsigned char[ pitch * height ];
			return outData != 0;
		}

		void *getScanLine(int y) {
			if (y < 0 || outData== NULL) {
				return NULL;
			}
			return outData + pitch * y;
		}

		static bool sizeCallback(void *callbackdata, unsigned int w, unsigned int h) {
			TLG_FrameDecode *decoder = (TLG_FrameDecode*)callbackdata;
			return decoder->setSize(w, h);
		}

		static void *scanLineCallback(void *callbackdata, int y) {
			TLG_FrameDecode *decoder = (TLG_FrameDecode*)callbackdata;
			return decoder->getScanLine(y);
		}

		/**
		 * データ転送
		 */
		HRESULT fill() {
			HRESULT result = S_OK;
			IWICImagingFactory *codecFactory = NULL;
			if ( SUCCEEDED( result ))
				result = CoCreateInstance( CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, reinterpret_cast<LPVOID *>( &codecFactory ));
			if ( SUCCEEDED( result ))
				result = codecFactory->CreateBitmapFromMemory ( width, height, GUID_WICPixelFormat32bppBGRA, pitch, pitch * height,
																												outData, reinterpret_cast<IWICBitmap **>( &( m_bitmapSource )));
			if ( codecFactory ) codecFactory->Release();
			return result;
		}

		HRESULT LoadImage(IStream *pIStream) {
			tMyStream stream(pIStream);
			HRESULT result = S_OK;
			int ret = TVPLoadTLG(this, sizeCallback, scanLineCallback, NULL, &stream);
			switch (ret) {
			case 0:
				// 成功
				result = fill();
				break;
			case 1:
				// 中断された
				result = E_FAIL;
				break;
			default:
				// エラー
				result = E_FAIL;
				break;
			}
			clear();
			return result;
		}

	private:
		int width;
		int height;
		unsigned int pitch;
		unsigned char *outData;
	};

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
			result = VerifyFactory();
			if ( SUCCEEDED( result ))
			{
				TLG_FrameDecode* frame = (TLG_FrameDecode*)CreateNewDecoderFrame( m_factory, 0 );
				result = frame->LoadImage(pIStream);
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

	BaseFrameDecode* TLG_Decoder::CreateNewDecoderFrame( IWICImagingFactory *factory , UINT i ) {
		return new TLG_FrameDecode( factory, i );
	}
	
	void TLG_Decoder::Register( RegMan &regMan )
	{
		HMODULE curModule = GetModuleHandle( L"tlg-wic-codec.dll" );
		wchar_t tempFileName[MAX_PATH];
		if ( curModule != NULL ) GetModuleFileName( curModule, tempFileName, MAX_PATH );

		regMan.SetSZ( L"CLSID\\{7ED96837-96F0-4812-B211-F13C24117ED3}\\Instance\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}", L"CLSID", L"{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}" );
		regMan.SetSZ( L"CLSID\\{7ED96837-96F0-4812-B211-F13C24117ED3}\\Instance\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}", L"FriendlyName", L"TLG Decoder" );
		regMan.SetSZ( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}", L"Version", L"1.0.0.1" );
		regMan.SetSZ( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}", L"Date", _T(__DATE__) );
		regMan.SetSZ( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}", L"SpecVersion", L"1.0.0.0" );
		regMan.SetSZ( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}", L"ColorManagementVersion", L"1.0.0.0" );
		regMan.SetSZ( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}", L"MimeTypes", L"x-image/tlg" );
		regMan.SetSZ( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}", L"FileExtensions", L".tlg" );
		regMan.SetDW( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}", L"SupportsAnimation", 0 );
		regMan.SetDW( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}", L"SupportChromakey", 1 );
		regMan.SetDW( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}", L"SupportLossless", 1 );
		regMan.SetDW( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}", L"SupportMultiframe", 1 );
		regMan.SetSZ( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}", L"ContainerFormat", L"{280BF6EC-0A7B-4870-8AB0-FC4DE12D0B7B}" );
		regMan.SetSZ( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}", L"Author", L"Go Watanabe" );
		regMan.SetSZ( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}", L"Description", L"TLG(kirikiri) Format Decoder" );
		regMan.SetSZ( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}", L"FriendlyName", L"TLG Decoder" );

		regMan.SetSZ( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}\\Formats", L"", L"" );
		regMan.SetSZ( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}\\Formats\\{6FDDC324-4E03-4BFE-B185-3D77768DC90F}", L"", L"" );

		regMan.SetSZ( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}\\InprocServer32", L"", tempFileName );
		regMan.SetSZ( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}\\InprocServer32", L"ThreadingModel", L"Apartment" );

		// パターン登録
		char *mask = "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff";
		regMan.SetSZ( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}\\Patterns", L"", L"" );

		regMan.SetDW( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}\\Patterns\\0", L"Position", 0 );
		regMan.SetDW( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}\\Patterns\\0", L"Length", 10 );
		regMan.SetBytes( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}\\Patterns\\0", L"Pattern", "TLG0.0\x00sds\x1a", 10 );
		regMan.SetBytes( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}\\Patterns\\0", L"Mask", mask, 10 );

		regMan.SetDW( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}\\Patterns\\1", L"Position", 0 );
		regMan.SetDW( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}\\Patterns\\1", L"Length", 10 );
		regMan.SetBytes( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}\\Patterns\\1", L"Pattern", "TLG5.0\x00raw\x1a", 10 );
		regMan.SetBytes( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}\\Patterns\\1", L"Mask", mask, 10 );

		regMan.SetDW( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}\\Patterns\\2", L"Position", 0 );
		regMan.SetDW( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}\\Patterns\\2", L"Length", 10 );
		regMan.SetBytes( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}\\Patterns\\2", L"Pattern", "TLG6.0\x00raw\x1a", 10 );
		regMan.SetBytes( L"CLSID\\{05103AD4-28F3-4229-A9A3-2928A8CE5E9A}\\Patterns\\2", L"Mask", mask, 10 );
	}
}
