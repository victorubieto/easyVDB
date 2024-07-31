#pragma once

#include <vector>
#include <string>

#include <glm/vec3.hpp>

#include "utils.h"

// in bytes
const unsigned int charSize = 1u;
const unsigned int boolSize = 1u;
const unsigned int uint32Size = 4u;
const unsigned int uint64Size = 8u;
const unsigned int uint128Size = 8u;
const unsigned int floatSize = uint32Size;
const unsigned int doubleSize = uint64Size;

enum Precision : unsigned int { stringType, int32Type, int64Type, boolType, halfFloatType, floatType, doubleType, vec3iType, vec3sType, vec3dType, pointDataIndex32, pointDataIndex64, undefined_precision };
struct PrecisionLUT { unsigned int exp; unsigned int bias; unsigned int size; };

inline std::string getPrecisionString(unsigned int idx)
{
	const char* PrecisionString[12] = { "string", "int32", "int64", "bool", "half", "float", "double", "vec3i", "vec3s", "vec3d", "ptdataidx32", "ptdataidx64" };
	return PrecisionString[idx];
}

inline Precision getPrecisionIdx(std::string precision)
{
	const char* PrecisionString[12] = { "string", "int32", "int64", "bool", "half", "float", "double", "vec3i", "vec3s", "vec3d", "ptdataidx32", "ptdataidx64" };
	for (unsigned int i = 0; i < 12; i++) {
		const char* p = PrecisionString[i];
		if (p == precision) {
			return Precision(i);
		}
	}
	return undefined_precision;
}

// -- special stuff

inline unsigned int as_uint(const float x) {
	return *(unsigned int*)&x;
}

inline float as_float(const unsigned int x) {
	return *(float*)&x;
}

inline float half_to_float(const unsigned short x) { // IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15, +-131008.0, +-6.1035156E-5, +-5.9604645E-8, 3.311 digits
	const unsigned int e = (x & 0x7C00) >> 10; // exponent
	const unsigned int m = (x & 0x03FF) << 13; // mantissa
	const unsigned int v = as_uint((float)m) >> 23; // evil log2 bit hack to count leading zeros in denormalized format
	return as_float((x & 0x8000) << 16 | (e != 0) * ((e + 112) << 23 | m) | ((e == 0) & (m != 0)) * ((v - 37) << 23 | ((m << (150 - v)) & 0x007FE000))); // sign : normalized : denormalized
}

inline unsigned short float_to_half(const float x) { // IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15, +-131008.0, +-6.1035156E-5, +-5.9604645E-8, 3.311 digits
	const unsigned int b = as_uint(x) + 0x00001000; // round-to-nearest-even: add last bit after truncated mantissa
	const unsigned int e = (b & 0x7F800000) >> 23; // exponent
	const unsigned int m = b & 0x007FFFFF; // mantissa; in line below: 0x007FF000 = 0x00800000-0x00001000 = decimal indicator flag - initial rounding
	return (b & 0x80000000) >> 16 | (e > 112) * ((((e - 112) << 10) & 0x7C00) | m >> 13) | ((e < 113) & (e > 101)) * ((((0x007FF000 + m) >> (125 - e)) + 1) >> 1) | (e > 143) * 0x7FFF; // sign : normalized : denormalized : saturate
}

// special stuff --

class BufferIterator {
public:
	// buffer data management
	std::vector<uint8_t> rawBuffer;
	unsigned int offset;

	BufferIterator(std::vector<uint8_t> source, unsigned int offset = 0u);

	// reads the N bytes sended and erases the N first values of the buffer
	long long readBytes(unsigned int count);
	uint8_t* readRawBytes(unsigned int count);
	bool readBool();
	float readFloat(unsigned int precision = doubleType);
	std::string readString(unsigned int castTo = stringType);
	std::string readString(std::string); // copy used in certain situations
	glm::vec3 readVector3(unsigned int precision = doubleType);

	PrecisionLUT floatingPointPrecisionLUT(unsigned int precision);
};

struct Compression
{
	bool none;
	bool zlib;
	bool activeMask;
	bool blosc;
};

struct SharedContext {
	Compression compression;
	Precision valueType;
	bool useHalf;
	bool useDelayedLoadMeta;
	std::string delayedLoadMeta;
	BufferIterator* bufferIterator;
	unsigned int* version;
};

struct Metadata
{
	std::string name;
	std::string type;
	std::string value;
	Metadata(std::string _name, std::string _type, std::string _value) : name(_name), type(_type), value(_value) {
	}
};