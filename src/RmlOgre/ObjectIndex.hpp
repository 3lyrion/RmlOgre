#ifndef NIMBLE_RMLOGRE_OBJECTINDEX_HPP
#define NIMBLE_RMLOGRE_OBJECTINDEX_HPP

#include <optional>
#include <vector>


namespace nimble::RmlOgre {

// A map where the key is auto-assigned
template <class T>
class ObjectIndex
{
	Vector<std::optional<T>> array;
	Vector<std::size_t> empty;

public:
	std::size_t insert(T&& resource)
	{
		std::size_t index;
		if(!this->empty.empty())
		{
			index = this->empty.back();
			this->empty.pop_back();
		}
		else
		{
			index = this->array.size();
			this->array.emplace_back(std::nullopt);
		}

		this->array[index] = std::move(resource);

		return index;
	}
	std::size_t insert(const T& resource)
	{
		return this->insert(T{resource});
	}

	void erase(std::size_t index)
	{
		if(!this->has(index))
			return;

		this->array[index] = std::nullopt;
		this->empty.push_back(index);
	}

	bool has(std::size_t index) const
	{
		return index < this->array.size()
			&& this->array[index].has_value();
	}

	T& operator[](std::size_t index)
	{
		return *this->array[index];
	}
	const T& operator[](std::size_t index) const
	{
		return *this->array[index];
	}

	T& at(std::size_t index)
	{
		assert(this->has(index) && "ObjectIndex::at index out of range");

		return this->array[index].value();
	}
	const T& at(std::size_t index) const
	{
		assert(this->has(index) && "ObjectIndex::at index out of range");

		return this->array[index].value();
	}
};

}

#endif // NIMBLE_RMLOGRE_OBJECTINDEX_HPP
