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
 * The node of a 9 bit book file: a packed move, two tree flags, a priority and
 * an arbitrary flat payload.
 *
 * The bit code of the 16 bit part is
 * (msb) MMMM MMMM MPPP PPCS (lsb)
 * where
 * M = packed move
 * P = priority, five bits - the two bits the move gave up
 * C = the node has a child
 * S = the node has a right sibling
 *
 * The payload, the payload concept and the merge policies are the ones of the
 * 11 bit book, see ../book/book-node.h; only the bit layout differs.
 */

#pragma once

#include <cstdint>
#include <type_traits>

#include "../book/book-node.h"
#include "packed-move.h"

namespace QaplaBook9 {

	using QaplaBook::BookPayload;
	using QaplaBook::EmptyPayload;
	using QaplaBook::KeepPayload;
	using QaplaBook::OverwritePayload;

	/**
	 * The priority of a move: the higher, the more often it is played, and a
	 * move of priority 0 is never played.
	 */
	enum bookPriority : uint8_t {
		PRIORITY_NEVER = 0,
		PRIORITY_MAX = 31,
		PRIORITY_DEFAULT = 16
	};

	/**
	 * A node as it is stored in a 9 bit book file.
	 */
	template <BookPayload PAYLOAD>
	struct PackedBookNode {
		enum nodeBits : uint16_t {
			HAS_CHILD = 0x0001,
			HAS_RIGHT_SIBLING = 0x0002,
			PRIORITY_SHIFT = 2,
			PRIORITY_MASK = 0x007C,
			MOVE_SHIFT = 7,
			MOVE_MASK = 0xFF80
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
			_bits = uint16_t((_bits & ~uint16_t(MOVE_MASK))
				| ((uint16_t(move) << MOVE_SHIFT) & MOVE_MASK));
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
		"a book without a payload has two byte nodes");
}
