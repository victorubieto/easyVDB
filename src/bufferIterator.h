#pragma once

#include <vector>
#include <string>

#include <glm/vec3.hpp>

#include "utils.h"

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

class BufferIterator {
public:
	// buffer data management
	std::vector<uint8_t> rawBuffer;
	unsigned int offset;

	BufferIterator(std::vector<uint8_t> source, unsigned int offset = 0u);

	// reads the N bytes sended and erases the N first values of the buffer
	int readBytes(unsigned int count);
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