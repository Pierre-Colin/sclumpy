#ifndef BYTE_H
#define BYTE_H

#include <climits>
#include <cstddef>

template<class It, class N>
It put_little_endian(It it, N n)
{
	for (unsigned b = 0; b < sizeof (N); ++b) {
		const auto bth_byte = (n >> (CHAR_BIT * b)) & N{0xff};
		*it++ = std::byte{static_cast<unsigned char>(bth_byte)};
	}
	return it;
}

template<class It, class N>
It put_big_endian(It it, N n)
{
	static constexpr unsigned W = sizeof (N);
	for (unsigned b = 0; b < W; ++b) {
		const auto bth_byte = (n >> (CHAR_BIT * (W - b))) & N{0xff};
		*it++ = std::byte{static_cast<unsigned char>(bth_byte)};
	}
	return it;
}

#endif
