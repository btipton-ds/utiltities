#pragma once
/*
This file is part of the DistFieldHexMesh application/library.

	The DistFieldHexMesh application/library is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	The DistFieldHexMesh application/library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	This link provides the exact terms of the GPL license <https://www.gnu.org/licenses/>.

	The author's interpretation of GPL 3 is that if you receive money for the use or distribution of the DistFieldHexMesh application/library or a derivative product, GPL 3 no longer applies.

	Under those circumstances, the author expects and may legally pursue a reasoble share of the income. To avoid the complexity of agreements and negotiation, the author makes
	no specific demands in this regard. Compensation of roughly 1% of net or $5 per user license seems appropriate, but is not legally binding.

	In lay terms, if you make a profit by using the DistFieldHexMesh application/library (violating the spirit of Open Source Software), I expect a reasonable share for my efforts.

	Robert R Tipton - Author

	Dark Sky Innovative Solutions http://darkskyinnovation.com/
*/

#include <pool_allocator.h>
#include <pool_vector.h>

#define TEMPL_DECL template<class T> 
#define ITER_TEMPL_DECL template <int IterType>
#define VECTOR_DECL vector<T> 
#define ITER_DECL vector<T>::_iterator<IterType>

namespace MultiCore {

TEMPL_DECL
	VECTOR_DECL::vector(const std::vector<T>& src)
{
	insert(end(), src);
}

TEMPL_DECL
VECTOR_DECL::vector(const std::initializer_list<T>& src)
{
	insert(end(), src);
}

TEMPL_DECL
VECTOR_DECL::~vector()
{
	free(_pData); // free<T> will handle destruction for each object in local_heap
}

TEMPL_DECL
VECTOR_DECL::operator std::vector<T>() const
{
	std::vector<T> result;
	result.insert(result.end(), begin(), end());
	return result;
}

TEMPL_DECL
void VECTOR_DECL::clear()
{
	for (size_t i = 0; i < _size; i++) {
		_pData[i] = T(); // Replace with empty objects, but DO NOT destroy them YET.
	}
	_size = 0;

#if DUPLICATE_STD_TESTS	
	_data.clear();
#endif
}

TEMPL_DECL
bool VECTOR_DECL::empty()
{
	return _size == 0;
}


TEMPL_DECL
size_t VECTOR_DECL::size() const
{
#if DUPLICATE_STD_TESTS	
	assert(_size == _data.size());
#endif
	return _size;
}

TEMPL_DECL
void VECTOR_DECL::resize(size_t val)
{
#if DUPLICATE_STD_TESTS	
	_data.resize(val);
#endif

	reserve(val);
	_size = val;
	size_t oldSize = _size;
	for (size_t i = oldSize; i <= _size; i++) {
		_pData[i] = {};
	}
}

TEMPL_DECL
void VECTOR_DECL::reserve(size_t newCapacity)
{
#if DUPLICATE_STD_TESTS	
	_data.reserve(newCapacity);
#endif
	if (newCapacity > _capacity) {
		T* pTmp = _pData;
		_pData = alloc<T>(newCapacity);

		if (pTmp) {
			for (size_t i = 0; i < _size; i++)
				_pData[i] = pTmp[i];

			free(pTmp);
			_capacity = newCapacity;
		}
	}
}

TEMPL_DECL
template<class ITER_TYPE>
void VECTOR_DECL::insert(const iterator& at, const ITER_TYPE& begin, const ITER_TYPE& end)
{
	size_t idx = (size_t)(at._pEntry - _pData);
#if DUPLICATE_STD_TESTS	
	_data.insert(_data.begin() + idx, begin, end);
#endif

	size_t entriesNeeded = end - begin;
	resize(_size + entriesNeeded);
	for (size_t i = _size - 1; i > idx + entriesNeeded; i--)
		_pData[i] = _pData[i - 1];

	for (auto iter = begin; iter != end; iter++) {
		_pData[idx++] = *iter;
	}
}

TEMPL_DECL
void VECTOR_DECL::insert(const iterator& at, const T& val)
{
	size_t idx = (size_t)(at._pEntry - _pData);
#if DUPLICATE_STD_TESTS	
	_data.insert(_data.begin() + idx, val);
#endif

	resize(_size + 1);
	for (size_t i = _size - 1; i > idx; i--)
		_pData[i] = _pData[i - 1];

	_pData[idx] = val;
}

TEMPL_DECL
void VECTOR_DECL::insert(const iterator& at, const std::initializer_list<T>& vals)
{
	size_t idx = (size_t) (at._pEntry - _pData);
#if DUPLICATE_STD_TESTS	
	_data.insert(_data.begin() + idx, vals.begin(), vals.end());
#endif

	auto begin = vals.begin();
	auto end = vals.end();

	size_t entriesNeeded = end - begin;
	resize(_size + entriesNeeded);
	for (size_t i = _size - 1; i >= idx + entriesNeeded; i--)
		_pData[i] = _pData[i - entriesNeeded];

	for (auto iter = begin; iter != end; iter++) {
		_pData[idx++] = *iter;
	}
}

TEMPL_DECL
VECTOR_DECL::iterator VECTOR_DECL::erase(const iterator& at)
{
	size_t idx = (size_t)(at._pEntry - _pData);
#if DUPLICATE_STD_TESTS	
	_data.erase(_data.begin() + idx);
#endif

	for (size_t i = idx; i < _size - 1; i++) {
		_pData[i] = _pData[i + 1];
	}
	_size--;
	return iterator(this, at._pEntry);
}

TEMPL_DECL
VECTOR_DECL::iterator VECTOR_DECL::erase(const iterator& begin, const iterator& end)
{
#if DUPLICATE_STD_TESTS	
	_data.erase(begin, end);
#endif
	return iterator(this, begin._pEntry);
}

TEMPL_DECL
MultiCore::vector<T>& VECTOR_DECL::operator = (const vector& rhs)
{
	if (_pData) {
		free(_pData);
		_pData = nullptr;
	}
	_size = rhs._size;
	_capacity = _size;

	if (_capacity > 0) {
		_pData = alloc<T>(_capacity);
		for (size_t i = 0; i < _size; i++)
			_pData[i] = rhs._pData[i];
	}

	return *this;
}

TEMPL_DECL
_NODISCARD _CONSTEXPR20 typename VECTOR_DECL::const_iterator VECTOR_DECL::begin() const noexcept
{
	return const_iterator(this, _pData);
}

TEMPL_DECL
_NODISCARD _CONSTEXPR20 typename VECTOR_DECL::iterator VECTOR_DECL::begin() noexcept
{
	return iterator(this, _pData);
}

TEMPL_DECL
_NODISCARD _CONSTEXPR20 typename VECTOR_DECL::const_iterator VECTOR_DECL::end() const noexcept
{
	return const_iterator(this, _pData + _size);
}

TEMPL_DECL
_NODISCARD _CONSTEXPR20 typename VECTOR_DECL::iterator VECTOR_DECL::end() noexcept
{
	return iterator(this, _pData + _size);
}

TEMPL_DECL
_NODISCARD _CONSTEXPR20 typename VECTOR_DECL::const_reverse_iterator VECTOR_DECL::rbegin() const noexcept
{
	return const_reverse_iterator(this, _pData + _size - 1);
}

TEMPL_DECL
_NODISCARD _CONSTEXPR20 typename VECTOR_DECL::reverse_iterator VECTOR_DECL::rbegin() noexcept
{
	return reverse_iterator(this, _pData + _size - 1);
}

TEMPL_DECL
_NODISCARD _CONSTEXPR20 typename VECTOR_DECL::const_reverse_iterator VECTOR_DECL::rend() const noexcept
{
	return const_reverse_iterator(this, _pData - 1);
}

TEMPL_DECL
_NODISCARD _CONSTEXPR20 typename VECTOR_DECL::reverse_iterator VECTOR_DECL::rend() noexcept
{
	return reverse_iterator(this, _pData - 1);
}

TEMPL_DECL
const T& VECTOR_DECL::front() const
{
	assert(!empty());
	size_t idx = 0;
	const T* pTData = (const T*)_pData;
	assert(data[idx] == pTData[idx]);
	return pTData[idx];
}

TEMPL_DECL
T& VECTOR_DECL::front()
{
	assert(!empty());
	size_t idx = 0;
	T* pTData = (T*)_pData;
#if DUPLICATE_STD_TESTS	
	assert(_data[idx] == pTData[idx]);
#endif
	return pTData[idx];
}

TEMPL_DECL
const T& VECTOR_DECL::back() const
{
	assert(!empty());
	size_t idx = _size - 1;
	if (idx >= _size) {
		assert(!"Out of bounds");
	}
	const T* pTData = (const T*)_pData;
#if DUPLICATE_STD_TESTS	
	assert(_data[idx] == pTData[idx]);
#endif
	return pTData[idx];
}

TEMPL_DECL
T& VECTOR_DECL::back()
{
	assert(!empty());
	size_t idx = _size - 1;
	if (idx >= _size) {
		assert(!"Out of bounds");
	}

	T* pTData = (T*)_pData;
#if DUPLICATE_STD_TESTS	
	assert(_data[idx] == pTData[idx]);
#endif
	return pTData[idx];
}

TEMPL_DECL
const T& VECTOR_DECL::operator[](size_t idx) const
{
	if (idx >= _size) {
		assert(!"Out of bounds");
	}
	return _pData[idx];
}

TEMPL_DECL
T& VECTOR_DECL::operator[](size_t idx)
{
	if (idx >= _size) {
		assert(!"Out of bounds");
	}
	return _pData[idx];
}

TEMPL_DECL
size_t VECTOR_DECL::push_back(const T& val)
{
#if DUPLICATE_STD_TESTS	
	_data.push_back(val);
#endif

	if (_size + 1 > _capacity) {
		size_t newCapacity = _capacity;
		if (newCapacity == 0)
			newCapacity = 10;
		else
			newCapacity += newCapacity; // Double each time

		reserve(newCapacity);
	}

	_pData[_size] = val;
	_size += 1;

	return _size;
}

TEMPL_DECL
void VECTOR_DECL::pop_back()
{
#if DUPLICATE_STD_TESTS	
	_data.pop_back();
#endif

	assert(_size > 0);
	_size--;
}

TEMPL_DECL
ITER_TEMPL_DECL
ITER_DECL::_iterator(const MultiCore::vector<T>* pSource, T* pEntry)
	: _pSource(const_cast<MultiCore::vector<T>*>(pSource))
	, _pEntry(pEntry)
{
}

TEMPL_DECL
ITER_TEMPL_DECL
bool ITER_DECL::operator == (const _iterator& rhs) const
{
	assert(_pSource == _pSource);
	return _pEntry == rhs._pEntry;
}

TEMPL_DECL
ITER_TEMPL_DECL
bool ITER_DECL::operator != (const _iterator& rhs) const
{
	assert(_pSource == _pSource);
	return _pEntry != rhs._pEntry;
}

TEMPL_DECL
ITER_TEMPL_DECL
bool ITER_DECL::operator < (const _iterator& rhs) const
{
	assert(_pSource == _pSource);
	return _pEntry < rhs._pEntry;
}

TEMPL_DECL
ITER_TEMPL_DECL
bool ITER_DECL::operator > (const _iterator& rhs) const
{
	assert(_pSource == _pSource);
	return _pEntry > rhs._pEntry;
}

TEMPL_DECL
ITER_TEMPL_DECL
ITER_DECL& ITER_DECL::operator ++ ()
{
	if (IterType == FORW_CONST || IterType == FORW)
		_pEntry++;
	else
		_pEntry--;
	return *this;
}

TEMPL_DECL
ITER_TEMPL_DECL
ITER_DECL& ITER_DECL::operator ++ (int)
{
	if (IterType == FORW_CONST || IterType == FORW)
		_pEntry++;
	else
		_pEntry--;
	return *this;
}

TEMPL_DECL
ITER_TEMPL_DECL
ITER_DECL& ITER_DECL::operator --()
{
	if (IterType == FORW_CONST || IterType == FORW)
		_pEntry--;
	else
		_pEntry++;
	return *this;
}

TEMPL_DECL
ITER_TEMPL_DECL
ITER_DECL& ITER_DECL::operator --(int)
{
	if (IterType == FORW_CONST || IterType == FORW)
		_pEntry--;
	else
		_pEntry++;
	return *this;
}

TEMPL_DECL
ITER_TEMPL_DECL
ITER_DECL ITER_DECL::operator + (size_t rhs) const
{
	_iterator result(*this);
	if (IterType == FORW_CONST || IterType == FORW)
		result._pEntry += rhs;
	else
		result._pEntry -= rhs;
	return result;
}

TEMPL_DECL
ITER_TEMPL_DECL
ITER_DECL ITER_DECL::operator - (size_t rhs) const
{
	_iterator result(*this);
	if (IterType == FORW_CONST || IterType == FORW)
		result._pEntry -= rhs;
	else
		result._pEntry += rhs;
	return result;
}

TEMPL_DECL
ITER_TEMPL_DECL
size_t ITER_DECL::operator - (const _iterator& rhs) const
{
	assert(_pSource == rhs._pSource);
	if (IterType == FORW_CONST || IterType == FORW) {
		return (size_t)(_pEntry - rhs._pEntry);
	} else {
		return (size_t)(rhs._pEntry - _pEntry);
	}
}

TEMPL_DECL
ITER_TEMPL_DECL
T& ITER_DECL::operator *() const
{
	return *_pEntry;
}

TEMPL_DECL
ITER_TEMPL_DECL
T* ITER_DECL::operator->() const
{
	return _pEntry;
}

}