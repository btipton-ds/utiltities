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

#include <assert.h>
#include <local_heap.h>
#include <pool_vector.h>

#define TEMPL_DECL template<class T> 
#define SET_DECL set<T> 

namespace MultiCore {

TEMPL_DECL
inline SET_DECL::set(const MultiCore::vector<T>& src)
{
	insert(src.begin(), src.end());
}

TEMPL_DECL
inline SET_DECL::set(const std::set<T>& src)
{
	if (!src.empty())
		insert(src.begin(), src.end());
}

TEMPL_DECL
inline SET_DECL::set(const std::initializer_list<T>& src)
{
	insert(src.begin(), src.end());
}

TEMPL_DECL
SET_DECL::operator std::set<T>() const
{
	std::set<T> result;
	result.insert(begin(), end());
	return result;
}

TEMPL_DECL
inline SET_DECL::~set()
{
}

TEMPL_DECL
template<class ITER_TYPE>
void SET_DECL::insert(const ITER_TYPE& begin, const ITER_TYPE& end)
{
#if DUPLICATE_STD_TESTS	
	_set.insert(begin, end);
#endif

	for (auto iter = begin; iter != end; iter++) {
		insert(*iter);
	}
}

TEMPL_DECL
inline bool SET_DECL::empty() const
{
	return vector<T>::empty();
}

TEMPL_DECL
inline size_t SET_DECL::size() const
{
	return vector<T>::size();
}

TEMPL_DECL
inline void SET_DECL::clear()
{
	vector<T>::clear();
}

TEMPL_DECL
typename SET_DECL::const_iterator SET_DECL::insert(const T& val)
{
#if DUPLICATE_STD_TESTS	
	_set.insert(val);
#endif
	const_iterator iter, nextIter;
	iter = find(val, nextIter);
	if (iter == end()) {
		return vector<T>::insert(nextIter, val);
	}
	return iter;
}

TEMPL_DECL
void SET_DECL::insert(const std::initializer_list<T>& vals)
{
#if DUPLICATE_STD_TESTS	
	_set.insert(vals.begin(), vals.end());
#endif

	for (auto iter = begin; iter != end; iter++) {
		insert(*iter);
	}
}

TEMPL_DECL
inline void SET_DECL::erase(const const_iterator& at)
{
#if DUPLICATE_STD_TESTS	
	//	_set.erase(_set.begin() + idx);
#endif

	vector<T>::erase(at);
}

TEMPL_DECL
inline void SET_DECL::erase(const T& val)
{
	auto iter = find(val);
	if (iter != end())
		erase(iter);
}

TEMPL_DECL
inline void SET_DECL::erase(const const_iterator& begin, const const_iterator& end)
{
#if DUPLICATE_STD_TESTS	
	//	_set.erase(begin, end);
#endif
	vector<T>::erase(begin, end);
}

TEMPL_DECL
inline MultiCore::set<T>& SET_DECL::operator = (const set& rhs)
{
#if DUPLICATE_STD_TESTS	
	_set = rhs._set;
#endif
	vector<T>::operator = (rhs);

	return *this;
}

TEMPL_DECL
inline typename SET_DECL::const_iterator SET_DECL::begin() const noexcept
{
	return vector<T>::begin();
}

TEMPL_DECL
inline typename SET_DECL::const_iterator SET_DECL::end() const noexcept
{
	return vector<T>::end();
}

TEMPL_DECL
inline typename SET_DECL::const_reverse_iterator SET_DECL::rbegin() const noexcept
{
	return vector<T>::rbegin();
}

TEMPL_DECL
inline typename SET_DECL::const_reverse_iterator SET_DECL::rend() const noexcept
{
	return vector<T>::rend();
}

TEMPL_DECL
inline typename SET_DECL::const_iterator SET_DECL::find(const T& val) const noexcept
{
	const_iterator next;
	return find(val, next);
}

TEMPL_DECL
inline bool SET_DECL::contains(const T& val) const
{
	return find(val) != end();
}

TEMPL_DECL
inline size_t SET_DECL::count(const T& val) const
{
	return find(val) == end() ? 0 : 1;
}

TEMPL_DECL
typename SET_DECL::const_iterator SET_DECL::find(const T& val, const_iterator& next) const noexcept
{
	next = end();
	size_t min = 0;
	size_t max = size();
	size_t idx = (min + max) / 2;
	while (idx < size() && min != max) {
		const auto& curVal = vector<T>::operator[](idx);
		if (val < curVal) {
			if (max == idx) {
				assert(!"Unexpected/impossible condition.");
			}
			max = idx;
		} else if (curVal < val) {
			if (min == idx) {
				if (idx < size() - 1)
					next = const_iterator(this, vector<T>::data() + idx + 1);
				else
					next = end();
				return end();

			}
			min = idx;
		} else {
			if (idx < size() - 1)
				next = const_iterator(this, vector<T>::data() + idx + 1);
			else
				next = end();
			return const_iterator(this, vector<T>::data() + idx);
		}

		idx = (min + max) / 2;
	}

	if (idx < size()) {
		next = const_iterator(this, vector<T>::data() + idx);
	}

	return end();
}

}

#undef TEMPL_DECL 
#undef SET_DECL 
