#include "regman.hpp"

namespace wicx
{
	void RegMan::SetRaw( wchar_t const *keyName, wchar_t const *valueName, unsigned type, void const *value, size_t valueSize )
	{
		m_keys.push_back( keyName );
		HKEY hKey;
		long err = RegCreateKey( HKEY_CLASSES_ROOT, keyName, &hKey );
		if ( err == ERROR_SUCCESS ) err = RegSetValueEx(hKey, valueName, 0, type, reinterpret_cast<BYTE const *>(value),
			static_cast<DWORD>(valueSize) );
	}

	void RegMan::Unregister()
	{
		for ( unsigned i = 0; i < m_keys.size(); ++i )
		{
			RegDeleteKey( HKEY_CLASSES_ROOT, m_keys[i] );
		}
	}
}
