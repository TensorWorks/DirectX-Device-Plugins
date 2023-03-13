#pragma once

namespace ObjectHelpers {


// Retrieves the list of keys for an STL associative container type (maps, sets, etc.)
// For the full list of supported container types, see:
// - <https://en.cppreference.com/w/cpp/named_req/AssociativeContainer>
// - <https://en.cppreference.com/w/cpp/named_req/UnorderedAssociativeContainer>
template <typename MappingType>
std::vector<typename MappingType::key_type> GetMappingKeys(const MappingType& mapping)
{
	std::vector<typename MappingType::key_type> keys;
	
	for (const auto& pair : mapping) {
		keys.push_back(pair.first);
	}
	
	return keys;
}

// Returns a zeroed-out instance of the specified struct type
template<typename T> T GetZeroedStruct()
{
	T instance;
	ZeroMemory(&instance, sizeof(T));
	return instance;
}


} // namespace ObjectHelpers
