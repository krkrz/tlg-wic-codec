#include "stdafx.hpp"
#include "wicx/regman.hpp"
#include "wicx/classfactory.hpp"
#include "ddsx/ddsdecoder.hpp"
#include "pvrx/pvrdecoder.hpp"

#include <shlobj.h>

STDAPI DllRegisterServer()
{    
	wicx::RegMan regMan;
	ddsx::DDS_Decoder::Register( regMan );
	pvrx::PVR_Decoder::Register( regMan );

	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

	return S_OK;
}

STDAPI DllUnregisterServer()
{    
	wicx::RegMan regMan;
	ddsx::DDS_Decoder::Register( regMan );
	pvrx::PVR_Decoder::Register( regMan );
	regMan.Unregister();

	return S_OK;
}

STDAPI DllGetClassObject( REFCLSID rclsid, REFIID riid, void **ppv )
{    
	HRESULT result = E_INVALIDARG; 

	if ( NULL != ppv )
	{
		IClassFactory *classFactory = NULL;

		if ( CLSID_DDS_Decoder == rclsid )
		{
			result = S_OK;
			classFactory = new wicx::ClassFactory<ddsx::DDS_Decoder>();
		}
		else if ( CLSID_PVR_Decoder == rclsid )
		{
			result = S_OK;
			classFactory = new wicx::ClassFactory<pvrx::PVR_Decoder>();
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
