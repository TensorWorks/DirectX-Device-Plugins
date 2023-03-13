#include "SafeArray.h"

unique_variant SafeArrayFactory::CreateStringArray(initializer_list<wstring> elems)
{
	// Create our array bounds descriptor
	SAFEARRAYBOUND bounds;
	bounds.lLbound = 0;
	bounds.cElements = elems.size();
	
	// Create a VARIANT to hold our array
	unique_variant vtArray;
	vtArray.vt = VT_ARRAY | VT_BSTR;
	
	// Create the SAFEARRAY and lock it for data access
	vtArray.parray = SafeArrayCreate(VT_BSTR, 1, &bounds);
	auto error = CheckHresult(SafeArrayLock(vtArray.parray));
	if (error) {
		throw error.Wrap(L"SafeArrayLock failed");
	}
	
	// Populate the array with the supplied elements
	BSTR* array = reinterpret_cast<BSTR*>(vtArray.parray->pvData);
	int index = 0;
	for (const auto& elem : elems)
	{
		// Note that a SAFEARRAY owns the memory of its elements, so we transfer ownership of each BSTR
		array[index] = wil::make_bstr(elem.c_str()).release();
		index++;
	}
	
	// Unlock the array
	SafeArrayUnlock(vtArray.parray);
	return vtArray;
}
