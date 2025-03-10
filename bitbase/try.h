/**
 * @license
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Volker Böhm
 * @copyright Copyright (c) 2021 Volker Böhm
 * @Overview
 * Implements a string try for compression
 */

#ifndef _QAPLA_COMPRESS_TRY_H
#define _QAPLA_COMPRESS_TRY_H

#include <cstdint>
#include <assert.h>
#include <vector>

using namespace std;

namespace QaplaCompress {

	enum class NodeType {
		Empty, Index, Bucket, List
	};

	typedef pair<uint32_t, uint32_t> compressRef_t;

	template <class T>
	class Reference {
	public:
		Reference() : _reference(0) {}
		Reference(T reference) : _reference(reference) {}
		Reference(NodeType nodeType, T reference) : Reference((reference << 3) + T(nodeType)) {};

		bool operator==(Reference<T> reference) const { return reference._reference == _reference; }
		bool operator!=(Reference<T> reference) const { return reference._reference != _reference; }

		void set(NodeType nodeType, T reference) { _reference = (reference << 3) + T(nodeType); }

		T reference() const { return _reference >> 3; }
		NodeType nodeType() const { return NodeType(_reference & 7); }

		bool isEmpty() const {
			return nodeType() == NodeType::Empty;
		}

	private:
		T _reference;
	};

	template <class T>
	class RefList {
	public:
		RefList() {
			clear();
		}

		uint32_t getCount() const { 
			return _number; 
		}

		void clear() {
			_number = 0;
		}

		Reference<T> operator[](uint8_t content) const { 
			Reference<T> result = 0;
			for (uint32_t i = 0; i < _number; i++) {
				if (_contentBytes[i] == content) {
					result = _references[i];
					break;
				}
			}
			return result;
		}

		uint8_t content(uint32_t index) const { return _contentBytes[index]; }
		Reference<T> reference(uint32_t index) const { return _references[index]; }

		bool set(uint8_t content, Reference<T> ref, T rootIndex) {
			_latestIndex = rootIndex;
			for (uint32_t i = 0; i < _number; i++) {
				if (_contentBytes[i] == content) {
					_references[i] = ref;
					return true;
				}
			}
			if (!hasSpace()) {
				return false;
			}
			_contentBytes[_number] = content;
			_references[_number] = ref;
			_number++;
			return true;
		}

		bool hasSpace() const {
			return _number < MAX_COUNT;
		}

		T getLatestIndex() const {
			return _latestIndex;
		}

	private:
		uint32_t _number;
		static const uint32_t MAX_COUNT = 4;
		array<uint8_t, MAX_COUNT> _contentBytes;
		array<Reference<T>, MAX_COUNT> _references;
		T _latestIndex;
	};

	template <class T>
	class RefSingelton {
	public:
		RefSingelton() { clear(); }

		void clear() {
			_ref = 0;
		}

		Reference<T> operator[](uint8_t content) const { 
			if (content == _content) return _ref;
			return 0;
		}

		void set(uint8_t content, Reference<T> ref) {
			_content = content, _ref = ref;
		}

		bool hasSpace() const {
			return false;
		}

	private:
		uint8_t _content;
		Reference<T> _ref;
	};


	template <class T>
	class RefBucket {
	public:
		RefBucket() { clear(); }
		RefBucket(const RefList<T>& list) : RefBucket() {
			for (uint32_t i = 0; i < list.getCount(); i++) {
				set(list.content(i), list.reference(i), list.getLatestIndex());
			}
		}
		Reference<T> operator[](uint8_t content) const { return _bucket[content]; }
		
		void set(uint8_t content, Reference<T> ref, T rootIndex) {
			_bucket[content] = ref;
			_latestIndex = rootIndex;
		}

		bool hasSpace() const {
			return true;
		}

		void clear() { 
			_bucket.fill(0); 
		}

		T getLatestIndex() const {
			return _latestIndex;
		}

	private:
		array<Reference<T>, 256> _bucket;
		T _latestIndex;
	};


	template <class T>
	class Try {
	public:
		Try(uint32_t maxDepth = 16) {
			_buckets.reserve(BUCKETS_GROWTH);
			_freeLists.reserve(MAX_LISTS);
			_lists.reserve(MAX_LISTS);
			_maxDepth = maxDepth;
			for (uint32_t i = MAX_LISTS; i > 0; --i) {
				_lists.emplace_back();
				_freeLists.push_back(i - 1);
			}
		}
		/**
		 * Adds a data element to the try
		 */
		void add(const vector<uint8_t>& data, T index) {
			if (_buckets.size() == _buckets.capacity()) {
				_buckets.reserve(_buckets.size() + BUCKETS_GROWTH);
			}
			uint8_t content = data[index];
			Reference<T> ref = _root[content];
			Reference<T> refBefore;
			T depth = search(ref, refBefore, data, index + 1);
			if (depth >= 2) {
				addToNodeRec(refBefore, data, index, depth - 1);
			}
			else {
				addToBucketRec(_root, data, index, 0);
			}
		}

		/**
		 * Searches for an existing data element
		 */
		compressRef_t searchAndAdd(const vector<uint8_t>& data, T index) {
			uint8_t content = data[index];
			Reference<T> ref = _root[content];
			Reference<T> refBefore;
			T depth = search(ref, refBefore, data, index + 1);
			
			// Get the result before adding the new element to ensure that 
			// we do not return anything of the added elements
			compressRef_t result = getResult(ref, data, index, depth);
			
			if (depth >= 2) {
				addToNodeRec(refBefore, data, index, depth - 1);
			}
			else {
				addToBucketRec(_root, data, index, 0);
			}

			return result;
		}

		/**
		 * Prints a memory usage statistic
		 */
		void print() const {
			cout << "Buckets used: " << _buckets.size()
				<< " Lists used: " << (MAX_LISTS - _freeLists.size())
				<< endl;
		}

		/**
		 * Clears all elements
		 */
		void clear() {
			// print();
			_root.clear();
			_buckets.clear();
			_lists.clear();
			_freeLists.clear();
			for (uint32_t i = MAX_LISTS; i > 0; --i) {
				_lists.emplace_back();
				_freeLists.push_back(i - 1);
			}
		}

	private:

		/**
		 * Gets the resulting information after a successful search
 		 * @param ref Element found
		 * @param data data vector
		 * @param index index of the data vector
		 * @param depth amount of matching data elements
		 * @returns { start index, amount of matching elements }
		 */
		compressRef_t getResult(Reference<T> ref, const vector<uint8_t>& data, T index, T depth) {
			NodeType nodeType = ref.nodeType();
			T reference = ref.reference();

			compressRef_t result(0, 0);
			if (nodeType == NodeType::Bucket) {
				result = compressRef_t(_buckets[reference].getLatestIndex(), depth);
			}
			else if (nodeType == NodeType::List) {
				result = compressRef_t(_lists[reference].getLatestIndex(), depth);
			}
			else if (nodeType == NodeType::Index) {
				index += depth;
				for (; index < data.size() && data[reference + depth] == data[index]; index++, depth++);
				result = compressRef_t(reference, depth);
			}
			return result;
		}

		/**
		 * Searches for an existing data element
		 * @param ref (in/out) Start element/ element found
		 * @param refBefore (out) Element before the element found
		 * @param data data vector
		 * @param index index of the data vector
		 * @returns depth in the tree, where the ref has been found (= amount of matching elements)
		 */
		T search(Reference<T>& ref, Reference<T>& refBefore, const vector<uint8_t>& data, T index) {
			uint32_t depth = 0;
			Reference<T> probe = ref;
			for (; index < data.size() && !probe.isEmpty(); index++) {
				depth++;
				refBefore = ref;
				ref = probe;
				NodeType nodeType = ref.nodeType();
				T reference = ref.reference();
				uint8_t content = data[index];

				if (nodeType == NodeType::Bucket) {
					probe = _buckets[reference][content];
				}
				else if (nodeType == NodeType::List) {
					probe = _lists[reference][content];
				}
				else {
					break;
				}
			}
			return depth;
		}

		/**
		 * Recursively adds an element to a bucket
		 */
		void addToBucketRec(RefBucket<T>& bucket, const vector<uint8_t>& data, T index, T depth) {
			uint8_t content = data[index + depth];
			Reference<T> ref = bucket[content];
			if (ref.isEmpty()) {
				bucket.set(content, Reference<T>(NodeType::Index, index), index);
			}
			else {
				Reference<T> newRef = addToNodeRec(ref, data, index, depth + 1);
				bucket.set(content, newRef, index);
			}
		}
		
		/**
		 * Recursively adds an element to a list
		 */
		bool addToListRec(RefList<T>& list, const vector<uint8_t>& data, T index, T depth) {
			uint8_t content = data[index + depth];
			Reference<T> ref = list[content];
			bool result = true;
			if (ref.isEmpty()) {
				if (list.hasSpace()) {
					list.set(content, Reference<T>(NodeType::Index, index), index);
				}
				else {
					result = false;
				}
			}
			else {
				Reference<T> newRef = addToNodeRec(ref, data, index, depth + 1);
				if (ref != newRef) {
					list.set(content, newRef, index);
				}
			}
			return result;
		}

		/**
		 * Adds content recursively
		 * @param ref reference to a node
		 * @param data input data stream
		 * @param index start index to the input data stream
		 * @param depth recursion depth = amount of bytes inserted to the tree
		 */
		Reference<T> addToNodeRec(Reference<T> ref, const vector<uint8_t>& data, T index, T depth) {
			NodeType nodeType = ref.nodeType();
			T reference = ref.reference();
			Reference<T> result = ref;
			if (index + depth >= data.size()) {
				return result;
			}

			if (nodeType == NodeType::Bucket) {
				addToBucketRec(_buckets[reference], data, index, depth);
			}
			else if (nodeType == NodeType::List) {
				if (!addToListRec(_lists[reference], data, index, depth)) {
					_buckets.emplace_back(_lists[reference]);
					_lists[reference].clear();
					_freeLists.push_back(reference);
					addToBucketRec(_buckets.back(), data, index, depth);
					result = Reference<T>(NodeType::Bucket, T(_buckets.size() - 1));
				}
			} 
			else if (nodeType == NodeType::Index) {
				if (depth >= _maxDepth || _freeLists.size() == 0) {
					result = Reference<T>(NodeType::Index, index);
				}
				else {
					T listRef = _freeLists.back();
					_freeLists.pop_back();
					result = Reference<T>(NodeType::List, listRef);
					addToListRec(_lists[listRef], data, index, depth);
					addToListRec(_lists[listRef], data, ref.reference(), depth);
				}
			}
			else {
				// empty must be handled before - other NodeTypes are not defined
				assert(false);
			}
			return result;
		}

		static const uint32_t MAX_LISTS = 0x20000;
		static const uint32_t MAX_SINGELTONS = 8192;
		static const uint32_t BUCKETS_GROWTH = 8192;
		uint32_t _maxDepth;
		RefBucket<T> _root;
		vector<uint32_t> _freeLists;
		vector<RefBucket<T>> _buckets;
		array<RefSingelton<T>, MAX_SINGELTONS> _singeltons;
		vector<RefList<T>> _lists;
	};

}


#endif		// _QAPLA_COMPRESS_TRY_H