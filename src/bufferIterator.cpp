#include "bufferIterator.h"

#include <iostream>
#include <bitset>
#include <sstream>
#include <cmath>

using namespace easyVDB;

BufferIterator::BufferIterator(std::vector<uint8_t> source, unsigned int offset)
{
	rawBuffer.resize(source.size());
	rawBuffer = source;

	this->offset = offset;
}

// reads the N bytes sended and erases the N first values of the buffer
long long BufferIterator::readBytes(unsigned int count) // right now, max 8 bytes
{
	long long buffer = 0;

	for (long long i = 0; i < count; i++) {
		buffer = buffer | (static_cast<long long>(rawBuffer[offset + i]) << (8 * i));
	}

	offset += count;
	return buffer;
}

uint8_t* BufferIterator::readRawBytes(unsigned int count)
{
	uint8_t* raw = new uint8_t[count];

	for (unsigned int i = 0u; i < count; i++) {
		raw[i] = rawBuffer[offset + i];
	}

	offset += count;
	return raw;
}

bool BufferIterator::readBool()
{
	return readBytes(boolSize) ? "true" : "false";
}

long long BufferIterator::readInt(unsigned int precision)
{
	PrecisionLUT precisionLUT = floatingPointPrecisionLUT(precision);

	if (precision == vec3sType || precision == vec3iType || precision == vec3dType) {
		// TODO
		// ..
		printf("Precision vec3 still not supported");
		assert(false);
	}

	if (precisionLUT.size == NULL) {
		std::cout << "[WARN] Unknown value type " << getPrecisionString(precision) << std::endl;
	}

	if (precision == int32Type) {
		uint32_t temp;

		temp = (uint32_t)rawBuffer[offset];
		temp |= (uint32_t)rawBuffer[offset + 1] << 8;
		temp |= (uint32_t)rawBuffer[offset + 2] << 16;
		temp |= (uint32_t)rawBuffer[offset + 3] << 24;

		offset += precisionLUT.size;
		long long val = hexToDec(std::to_string(temp));

		return val;
	}
	else if (precision == int64Type) {
		uint64_t temp;

		temp = (uint64_t)rawBuffer[offset];
		temp |= (uint64_t)rawBuffer[offset + 1] << 8;
		temp |= (uint64_t)rawBuffer[offset + 2] << 16;
		temp |= (uint64_t)rawBuffer[offset + 3] << 24;
		temp |= (uint64_t)rawBuffer[offset + 4] << 32;
		temp |= (uint64_t)rawBuffer[offset + 5] << 40;
		temp |= (uint64_t)rawBuffer[offset + 6] << 48;
		temp |= (uint64_t)rawBuffer[offset + 7] << 56;

		offset += precisionLUT.size;
		long long val = stoll(std::to_string(temp), nullptr);

		return val;
	}
}

float BufferIterator::readFloat(unsigned int precision)
{
	PrecisionLUT precisionLUT = floatingPointPrecisionLUT(precision);

	if (precision == vec3sType || precision == vec3iType || precision == vec3dType) {
		// TODO
		// ..
		printf("Precision vec3 still not supported");
		assert(false);
	}

	if (precisionLUT.size == NULL) {
		std::cout << "[WARN] Unknown value type " << getPrecisionString(precision) << std::endl;
	}

	if (precision == int32Type || precision == int64Type) {

		std::string binary = "";
		for (unsigned int i = 0u; i < precisionLUT.size; i++) {
			unsigned int bin = readBytes(charSize);
			std::string s = std::bitset< 8 >(bin).to_string();
			binary = s + binary;
		}

		// NOTE https://stackoverflow.com/questions/37022434/how-do-i-parse-a-twos-complement-string-to-a-number
		// also this https://stackoverflow.com/questions/29931827/stoi-causes-out-of-range-error
		return (int)~~std::stoll(binary, nullptr, 2);
	}
	else if (precision == halfFloatType)
	{
		unsigned short raw = readBytes(charSize);
		raw |= static_cast<unsigned short>(readBytes(charSize)) << 8;
		return half_to_float(raw);
	}
	else if (precision == floatType)
	{
		unsigned int raw = readBytes(charSize);
		raw |= static_cast<unsigned short>(readBytes(charSize)) << 8;
		raw |= static_cast<unsigned short>(readBytes(charSize)) << 16;
		raw |= static_cast<unsigned short>(readBytes(charSize)) << 24;
		return as_float(raw);
	} 
	else if (precision == doubleType)
	{
		// TO DO
		// .. we need to return a double somehow

		/*printf("Precision double still not supported");
		assert(false);*/

		/*union {
			unsigned char uint8;
			unsigned short uint16;
			unsigned int uint32;
			unsigned long long uint64;
			float float32;
			double float64;
		};*/

		std::string binary = "";
		for (unsigned int i = 0u; i < precisionLUT.size; i++) {
			unsigned int bin = readBytes(charSize);
			std::string s = std::bitset< 8 >(bin).to_string();
			binary = s + binary;
		}

		int sign = binary[0] == (char)'1' ? -1 : 1;

		int exponent = std::stoi(std::string(&binary[1], &binary[precisionLUT.exp + 1]), nullptr, 2) - precisionLUT.bias;
		std::string mantissa = "1" + std::string(&binary[precisionLUT.exp + 1], &binary[binary.size()]);

		// NOTE https://stackoverflow.com/questions/37109968/how-to-convert-binary-fraction-to-decimal
		std::string v1_string = exponent < 0 ? "0.0" : std::string(&mantissa[0], &mantissa[exponent + 1]);
		std::string aux_v2_string = "";
		for (int i = 0; i < (-exponent - 1); i++) {
			aux_v2_string += "0";
		}
		std::string v2_string = 
			"0." + 
			aux_v2_string + 
			std::string(exponent < 0 ? &mantissa[0] : &mantissa[exponent + 1], &mantissa[mantissa.size()]);

		int v1 = std::stoi(v1_string, nullptr, 2);
		// TODO: Clean :) optimize
		std::string aux2_v2_string = v2_string;
		aux2_v2_string.erase(remove(aux2_v2_string.begin(), aux2_v2_string.end(), '.'), aux2_v2_string.end());
		std::stringstream test(v2_string);
		std::string aux3_v2_string;
		std::vector<std::string> v2_seglist;
		while (std::getline(test, aux3_v2_string, '.'))
		{
			v2_seglist.push_back(aux3_v2_string);
		}
		int v2_len = (v2_seglist.size() > 1 ? v2_seglist[1] : std::string("")).size();
		double v2_len_p2 = pow(2, v2_len);
		double v2_toNumber = std::stoll(aux2_v2_string, nullptr, 2);
		double v2 = v2_toNumber / v2_len_p2;
		//

		if (v1 == 0 && v2 == 0) {
			return 0;
		}

		return sign * (v1 + v2);
	}
}

std::string BufferIterator::readString(unsigned int castTo)
{
	unsigned int nameSize = readBytes(uint32Size);
	std::string name = "";

	if (castTo == int64Type) {
		name = std::to_string(readFloat(int64Type));
	}
	else if (castTo == boolType) {
		name = readBytes(nameSize) ? "true" : "false";
	}
	else if (castTo == vec3iType) {
		std::string v1 = std::to_string(readFloat(int32Type));
		std::string v2 = std::to_string(readFloat(int32Type));
		std::string v3 = std::to_string(readFloat(int32Type));
		name = v1 + ", " + v2 + ", " + v3;
	}
	else if (castTo == vec3sType) {
		std::string v1 = std::to_string(readFloat(floatType));
		std::string v2 = std::to_string(readFloat(floatType));
		std::string v3 = std::to_string(readFloat(floatType));
		name = v1 + ", " + v2 + ", " + v3;
	}
	else if (castTo == vec3dType) {
		std::string v1 = std::to_string(readFloat(floatType));
		std::string v2 = std::to_string(readFloat(floatType));
		std::string v3 = std::to_string(readFloat(floatType));
		name = v1 + ", " + v2 + ", " + v3;
	}
	else {
		for (unsigned int i = 0; i < nameSize; i++) {
			name += readBytes(charSize);
		}
	}

	return name;
}

std::string BufferIterator::readString(std::string castTo)
{
	unsigned int nameSize = readBytes(uint32Size);
	std::string name = "";

	if (castTo == getPrecisionString(int32Type)) {
		name = std::to_string(readFloat(int32Type));
	}
	else if (castTo == getPrecisionString(int64Type)) {
		name = std::to_string(readFloat(int64Type));
	}
	else if (castTo == getPrecisionString(boolType)) {
		name = readBytes(nameSize) ? "true" : "false";
	}
	else if (castTo == getPrecisionString(vec3iType)) {
		std::string v1 = std::to_string(readFloat(int32Type));
		std::string v2 = std::to_string(readFloat(int32Type));
		std::string v3 = std::to_string(readFloat(int32Type));
		name = v1 + ", " + v2 + ", " + v3;
	}
	else if (castTo == getPrecisionString(vec3sType)) {
		std::string v1 = std::to_string(readFloat(floatType));
		std::string v2 = std::to_string(readFloat(floatType));
		std::string v3 = std::to_string(readFloat(floatType));
		name = v1 + ", " + v2 + ", " + v3;
	}
	else if (castTo == getPrecisionString(vec3dType)) {
		std::string v1 = std::to_string(readFloat(floatType));
		std::string v2 = std::to_string(readFloat(floatType));
		std::string v3 = std::to_string(readFloat(floatType));
		name = v1 + ", " + v2 + ", " + v3;
	}
	else {
		for (unsigned int i = 0; i < nameSize; i++) {
			name += readBytes(charSize);
		}
	}

	return name;
}

glm::vec3 BufferIterator::readVector3(unsigned int precision)
{
	glm::vec3 vec(0.f);

	vec.x = readFloat(precision);
	vec.y = readFloat(precision);
	vec.z = readFloat(precision);

	return vec;
}

PrecisionLUT BufferIterator::floatingPointPrecisionLUT(unsigned int precision)
{
	unsigned int exp = NULL;
	unsigned int bias = NULL;
	unsigned int size = NULL;

	switch (precision)
	{
	case doubleType:
		exp = 11u;
		bias = (1u << (11u - 1u)) - 1u;
		size = doubleSize;
		break;
	case floatType:
		exp = 8u;
		bias = (1u << (8u - 1u)) - 1u;
		size = uint32Size;
		break;
	case int32Type:
		size = uint32Size;
		break;
	case int64Type:
		size = uint64Size;
		break;
	case pointDataIndex32:
		size = uint32Size;
		break;
	case pointDataIndex64:
		size = uint64Size;
		break;
	case halfFloatType:
		size = uint32Size / 2u;
		exp = 5u;
		bias = (1u << (5u - 1u)) - 1u;
		break;
	default:
		break;
	}

	return PrecisionLUT({ exp , bias, size });
}