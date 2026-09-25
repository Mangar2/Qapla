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
 * @copyright Copyright (c) 2026 Volker Böhm
 * @Overview
 * A book of chess moves: a tree of packed moves, every node carrying a flat
 * payload of the caller's choosing - a priority for an opening book, an
 * evaluation or a win/draw/loss statistic for a library of training positions.
 *
 * In the file the tree is a preorder array, as in the Spike books: the child of
 * the node at position p is at p + 1, its right sibling behind its whole
 * subtree. That is compact but it makes inserting a line cost a move of the
 * whole array, so in memory the tree keeps explicit child and sibling indices
 * instead and the preorder form is built when the book is written. Adding a
 * line is then a walk down the tree, independent of the size of the book.
 *
 * Writing a book needs no board at all: the caller states the moves it played.
 * Only reading one does, because a packed move has no departure square - and
 * all it needs of a board is whether a square is occupied, see BookPosition in
 * packed-move.h.
 */

#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <stdexcept>
#include <vector>

#include "book-io.h"
#include "book-node.h"
#include "packed-move.h"

namespace QaplaBook {

	template <BookPayload PAYLOAD = EmptyPayload>
	class Book {
	public:
		using Node = PackedBookNode<PAYLOAD>;
		using NodeIndex = uint32_t;

		/** The position before the first move. It is not stored in the file. */
		static constexpr NodeIndex ROOT = 0;
		static constexpr NodeIndex NO_NODE = ~NodeIndex(0);

		static_assert(sizeof(Node) <= sizeof(uint16_t) + sizeof(PAYLOAD) + 1,
			"the payload needs an alignment of one or two bytes, otherwise the "
			"compiler pads every single node of the file");

		Book() {
			clear();
		}

		/**
		 * A child move of a node, ready to be played: the move with both
		 * squares, and a flat copy of the payload.
		 */
		struct Child {
			NodeIndex node = NO_NODE;
			BookMove move;
			uint8_t priority = PRIORITY_DEFAULT;
			PAYLOAD payload{};
		};

		// ---------------------- Building -----------------------------------

		/**
		 * Adds a line played from the root position, or updates it if it is
		 * already in the book. Every ply gets the payload of the same index;
		 * merge decides what happens to a node that is already there, it is
		 * called with the stored payload and the new one.
		 * Returns the node of the last move of the line.
		 * Throws std::invalid_argument if a move cannot be packed.
		 */
		template <typename MERGE = OverwritePayload>
		NodeIndex addLine(std::span<const BookMove> moves,
			std::span<const PAYLOAD> payloads, MERGE merge = {}) {
			assert(payloads.empty() || payloads.size() == moves.size());
			NodeIndex current = ROOT;
			for (size_t ply = 0; ply < moves.size(); ++ply) {
				current = addMove(current, moves[ply]);
				if (ply < payloads.size()) {
					merge(_nodes[current]._payload, payloads[ply]);
				}
			}
			return current;
		}

		/**
		 * Adds a line without touching any payload: a new node gets the default
		 * payload, a node that is already in the book keeps what it has.
		 */
		NodeIndex addLine(std::span<const BookMove> moves) {
			return addLine(moves, std::span<const PAYLOAD>{}, KeepPayload{});
		}

		/**
		 * Adds a single move below a node, or returns the node of that move if
		 * it is already there.
		 * Throws std::invalid_argument if the move cannot be packed.
		 */
		NodeIndex addMove(NodeIndex parent, const BookMove& move) {
			const auto packed = packMove(move);
			if (!packed) {
				throw std::invalid_argument("book: move has no direction the format can pack");
			}
			return addPackedMove(parent, *packed);
		}

		/**
		 * Adds a packed move below a node, or returns the node of that move if
		 * it is already there.
		 */
		NodeIndex addPackedMove(NodeIndex parent, PackedMove move) {
			assert(parent < _nodes.size());
			const NodeIndex existing = findPackedMove(parent, move);
			if (existing != NO_NODE) return existing;

			const NodeIndex child = NodeIndex(_nodes.size());
			_nodes.push_back(Entry{ ._move = move });
			if (_nodes[parent]._firstChild == NO_NODE) {
				_nodes[parent]._firstChild = child;
			}
			else {
				_nodes[_nodes[parent]._lastChild]._nextSibling = child;
			}
			_nodes[parent]._lastChild = child;
			return child;
		}

		/**
		 * Drops all moves, keeping the header.
		 */
		void clear() {
			_nodes.clear();
			// The root is a node like any other, it only has no move. Having it
			// in the array keeps every walk below free of special cases.
			_nodes.push_back(Entry{});
		}

		// ---------------------- Reading -------------------------------------

		/**
		 * Follows a line from the root position.
		 * Returns the node of the last move, or NO_NODE if the line is not in
		 * the book.
		 */
		NodeIndex findLine(std::span<const BookMove> moves) const {
			NodeIndex current = ROOT;
			for (const BookMove& move : moves) {
				const auto packed = packMove(move);
				if (!packed) return NO_NODE;
				current = findPackedMove(current, *packed);
				if (current == NO_NODE) return NO_NODE;
			}
			return current;
		}

		/**
		 * Searches one move below a node.
		 * Returns the node of the move, or NO_NODE if it is not in the book.
		 */
		NodeIndex findPackedMove(NodeIndex parent, PackedMove move) const {
			assert(parent < _nodes.size());
			for (NodeIndex child = _nodes[parent]._firstChild; child != NO_NODE;
				child = _nodes[child]._nextSibling) {
				if (_nodes[child]._move == move) return child;
			}
			return NO_NODE;
		}

		/**
		 * All moves the book holds below a node, unpacked against the position
		 * the node stands for, with a flat copy of every payload.
		 */
		template <BookPosition POSITION>
		std::vector<Child> children(NodeIndex parent, const POSITION& position) const {
			assert(parent < _nodes.size());
			std::vector<Child> children;
			for (NodeIndex child = _nodes[parent]._firstChild; child != NO_NODE;
				child = _nodes[child]._nextSibling) {
				const Entry& entry = _nodes[child];
				children.push_back(Child{
					.node = child,
					.move = unpackMove(entry._move, position),
					.priority = entry._priority,
					.payload = entry._payload });
			}
			return children;
		}

		/**
		 * The move of a node, unpacked against the position it is played from.
		 */
		template <BookPosition POSITION>
		BookMove move(NodeIndex node, const POSITION& position) const {
			assert(node != ROOT && node < _nodes.size());
			return unpackMove(_nodes[node]._move, position);
		}

		PackedMove packedMove(NodeIndex node) const {
			assert(node < _nodes.size());
			return _nodes[node]._move;
		}

		NodeIndex firstChild(NodeIndex node) const {
			assert(node < _nodes.size());
			return _nodes[node]._firstChild;
		}

		NodeIndex nextSibling(NodeIndex node) const {
			assert(node < _nodes.size());
			return _nodes[node]._nextSibling;
		}

		/** Copies the payload of a node out. */
		PAYLOAD payload(NodeIndex node) const {
			assert(node < _nodes.size());
			return _nodes[node]._payload;
		}

		void setPayload(NodeIndex node, const PAYLOAD& payload) {
			assert(node < _nodes.size());
			_nodes[node]._payload = payload;
		}

		uint8_t priority(NodeIndex node) const {
			assert(node < _nodes.size());
			return _nodes[node]._priority;
		}

		void setPriority(NodeIndex node, uint8_t priority) {
			assert(node < _nodes.size() && priority <= PRIORITY_MAX);
			_nodes[node]._priority = priority;
		}

		/** The number of moves in the book, the root not counted. */
		size_t size() const {
			return _nodes.size() - 1;
		}

		const BookHeader& header() const {
			return _header;
		}

		BookHeader& header() {
			return _header;
		}

		// ---------------------- File and memory image -----------------------

		/**
		 * Writes the book, node array in preorder behind the header.
		 * Throws BookFormatError if the file cannot be written.
		 */
		void writeToFile(const std::filesystem::path& path) const {
			std::ofstream stream(path, std::ios::binary | std::ios::trunc);
			if (!stream) throw BookFormatError("cannot write " + path.string());
			const std::vector<Node> nodes = toPreorder();
			BookHeader header = _header;
			header.fileVersion = BOOK_FILE_VERSION;
			header.nodeSize = uint32_t(sizeof(Node));
			writeBookHeader(stream, header, uint32_t(nodes.size()));
			if (!nodes.empty()) {
				stream.write(reinterpret_cast<const char*>(nodes.data()),
					std::streamsize(nodes.size() * sizeof(Node)));
			}
			if (!stream) throw BookFormatError("writing " + path.string() + " failed");
		}

		/**
		 * Reads a book, replacing everything the book held.
		 * A file written without a payload - every Spike book - is read into a
		 * book of any payload type, every node getting the default payload. A
		 * file whose nodes have a different size than this book's nodes is
		 * refused: its bytes would silently mean something else.
		 * Returns false if the file cannot be opened, throws BookFormatError if
		 * it is not a book this code can read.
		 */
		bool readFromFile(const std::filesystem::path& path) {
			std::ifstream stream(path, std::ios::binary);
			if (!stream) return false;
			const uint32_t nodeCount = readBookHeader(stream, _header);
			std::vector<Node> nodes(nodeCount);
			if (_header.nodeSize == sizeof(Node)) {
				readNodes(stream, path, std::span<Node>(nodes));
			}
			else if (_header.nodeSize == sizeof(uint16_t)) {
				std::vector<uint16_t> bits(nodeCount);
				readNodes(stream, path, std::span<uint16_t>(bits));
				for (size_t index = 0; index < nodes.size(); ++index) {
					nodes[index]._bits = bits[index];
				}
			}
			else {
				throw BookFormatError(path.string() + " holds nodes of "
					+ std::to_string(_header.nodeSize) + " bytes, this book has "
					+ std::to_string(sizeof(Node)));
			}
			fromPreorder(nodes);
			return true;
		}

		/**
		 * The tree as the compact preorder array of the file format.
		 */
		std::vector<Node> toPreorder() const {
			std::vector<Node> nodes;
			nodes.reserve(size());
			std::vector<NodeIndex> stack;
			pushChildren(ROOT, stack);
			while (!stack.empty()) {
				const NodeIndex index = stack.back();
				stack.pop_back();
				const Entry& entry = _nodes[index];
				Node node;
				node.setMove(entry._move);
				node.setPriority(entry._priority);
				node.setChild(entry._firstChild != NO_NODE);
				node.setRightSibling(entry._nextSibling != NO_NODE);
				node._payload = entry._payload;
				nodes.push_back(node);
				pushChildren(index, stack);
			}
			assert(nodes.size() == size());
			return nodes;
		}

		/**
		 * Rebuilds the tree from a preorder array, replacing everything the book
		 * held. Throws BookFormatError if the array does not describe a tree.
		 */
		void fromPreorder(std::span<const Node> nodes) {
			clear();
			_nodes.reserve(nodes.size() + 1);
			// One frame per level of the tree that is still being filled: the
			// node the level hangs below, its last child so far, and whether
			// that node has a sibling of its own - which is what tells us how
			// many levels are finished once a node without a sibling is read.
			struct Frame {
				NodeIndex parent = ROOT;
				bool parentHasSibling = false;
			};
			std::vector<Frame> frames;
			frames.push_back(Frame{});

			for (const Node& node : nodes) {
				if (frames.empty()) throw BookFormatError("preorder array has trailing nodes");
				const NodeIndex index = appendChild(frames.back().parent, node);
				if (node.hasChild()) {
					frames.push_back(Frame{ .parent = index,
						.parentHasSibling = node.hasRightSibling() });
				}
				else if (!node.hasRightSibling()) {
					for (bool levelIsDone = true; levelIsDone && !frames.empty();) {
						levelIsDone = !frames.back().parentHasSibling;
						frames.pop_back();
					}
				}
			}
			if (_nodes.size() != nodes.size() + 1) {
				throw BookFormatError("preorder array is not a tree");
			}
		}

	private:
		/**
		 * A node while the book is in memory: the packed move and the payload as
		 * they go into the file, plus the links that make inserting cheap.
		 */
		struct Entry {
			PackedMove _move = PACKED_MOVE_NONE;
			uint8_t _priority = PRIORITY_DEFAULT;
			PAYLOAD _payload{};
			NodeIndex _firstChild = NO_NODE;
			NodeIndex _lastChild = NO_NODE;
			NodeIndex _nextSibling = NO_NODE;
		};

		/**
		 * Pushes the children of a node on the stack, last child first, so that
		 * popping walks them from left to right.
		 */
		void pushChildren(NodeIndex parent, std::vector<NodeIndex>& stack) const {
			const size_t firstPushed = stack.size();
			for (NodeIndex child = _nodes[parent]._firstChild; child != NO_NODE;
				child = _nodes[child]._nextSibling) {
				stack.push_back(child);
			}
			std::reverse(stack.begin() + firstPushed, stack.end());
		}

		/**
		 * Appends a node read from a file below a parent, keeping the order of
		 * the file.
		 */
		NodeIndex appendChild(NodeIndex parent, const Node& node) {
			const NodeIndex index = NodeIndex(_nodes.size());
			_nodes.push_back(Entry{ ._move = node.move(), ._priority = node.priority(),
				._payload = node._payload });
			if (_nodes[parent]._firstChild == NO_NODE) {
				_nodes[parent]._firstChild = index;
			}
			else {
				_nodes[_nodes[parent]._lastChild]._nextSibling = index;
			}
			_nodes[parent]._lastChild = index;
			return index;
		}

		template <typename RECORD>
		static void readNodes(std::istream& stream, const std::filesystem::path& path,
			std::span<RECORD> records) {
			if (records.empty()) return;
			stream.read(reinterpret_cast<char*>(records.data()),
				std::streamsize(records.size() * sizeof(RECORD)));
			if (!stream) throw BookFormatError(path.string() + " ends inside the node array");
		}

		std::vector<Entry> _nodes;
		BookHeader _header;
	};
}
