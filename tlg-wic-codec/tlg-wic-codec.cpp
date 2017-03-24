#include "stdafx.hpp"
#include "wicx/regman.hpp"
#include "wicx/classfactory.hpp"
#include "tlgx/tlgdecoder.hpp"

#include <shlobj.h>

STDAPI DllRegisterServer()
{    
	wicx::RegMan regMan;
	tlgx::TLG_Decoder::Register( regMan );

	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

	return S_OK;
}

STDAPI DllUnregisterServer()
{    
	wicx::RegMan regMan;
	tlgx::TLG_Decoder::Register( regMan );
	regMan.Unregister();

	return S_OK;
}

STDAPI DllGetClassObject( REFCLSID rclsid, REFIID riid, void **ppv )
{    
	HRESULT result = E_INVALIDARG; 

	if ( NULL != ppv )
	{
		IClassFactory *classFactory = NULL;

		if ( CLSID_TLG_Decoder == rclsid )
		{
			result = S_OK;
			classFactory = new wicx::ClassFactory<tlgx::TLG_Decoder>();
		}
		else
			result = E_NOINTERFACE;

		if ( SUCCEEDED( result ))
		{
			if ( NULL != classFactory )
				result = classFactory->QueryInterface( riid, ppv );
			else
				result = E_OUTOFMEMORY;
		}
	}

	return result;
}

BOOL APIENTRY DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved )
{
	UNREFERENCED_PARAMETER( lpvReserved );

	switch ( fdwReason )
	{
		case DLL_PROCESS_ATTACH:
			DisableThreadLibraryCalls( hinstDLL );
			break;

		case DLL_PROCESS_DETACH:
			break;
	}

	return TRUE;
}
