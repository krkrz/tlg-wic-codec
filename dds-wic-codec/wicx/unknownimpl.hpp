#pragma once

#include "../stdafx.hpp"

namespace wicx
{
	class UnknownImpl
	{
	public:
		UnknownImpl() : m_numReferences(0) {}

		ULONG STDMETHODCALLTYPE AddRef() { return ++m_numReferences; }

		ULONG STDMETHODCALLTYPE Release()
		{
			ULONG result;

			if ( m_numReferences > 0 )
			{
				--m_numReferences;
				result = m_numReferences;
			}
			else
				result = m_numReferences = 0;

			return result;
		}

	private:
		int m_numReferences;
	};
}
