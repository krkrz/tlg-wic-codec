#pragma once

#include "../stdafx.hpp"

namespace wicx
{
	class RegMan
	{
	public:
		void SetSZ( wchar_t const *keyName, wchar_t const *valueName, LPCWSTR value )
		{
			SetRaw( keyName, valueName, REG_SZ, value, sizeof(wchar_t)*(wcslen(value)) );
		}

		void SetDW( wchar_t const *keyName, wchar_t const *valueName, DWORD value )
		{
			SetRaw( keyName, valueName, REG_DWORD, &value, sizeof(value) );
		}

		void SetBytes( wchar_t const *keyName, wchar_t const *valueName, void *value, size_t count )
		{
			SetRaw( keyName, valueName, REG_BINARY, value, count );
		}

		void Unregister();

	private:
		std::vector< wchar_t const * > m_keys;

		void SetRaw( wchar_t const *keyName, wchar_t const *valueName, unsigned type, void const *value, size_t valueSize );
	};
}
