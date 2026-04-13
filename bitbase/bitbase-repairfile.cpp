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
 * @copyright Copyright (c) 2025 Volker Böhm
 */

#include "bitbase-repairfile.h"

#include <cstring>
#include <fstream>
#include <stdexcept>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace QaplaBitbase {

// =============================================================================
// write
// =============================================================================

void BitbaseRePairFile::write(const std::string& fileName,
                               const std::vector<BitbaseResult>& results)
{
    // Compress the WDL sequence.
    QaplaRePair::RePairData data = QaplaRePair::compress(results);
    std::vector<uint8_t> payload = QaplaRePair::serialize(data);

    // Build the 16-byte file header.
    uint8_t header[HEADER_BYTES]{};
    uint32_t m1 = MAGIC1;
    uint32_t m2 = MAGIC2;
    uint64_t entryCount = static_cast<uint64_t>(results.size());
    std::memcpy(header + 0, &m1,         4);
    std::memcpy(header + 4, &m2,         4);
    std::memcpy(header + 8, &entryCount, 8);

    // Write header + payload to file.
    std::ofstream ofs(fileName, std::ios::binary | std::ios::trunc);
    if (!ofs) {
        throw std::runtime_error(
            "BitbaseRePairFile::write: cannot open file for writing: " + fileName);
    }
    ofs.write(reinterpret_cast<const char*>(header),        HEADER_BYTES);
    ofs.write(reinterpret_cast<const char*>(payload.data()), payload.size());
    if (!ofs) {
        throw std::runtime_error(
            "BitbaseRePairFile::write: I/O error writing file: " + fileName);
    }
}

// =============================================================================
// open
// =============================================================================

bool BitbaseRePairFile::open(const std::string& fileName)
{
    close(); // release any previous mapping

#ifdef _WIN32
    // Convert UTF-8 path to wide string for the Windows API.
    int wlen = MultiByteToWideChar(CP_UTF8, 0, fileName.c_str(), -1, nullptr, 0);
    std::wstring wname(static_cast<size_t>(wlen), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, fileName.c_str(), -1, wname.data(), wlen);

    _fileHandle = CreateFileW(
        wname.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (_fileHandle == INVALID_HANDLE_VALUE) {
        return false;
    }

    LARGE_INTEGER fileSize{};
    if (!GetFileSizeEx(_fileHandle, &fileSize)) {
        CloseHandle(_fileHandle);
        _fileHandle = INVALID_HANDLE_VALUE;
        return false;
    }
    _mappedSize = static_cast<size_t>(fileSize.QuadPart);

    _mappingHandle = CreateFileMappingW(
        _fileHandle, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!_mappingHandle) {
        CloseHandle(_fileHandle);
        _fileHandle = INVALID_HANDLE_VALUE;
        return false;
    }

    _mappedData = static_cast<const uint8_t*>(
        MapViewOfFile(_mappingHandle, FILE_MAP_READ, 0, 0, 0));
    if (!_mappedData) {
        CloseHandle(_mappingHandle);
        CloseHandle(_fileHandle);
        _mappingHandle = nullptr;
        _fileHandle    = INVALID_HANDLE_VALUE;
        return false;
    }

#else
    _fd = ::open(fileName.c_str(), O_RDONLY);
    if (_fd < 0) {
        return false;
    }
    struct stat st{};
    if (::fstat(_fd, &st) < 0) {
        ::close(_fd);
        _fd = -1;
        return false;
    }
    _mappedSize = static_cast<size_t>(st.st_size);

    void* ptr = ::mmap(nullptr, _mappedSize, PROT_READ, MAP_SHARED, _fd, 0);
    if (ptr == MAP_FAILED) {
        ::close(_fd);
        _fd = -1;
        return false;
    }
    _mappedData = static_cast<const uint8_t*>(ptr);
#endif

    // Validate the file header.
    if (_mappedSize < HEADER_BYTES) {
        close();
        return false;
    }
    uint32_t m1{}, m2{};
    std::memcpy(&m1, _mappedData + 0, 4);
    std::memcpy(&m2, _mappedData + 4, 4);
    if (m1 != MAGIC1 || m2 != MAGIC2) {
        close();
        return false;
    }
    std::memcpy(&_entryCount, _mappedData + 8, 8);

    // Deserialize the Re-Pair payload from the mapped memory.
    // deserialize() reads sequentially through the mapping; the OS will
    // page-fault data in on demand and cache pages for subsequent probes.
    const uint8_t* payload     = _mappedData + HEADER_BYTES;
    size_t         payloadSize = _mappedSize  - HEADER_BYTES;
    try {
        _repairData = QaplaRePair::deserialize(payload, payloadSize);
    } catch (...) {
        close();
        return false;
    }

    _isOpen = true;
    return true;
}

// =============================================================================
// close
// =============================================================================

void BitbaseRePairFile::close()
{
#ifdef _WIN32
    if (_mappedData) {
        UnmapViewOfFile(_mappedData);
        _mappedData = nullptr;
    }
    if (_mappingHandle) {
        CloseHandle(_mappingHandle);
        _mappingHandle = nullptr;
    }
    if (_fileHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(_fileHandle);
        _fileHandle = INVALID_HANDLE_VALUE;
    }
#else
    if (_mappedData) {
        ::munmap(const_cast<uint8_t*>(_mappedData), _mappedSize);
        _mappedData = nullptr;
    }
    if (_fd >= 0) {
        ::close(_fd);
        _fd = -1;
    }
#endif

    _mappedSize = 0;
    _entryCount = 0;
    _isOpen     = false;
    _repairData = {};
}

// =============================================================================
// probe
// =============================================================================

BitbaseResult BitbaseRePairFile::probe(uint64_t index) const
{
    // WDLValue 0/1/2 maps directly to BitbaseResult 0/1/2 (see recursive-pairing.h).
    QaplaRePair::WDLValue wdl = QaplaRePair::probe(_repairData, index);
    return static_cast<BitbaseResult>(static_cast<uint8_t>(wdl));
}

} // namespace QaplaBitbase
