/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include <fmt/core.h>

class BinaryReader
{
	public:
		BinaryReader() = default;

		explicit BinaryReader(const std::vector<uint8_t> &data)
		: m_data(data.data())
		, m_size(data.size())
		, m_pos(0)
		{
		}

		BinaryReader(const uint8_t *data, size_t size)
		: m_data(data)
		, m_size(size)
		, m_pos(0)
		{
		}

		void reset(const uint8_t *data, size_t size)
		{
			m_data = data;
			m_size = size;
			m_pos = 0;
		}

		void reset(const std::vector<uint8_t> &data)
		{
			reset(data.data(), data.size());
		}

		uint8_t getU8()
		{
			ensureCanRead(1);
			uint8_t v = m_data[m_pos];
			m_pos += 1;
			return v;
		}

		int8_t get8()
		{
			return static_cast<int8_t>(getU8());
		}

		uint16_t getU16()
		{
			ensureCanRead(2);
			uint16_t v = 0;
			std::memcpy(&v, m_data + m_pos, 2);
			m_pos += 2;
			return v;
		}

		int16_t get16()
		{
			return static_cast<int16_t>(getU16());
		}

		uint32_t getU32()
		{
			ensureCanRead(4);
			uint32_t v = 0;
			std::memcpy(&v, m_data + m_pos, 4);
			m_pos += 4;
			return v;
		}

		int32_t get32()
		{
			return static_cast<int32_t>(getU32());
		}

		uint64_t getU64()
		{
			ensureCanRead(8);
			uint64_t v = 0;
			std::memcpy(&v, m_data + m_pos, 8);
			m_pos += 8;
			return v;
		}

		int64_t get64()
		{
			return static_cast<int64_t>(getU64());
		}

		std::string getString()
		{
			uint16_t len = getU16();
			return getString(len);
		}

		std::string getString(uint16_t len)
		{
			if (len == 0) {
				return {};
			}
			ensureCanRead(len);
			std::string result(reinterpret_cast<const char *>(m_data + m_pos), len);
			m_pos += len;
			return result;
		}

		std::vector<uint8_t> getBytes(size_t count)
		{
			ensureCanRead(count);
			std::vector<uint8_t> result(m_data + m_pos, m_data + m_pos + count);
			m_pos += count;
			return result;
		}

		void readBytes(uint8_t *dest, size_t count)
		{
			ensureCanRead(count);
			std::memcpy(dest, m_data + m_pos, count);
			m_pos += count;
		}

		template <typename T>
		bool read(T &out)
		{
			if (m_pos + sizeof(T) > m_size) {
				return false;
			}
			std::memcpy(&out, m_data + m_pos, sizeof(T));
			m_pos += sizeof(T);
			return true;
		}

		void skip(size_t count)
		{
			ensureCanRead(count);
			m_pos += count;
		}

		bool trySkip(size_t count)
		{
			if (m_pos + count > m_size) {
				return false;
			}
			m_pos += count;
			return true;
		}

		void seek(size_t pos)
		{
			if (pos > m_size) {
				throw std::runtime_error(
				    fmt::format("BinaryReader: seek to {} exceeds data size {}", pos, m_size)
				);
			}
			m_pos = pos;
		}

		size_t tell() const { return m_pos; }
		size_t size() const { return m_size; }
		size_t remaining() const { return m_size - m_pos; }
		bool eof() const { return m_pos >= m_size; }
		bool canRead(size_t count = 1) const { return m_pos + count <= m_size; }

		const uint8_t *data() const { return m_data; }
		const uint8_t *current() const { return m_data + m_pos; }

	private:
		void ensureCanRead(size_t count) const
		{
			if (m_pos + count > m_size) {
				throw std::runtime_error(
				    fmt::format("BinaryReader: read of {} bytes at position {} exceeds data size {}", count, m_pos, m_size)
				);
			}
		}

		const uint8_t *m_data = nullptr;
		size_t m_size = 0;
		size_t m_pos = 0;
};
