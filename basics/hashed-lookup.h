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
 * Implements piece square table for static evaluation for the piece placement
 */

#pragma once

#include <array>
#include <cstdint>
#include <random>
#include <limits>
#include <optional>

namespace QaplaBasics {


    template <size_t TABLE_SIZE>
    class SignatureHasher {
    public:
        static uint32_t hash(uint32_t white_signature, uint32_t black_signature) {
            const auto& rw = random_white();
            const auto& rb = random_black();
            return rw[white_signature] ^ rb[black_signature];
        }

    private:
        static const std::array<uint32_t, TABLE_SIZE>& random_white() {
            static const std::array<uint32_t, TABLE_SIZE> table = init_table(42);
            return table;
        }

        static const std::array<uint32_t, TABLE_SIZE>& random_black() {
            static const std::array<uint32_t, TABLE_SIZE> table = init_table(1337);
            return table;
        }

        static std::array<uint32_t, TABLE_SIZE> init_table(uint32_t seed) {
            std::array<uint32_t, TABLE_SIZE> table{};
            std::mt19937 rng(seed);
            std::uniform_int_distribution<uint32_t> dist(0, std::numeric_limits<uint32_t>::max());
            for (auto& val : table) val = dist(rng);
            return table;
        }
    };


    /**
     * @brief Generic hash table using externally computed keys and hashes.
     *
     * @tparam KeyT        Type of the key. Must be equality-comparable and hashable.
     * @tparam ValueT      Type of the value to store.
     * @tparam TABLE_SIZE  Size of the table. Should be > expected number of entries.
     */
    template <typename KeyT, typename ValueT, size_t TABLE_SIZE>
    class HashedLookup {
    public:
        /**
         * @brief Inserts a value using the given key and hash.
         *
         * @param key   The key to insert.
         * @param hash  Hash value for the key.
         * @param value The value to store.
         * @return True if successfully inserted, false if table is full.
         */
        bool insert(KeyT key, uint32_t hash, ValueT value) {
            size_t index = hash % TABLE_SIZE;

            for (size_t i = 0; i < TABLE_SIZE; ++i) {
                auto& entry = table[(index + i) % TABLE_SIZE];
                if (entry.key == key) {
                    // overwrite without counting as new insert
                    entry = { key, value };
                    return true;
                }
                if (is_empty(entry.key)) {
                    if (i > 0) total_collisions++;
                    total_inserts++;
                    entry = { key, value };
                    check_warning();
                    return true;
                }
            }

            std::cerr << "ERROR: HashedLookup table is full.\n";
            return false;
        }

        /**
         * @brief Looks up a value by key and its hash.
         *
         * @param key  The key to search for.
         * @param hash The hash value of the key.
         * @return std::optional<ValueT> if found, or std::nullopt.
         */
        std::optional<ValueT> lookup(KeyT key, uint32_t hash) const {
            size_t index = hash % TABLE_SIZE;

            for (size_t i = 0; i < TABLE_SIZE; ++i) {
                const auto& entry = table[(index + i) % TABLE_SIZE];
                if (is_empty(entry.key))
                    return std::nullopt;
                if (entry.key == key)
                    return entry.value;
            }

            return std::nullopt;
        }

        void print() {
            std::cout << "Hashed lookup: "
                << TABLE_SIZE << " slots "
                << total_collisions << " collisions in "
                << total_inserts << " inserts).\n";
        }

    private:
        static constexpr KeyT EMPTY_KEY = std::numeric_limits<KeyT>::max();

        struct Entry {
            KeyT key = EMPTY_KEY;
            ValueT value;
        };

        std::array<Entry, TABLE_SIZE> table;

        static constexpr bool is_empty(KeyT key) {
            return key == EMPTY_KEY;
        }

        size_t total_inserts = 0;
        size_t total_collisions = 0;

        void check_warning() const {
            if (total_inserts > 0 && (total_collisions * 100) / total_inserts > 10) {
                std::cerr << "WARNING: HashedLookup has high collision rate ("
                    << total_collisions << " collisions in "
                    << total_inserts << " inserts).\n";
            }
        }
    };


    /**
     * @brief Specialized lookup for piece signatures using internal hashing.
     *
     * @tparam ValueT            Value type to store.
     * @tparam TABLE_SIZE        Number of table entries.
     * @tparam SIG_BITS_PER_SIDE Bit-width of one side's signature (e.g. 12).
     */
    template <typename ValueT, size_t TABLE_SIZE, uint32_t SIG_BITS_PER_SIDE>
    class PieceSignatureHashedLookup {
    public:
        using KeyT = uint32_t;

        /**
         * @brief Inserts a value by full 24-bit signature.
         *
         * @param signature Combined signature (white << bits | black)
         * @param value     Value to store
         * @return True if inserted successfully.
         */
        bool insert(KeyT signature, ValueT value) {
            uint32_t black = signature >> SIG_BITS_PER_SIDE;
            uint32_t white = signature & ((static_cast<uint32_t>(1) << SIG_BITS_PER_SIDE) - 1);
            uint32_t hash = SignatureHasher<(static_cast<uint32_t>(1) << SIG_BITS_PER_SIDE)>::hash(white, black);
            return getTable().insert(signature, hash, value);
        }

        /**
         * @brief Looks up a value by full 24-bit signature.
         *
         * @param signature Combined signature key.
         * @return std::optional<ValueT> if found.
         */
        std::optional<ValueT> lookup(KeyT signature) const {
            
            uint32_t black = signature >> SIG_BITS_PER_SIDE;
            uint32_t white = signature & ((static_cast<uint32_t>(1) << SIG_BITS_PER_SIDE) - 1);
            uint32_t hash = SignatureHasher<(static_cast<uint32_t>(1) << SIG_BITS_PER_SIDE)>::hash(white, black);
            return getTable().lookup(signature, hash);
        }

        void print() { getTable().print(); }

    private:
        using HashedTable = HashedLookup<KeyT, ValueT, TABLE_SIZE>;

        static HashedTable& getTable() {
            static HashedTable table;
            return table;
        }
    };

}


