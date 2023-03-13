#pragma once

#include "ErrorHandling.h"

using std::initializer_list;
using std::wstring;
using wil::unique_variant;


// Provides functionality for iterating over the contents of a one-dimensional SAFEARRAY
template <typename T>
class SafeArrayIterator
{
	public:
		SafeArrayIterator(SAFEARRAY* array)
		{
			// Lock the array for data access
			this->array = array;
			this->reinterpretedArray = reinterpret_cast<T*>(this->array->pvData);
			auto error = CheckHresult(SafeArrayLock(this->array));
			if (error) {
				throw error.Wrap(L"SafeArrayLock failed");
			}
			
			// Retrieve the array bounds and compute the number of elements
			long lowerBound = 0;
			long upperBound = 0;
			SafeArrayGetLBound(this->array, 1, &lowerBound);
			SafeArrayGetUBound(this->array, 1, &upperBound);
			this->numElements = (upperBound - lowerBound) + 1;
		}
		
		~SafeArrayIterator()
		{
			// Unlock the array
			SafeArrayUnlock(this->array);
			this->array = nullptr;
			this->reinterpretedArray = nullptr;
		}
		
		T* begin() {
			return this->reinterpretedArray;
		}
		
		const T* begin() const {
			return this->reinterpretedArray;
		}
		
		T* end() {
			return this->reinterpretedArray + this->numElements;
		}
		
		const T* end() const {
			return this->reinterpretedArray + this->numElements;
		}
		
	private:
		SAFEARRAY* array;
		T* reinterpretedArray;
		long numElements;
};


// Provides functionality for creating one-dimensional SAFEARRAY instances for specific element types
class SafeArrayFactory
{
	public:
		
		// Creates a SAFEARRAY of BSTR strings and wraps it in a VARIANT
		static unique_variant CreateStringArray(initializer_list<wstring> elems);
};
