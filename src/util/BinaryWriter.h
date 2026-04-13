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

class BinaryWriter
{
	public:
		BinaryWriter() = default;

		explicit BinaryWriter(size_t reserveSize)
		{
			m_buffer.reserve(reserveSize);
		}

		void addU8(uint8_t v)
		{
			m_buffer.push_back(v);
		}

		void add8(int8_t v)
		{
			addU8(static_cast<uint8_t>(v));
		}

		void addU16(uint16_t v)
		{
			uint8_t bytes[2];
			std::memcpy(bytes, &v, 2);
			m_buffer.insert(m_buffer.end(), bytes, bytes + 2);
		}

		void add16(int16_t v)
		{
			addU16(static_cast<uint16_t>(v));
		}

		void addU32(uint32_t v)
		{
			uint8_t bytes[4];
			std::memcpy(bytes, &v, 4);
			m_buffer.insert(m_buffer.end(), bytes, bytes + 4);
		}

		void add32(int32_t v)
		{
			addU32(static_cast<uint32_t>(v));
		}

		void addU64(uint64_t v)
		{
			uint8_t bytes[8];
			std::memcpy(bytes, &v, 8);
			m_buffer.insert(m_buffer.end(), bytes, bytes + 8);
		}

		void add64(int64_t v)
		{
			addU64(static_cast<uint64_t>(v));
		}

		void addString(const std::string &str)
		{
			if (str.size() > 0xFFFF) {
				throw std::runtime_error(
				    fmt::format("BinaryWriter: string length {} exceeds uint16 max", str.size())
				);
			}
			addU16(static_cast<uint16_t>(str.size()));
			addBytes(reinterpret_cast<const uint8_t *>(str.data()), str.size());
		}

		void addBytes(const uint8_t *data, size_t count)
		{
			m_buffer.insert(m_buffer.end(), data, data + count);
		}

		void addBytes(const std::vector<uint8_t> &data)
		{
			m_buffer.insert(m_buffer.end(), data.begin(), data.end());
		}

		void writeU8At(size_t pos, uint8_t v)
		{
			ensurePosition(pos, 1);
			m_buffer[pos] = v;
		}

		void writeU16At(size_t pos, uint16_t v)
		{
			ensurePosition(pos, 2);
			std::memcpy(&m_buffer[pos], &v, 2);
		}

		void writeU32At(size_t pos, uint32_t v)
		{
			ensurePosition(pos, 4);
			std::memcpy(&m_buffer[pos], &v, 4);
		}

		void writeU64At(size_t pos, uint64_t v)
		{
			ensurePosition(pos, 8);
			std::memcpy(&m_buffer[pos], &v, 8);
		}

		void padTo(size_t targetSize, uint8_t fillByte = 0)
		{
			if (m_buffer.size() < targetSize) {
				m_buffer.resize(targetSize, fillByte);
			}
		}

		void clear()
		{
			m_buffer.clear();
		}

		void reserve(size_t capacity)
		{
			m_buffer.reserve(capacity);
		}

		size_t size() const { return m_buffer.size(); }
		bool empty() const { return m_buffer.empty(); }

		const uint8_t *data() const { return m_buffer.data(); }
		uint8_t *data() { return m_buffer.data(); }

		const std::vector<uint8_t> &buffer() const { return m_buffer; }
		std::vector<uint8_t> &buffer() { return m_buffer; }

		std::vector<uint8_t> takeBuffer()
		{
			return std::move(m_buffer);
		}

	private:
		void ensurePosition(size_t pos, size_t count) const
		{
			if (pos + count > m_buffer.size()) {
				throw std::runtime_error(
				    fmt::format("BinaryWriter: write at position {} with size {} exceeds buffer size {}", pos, count, m_buffer.size())
				);
			}
		}

		std::vector<uint8_t> m_buffer;
};
