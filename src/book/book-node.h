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
 * The node of a book file: a packed move, two tree flags, a priority and an
 * arbitrary flat payload.
 *
 * The bit code of the 16 bit part is the one of the Spike books
 * (msb) MMMM MMMM MMMP PPCS (lsb)
 * where
 * M = packed move
 * P = priority
 * C = the node has a child
 * S = the node has a right sibling
 *
 * The payload is appended to those 16 bits and is copied in and out flat, so it
 * has to be trivially copyable - the whole node array is read and written with
 * a single block transfer. An empty payload costs nothing: with
 * QAPLA_NO_UNIQUE_ADDRESS the node is two bytes and therefore byte identical to
 * the old books.
 *
 * Keep the alignment of a payload at one or two bytes. A payload aligned to
 * four would make the compiler insert two padding bytes behind the 16 bit part
 * and every node in the file would grow by them; the static_assert on the node
 * size in book.h is there to turn that into a compile error rather than a
 * bigger file.
 */

#pragma once

#include <cstdint>
#include <type_traits>

#include "packed-move.h"

 // MSVC keeps its own ABI and ignores the standard attribute, clang-cl
 // understands the MSVC spelling of it.
#if defined(_MSC_VER)
#define QAPLA_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
#define QAPLA_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif

namespace QaplaBook {

	/**
	 * A payload has to be flat copyable, because it is written to and read from
	 * the file as raw memory, and default constructible, because reading a book
	 * that has no payload has to produce one.
	 */
	template <typename PAYLOAD>
	concept BookPayload = std::is_trivially_copyable_v<PAYLOAD>
		&& std::is_default_constructible_v<PAYLOAD>;

	/**
	 * The payload of a book that stores nothing but moves, for example a plain
	 * opening book.
	 */
	struct EmptyPayload {
		constexpr bool operator==(const EmptyPayload& payloadToCompare) const = default;
	};

	/**
	 * The priority of a move: 7 is played most often, a move of priority n - 1
	 * is played 2/3 as often as one of priority n, and a move of priority 0 is
	 * never played.
	 */
	enum bookPriority : uint8_t {
		PRIORITY_NEVER = 0,
		PRIORITY_MAX = 7,
		PRIORITY_DEFAULT = 4
	};

	/**
	 * A node as it is stored in a book file.
	 */
	template <BookPayload PAYLOAD>
	struct PackedBookNode {
		enum nodeBits : uint16_t {
			HAS_CHILD = 0x0001,
			HAS_RIGHT_SIBLING = 0x0002,
			PRIORITY_SHIFT = 2,
			PRIORITY_MASK = 0x001C,
			MOVE_SHIFT = 5,
			MOVE_MASK = 0xFFE0
		};

		uint16_t _bits = 0;
		QAPLA_NO_UNIQUE_ADDRESS PAYLOAD _payload{};

		constexpr PackedMove move() const {
			return PackedMove((_bits & MOVE_MASK) >> MOVE_SHIFT);
		}

		constexpr uint8_t priority() const {
			return uint8_t((_bits & PRIORITY_MASK) >> PRIORITY_SHIFT);
		}

		constexpr bool hasChild() const {
			return (_bits & HAS_CHILD) != 0;
		}

		constexpr bool hasRightSibling() const {
			return (_bits & HAS_RIGHT_SIBLING) != 0;
		}

		constexpr void setMove(PackedMove move) {
			_bits = uint16_t((_bits & ~uint16_t(MOVE_MASK)) | (uint16_t(move) << MOVE_SHIFT));
		}

		constexpr void setPriority(uint8_t priority) {
			_bits = uint16_t((_bits & ~uint16_t(PRIORITY_MASK))
				| ((uint16_t(priority) << PRIORITY_SHIFT) & PRIORITY_MASK));
		}

		constexpr void setChild(bool hasChild) {
			_bits = uint16_t(hasChild ? (_bits | HAS_CHILD) : (_bits & ~uint16_t(HAS_CHILD)));
		}

		constexpr void setRightSibling(bool hasRightSibling) {
			_bits = uint16_t(hasRightSibling ? (_bits | HAS_RIGHT_SIBLING)
				: (_bits & ~uint16_t(HAS_RIGHT_SIBLING)));
		}
	};

	static_assert(std::is_trivially_copyable_v<PackedBookNode<EmptyPayload>>);
	static_assert(sizeof(PackedBookNode<EmptyPayload>) == 2,
		"a book without a payload has to keep the two byte nodes of the old books");

	/**
	 * Merge policy used when a line is added that is already in the book: the
	 * stored payload is replaced.
	 */
	struct OverwritePayload {
		template <typename PAYLOAD>
		constexpr void operator()(PAYLOAD& stored, const PAYLOAD& incoming) const {
			stored = incoming;
		}
	};

	/**
	 * Merge policy that leaves a payload that is already in the book alone.
	 */
	struct KeepPayload {
		template <typename PAYLOAD>
		constexpr void operator()(PAYLOAD&, const PAYLOAD&) const {
		}
	};
}
