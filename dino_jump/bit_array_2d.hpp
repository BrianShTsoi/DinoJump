#ifndef BIT_ARRAY_2D_HPP
#define BIT_ARRAY_2D_HPP

#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>

template <std::size_t N> class BitArray2D {
  public:
	BitArray2D() : array{} {};

	template <std::size_t R>
	BitArray2D(const std::array<std::array<uint8_t, N>, R> &orig_array) : array{} {
		static_assert(R <= MAX_ROW_SIZE, "Exceed maximum row size supported");

		// Setting column by column is more cache-friendly
		for (int c = 0; c < N; c++) {
			for (int r = 0; r < R; r++) {
				uint8_t b = orig_array[r][c];
				assert(b == 0 || b == 1);
				array[c] |= static_cast<uint64_t>(b) << r;
			}
		}
	}

	template <std::size_t R> std::array<std::array<uint8_t, N>, R> to_uint8_array() const {
		static_assert(R <= MAX_ROW_SIZE, "Exceed maximum row size supported");
		std::array<std::array<uint8_t, N>, R> frame = {0};

		for (int r = 0; r < R; r++) {
			for (int c = 0; c < N; c++) {
				frame[r][c] = get(r, c);
			}
		}

		return frame;
	}

	uint8_t get(const int r, const int c) const {
		assert(r < MAX_ROW_SIZE);
		assert(c < N);

		return (array[c] >> r) & 1;
	}

	void set(const int r, const int c, const uint8_t b) {
		assert(r < MAX_ROW_SIZE);
		assert(c < N);
		assert(b == 0 || b == 1);

		// Clear the bit, then set the bit
		array[c] &= ~(static_cast<uint64_t>(1) << r);
		array[c] |= static_cast<uint64_t>(b) << r;
	}

	// Note: row index 0 corresponds to the least significant bit
	void set_col(int c, uint64_t val) { array[c] = val; }

	uint64_t get_col(int c) const { return array[c]; }

	void clear() { array = {0}; }

	// Each non-zero column is represented by its hex form (maximum 12 chars) +
	// comma. Columns which are all zeros are grouped together, represented by the
	// format "#n", where n is the number of zero columns, in hex form. `buf` must
	// have a size that can store the maximum length of the compressed form
	// (including '\0') .
	// For example, the Frame class has MAX_X columns, each with 48 bits (which
	// can be represented by 12 hex characters). The given buf must have a size at
	// least (12 + 1) * MAX_X + 1.
	void compressed_form(char *buf) const {
		// Start with '\0' so that strlen(buf) is 0
		buf[0] = '\0';
		for (int x = 0; x < N; x++) {
			uint64_t val = get_col(x);

			if (val != 0) {
				// sprintf(buf + strlen(buf), "%lX,", val);
				sprintf(buf + strlen(buf), "%llX,", (unsigned long long)val);
			} else {
				int zeros = 1;
				while (x + zeros < N && get_col(x + zeros) == 0) {
					zeros++;
				}
				sprintf(buf + strlen(buf), "#%X,", zeros);
				x += zeros - 1;
			}
		}
	}

  private:
	static constexpr int MAX_ROW_SIZE = 64;
	std::array<uint64_t, N> array;
};

#endif // BIT_ARRAY_2D_HPP

// #ifndef BIT_ARRAY_2D_HPP
// #define BIT_ARRAY_2D_HPP

// #include <array>
// #include <cassert>
// #include <string.h>

// template <std::size_t N> class BitArray2D {
//   public:
// 	BitArray2D() : array{} {};

// 	template <std::size_t R>
// 	BitArray2D(const std::array<std::array<uint8_t, N>, R> &orig_array) :
// array{} { 		static_assert(R <= MAX_ROW_SIZE, "Exceed maximum row
// size supported");

// 		// Setting column by column is more cache-friendly
// 		for (int c = 0; c < N; c++) {
// 			for (int r = 0; r < R; r++) {
// 				uint8_t b = orig_array[r][c];
// 				assert(b == 0 || b == 1);
// 				array[c] |= static_cast<uint64_t>(b) << r;
// 			}
// 		}
// 	}

// 	template <std::size_t R> std::array<std::array<uint8_t, N>, R>
// to_uint8_array() const { 		static_assert(R <= MAX_ROW_SIZE, "Exceed
// maximum row size supported"); 		std::array<std::array<uint8_t,
// N>, R> frame = {0};

// 		for (int r = 0; r < R; r++) {
// 			for (int c = 0; c < N; c++) {
// 				frame[r][c] = get(r, c);
// 			}
// 		}

// 		return frame;
// 	}

// 	uint8_t get(const int r, const int c) const {
// 		assert(r < MAX_ROW_SIZE);
// 		assert(c < N);

// 		return (array[c] >> r) & 1;
// 	}

// 	void set(const int r, const int c, const uint8_t b) {
// 		assert(r < MAX_ROW_SIZE);
// 		assert(c < N);
// 		assert(b == 0 || b == 1);

// 		// Clear the bit, then set the bit
// 		array[c] &= ~(static_cast<uint64_t>(1) << r);
// 		array[c] |= static_cast<uint64_t>(b) << r;
// 	}

// 	// Note: row index 0 correspond to the least significant bit
// 	void set_col(int c, uint64_t val) { array[c] = val; }

// 	uint64_t get_col(int c) const { return array[c]; }

// 	void clear() { array = {0}; }

// 	// Each non-zero column is represented by its hex form (maximum 16
// chars) + comma. Columns
// 	// which are all zeros are grouped together, represented by the format
// "#n", where n is the
// 	// number of zero columns, in hex form.
// 	// `buf` must have a size that can store the maximum length of the
// compressed form
// 	// (including '\0') .
// 	// For example, the Frame class has MAX_X columns, each with 48 bits
// (which can be
// 	// represented by 12 hex characters). The given buf must have a size at
// least (12 + 1) *
// 	// MAX_X + 1.
// 	void compressed_form(char *buf) const {
// 		// Start with '\0' so that strlen(buf) is 0
// 		buf[0] = '\0';
// 		for (int x = 0; x < N; x++) {
// 			uint64_t val = get_col(x);

// 			if (val != 0) {
// 				sprintf(buf + strlen(buf), "%lX,", val);
// 			} else {
// 				int zeros = 1;
// 				while (x + zeros < N && get_col(x + zeros) == 0)
// { 					zeros++;
// 				}
// 				sprintf(buf + strlen(buf), "#%X,", zeros);
// 				x += zeros - 1;
// 			}
// 		}
// 	}

//   private:
// 	static constexpr int MAX_ROW_SIZE = 64;
// 	std::array<uint64_t, N> array;
// };

// #endif // BIT_ARRAY_2D_HPP