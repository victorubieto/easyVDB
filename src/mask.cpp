#include "mask.h"

#include <string>
#include <vector>
#include <bitset>

#include "bufferIterator.h"

#include "node.h"

using namespace easyVDB;

Mask::Mask()
{
	targetNode = nullptr; 
	onCache = -1;
	offCache = -1;
	size = 0;
}

Mask::Mask(const int _size)
{
	targetNode = nullptr;
	onCache = -1;
	offCache = -1;
	size = 0;
	
	onIndexCache.resize(_size);
	uint8_t* ptr = onIndexCache.data();
	memset(ptr, 0, _size * sizeof(uint8_t));
}

void Mask::read(InternalNode* node)
{
	targetNode = node;

	int dim = 1 << targetNode->log2dim;
	this->size = 1 << (3 * targetNode->log2dim);
	unsigned int wordCount = this->size >> 6;

	onIndexCache.resize(wordCount * 64);
	for (unsigned int i = 0; i < wordCount * 64; i += 8) {
		unsigned int byte = static_cast<unsigned int>(targetNode->sharedContext->bufferIterator->readBytes(1));
		// reverse and "push" to onIndexCache mask 
		for (unsigned int k = 0; k < 8; k++) {
			onIndexCache[i+k] = (byte & 0x01);
			byte = byte >> 1;
		}
	}

	this->onCache = countOn();
	this->offCache = countOff();

	// NOTE Cache mask indices, They don't do anything because no callback?
	/*this->forEachOn();
	this->forEachOff();*/
}

int Mask::countOn()
{
	if (onCache != -1) {
		return onCache;
	}

	int count = 0;
	count = static_cast<int>(std::count(onIndexCache.begin(), onIndexCache.end(), 1));

	return count;
}

int Mask::countOff()
{
	if (offCache != -1) {
		return offCache;
	}
	return size - countOn();
}

//void Mask::forEachOn()
//{
//	// this function does something for each word on cache
//}
//
//void Mask::forEachOff()
//{
//	// this function does something for each word off cache
//}

bool Mask::isOn(int offset)
{
	return this->onIndexCache[offset];
}

bool Mask::isOff(int offset)
{
	return !this->onIndexCache[offset];
}