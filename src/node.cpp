#include "node.h"

#include <bitset>
#include <iostream>

#include "accessor.h"
#include "compression.h"

RootNode::RootNode()
{
	origin = glm::vec3(0.0f);
	leavesCount = 0;
}

void RootNode::read()
{
	background = sharedContext->bufferIterator->readFloat(sharedContext->valueType);
	numTiles = sharedContext->bufferIterator->readBytes(uint32Size);
	numChildren = sharedContext->bufferIterator->readBytes(uint32Size);
	// init table
	origin = glm::vec3(0.0f);

	readChildren();
}

void RootNode::readChildren()
{
	if (numTiles == 0 && numChildren == 0) {
		std::cout << "[INFO] Empty root node" << std::endl;
		return;
	}

	leavesCount = 0;
	table.resize(numChildren);

	for (unsigned int i = 0; i < numTiles; i++) {
		readTile();
	}

	for (unsigned int i = 0; i < numChildren; i++) {
		readInternalNode(i);
	}
}

void RootNode::readTile()
{
	std::cout << "[WARN] Unsupported: Tile nodes" << std::endl;

	glm::vec3 vec(0.0f);
	vec.x = sharedContext->bufferIterator->readFloat(int32Type);
	vec.y = sharedContext->bufferIterator->readFloat(int32Type);
	vec.z = sharedContext->bufferIterator->readFloat(int32Type);

	float value = sharedContext->bufferIterator->readFloat(sharedContext->valueType);
	bool active = sharedContext->bufferIterator->readBool();

	// table.push_back(); // TODO: RootNode.js L54
}

void RootNode::readInternalNode(unsigned int id)
{
	glm::vec3 vec(0.0f);
	vec.x = sharedContext->bufferIterator->readFloat(int32Type);
	vec.y = sharedContext->bufferIterator->readFloat(int32Type);
	vec.z = sharedContext->bufferIterator->readFloat(int32Type);

	table[id].sharedContext = sharedContext;
	table[id].read(id, vec, background, 0);

	leavesCount += table[id].leavesCount;
}

float RootNode::getValue(glm::ivec3 pos, Accessor* accessor)
{
	float max = 0.0;

	for (unsigned int i = 0; i < this->table.size(); i++) {
		max = std::max(max, this->table[i].getValue(pos, accessor));

		if (max != 0.0) {
			i = table.size(); // break?
		}
	}

	return max;
}

InternalNode::InternalNode()
{
	firstChild = nullptr;
	parent = nullptr;

	bboxInitialized = false;
}

void InternalNode::read(unsigned int _id, glm::vec3 _origin, float _background, unsigned int _depth)
{
	this->depth = _depth;
	this->id = _id;
	this->origin = _origin;
	this->background = _background || 0.0f;

	this->log2dim = log2dimMap[_depth];
	std::string nodeType = _depth < (sizeof(nodeTypeMap) / sizeof(nodeTypeMap[0])) ? nodeTypeMap[_depth] : std::string("invalid");

	unsigned int sum = 0;
	for (unsigned int i = 0; i < (sizeof(totalMap) / sizeof(totalMap[0])); i++) {
		if (i >= _depth) {
			sum += totalMap[i];
		}
	}
	this->total = this->log2dim + sum;

	this->dim = 1 << total;
	this->numValues = 1 << (3 * this->log2dim);
	int level = 2 - _depth;
	int numVoxels = 1 << (3 * total);
	this->offsetMask = dim - 1;

	if (depth < 2) {
		childMask = Mask();
		childMask.read(this);
	}
	valueMask = Mask();
	valueMask.read(this);

	if (isLeaf()) {
		leavesCount = 1u;
	}
	else {
		table = std::vector<InternalNode*>(numValues, nullptr); // only init table for the non leafs
		leavesCount = 0u;
	}

	// init values
	firstChild = nullptr;

	if (*(sharedContext->version) < 214u) {
		std::cout << "[WARN] Unsupported: Internal-node compression" << std::endl;
		return;
	}

	readValues();
}

void InternalNode::readValues()
{
	oldVersion = *(sharedContext->version) < 222;
	useCompression = sharedContext->compression.activeMask;

	if (isLeaf()) {
		values = std::vector<float>(valueMask.size, 0.0f);

		for (int i = 0; i < valueMask.size; i++) {

			if (valueMask.countOn() == 0) {
				return;
			}

			for (unsigned int j = 0; j < valueMask.onIndexCache.size(); j++) {
				values[j] = (float)valueMask.onIndexCache[j];
			}
		}
		return;
	}

	numValues = oldVersion ? childMask.countOff() : numValues;
	unsigned int metadata = 0x110;

	if (*(sharedContext->version) >= 222u) {
		metadata = sharedContext->bufferIterator->readBytes(1u);
	}

	float inactiveVal1 = background;
	float inactiveVal0 = metadata == 6u ? background : !background;

	if (metadata == 2u || metadata == 4u || metadata == 5u) {
		std::cout << "[WARN] Unsupported: Compression::readCompressedValues first conditional" << std::endl;
		// Read one of at most two distinct inactive values.
		//     if (seek) {
		//         is.seekg(/*bytes=*/sizeof(ValueT), std::ios_base::cur);
		//     } else {
		//         is.read(reinterpret_cast<char*>(&inactiveVal0), /*bytes=*/sizeof(ValueT));
		//     }
		//     if (metadata == MASK_AND_TWO_INACTIVE_VALS) {
		//         // Read the second of two distinct inactive values.
		//         if (seek) {
		//             is.seekg(/*bytes=*/sizeof(ValueT), std::ios_base::cur);
		//         } else {
		//             is.read(reinterpret_cast<char*>(&inactiveVal1), /*bytes=*/sizeof(ValueT));
		//         }
		//     }
	}

	if (metadata == 3u || metadata == 4u || metadata == 5u) {
		selectionMask = Mask();
		selectionMask.read(this);
	}

	unsigned int tempCount = numValues;

	if (useCompression && metadata != 6 && *(sharedContext->version) >= 222u) {
		tempCount = valueMask.countOn();
	}

	readData(tempCount);

	if (useCompression && tempCount != numValues) {
		std::cout << "[WARN] Unsupported: Inactive values" << std::endl;
		// Restore inactive values, using the background value and, if available,
		//     // the inside/outside mask.  (For fog volumes, the destination buffer is assumed
		//     // to be initialized to background value zero, so inactive values can be ignored.)
		//     for (Index destIdx = 0, tempIdx = 0; destIdx < MaskT::SIZE; ++destIdx) {
		//         if (valueMask.isOn(destIdx)) {
		//             // Copy a saved active value into this node's buffer.
		//             destBuf[destIdx] = tempBuf[tempIdx];
		//             ++tempIdx;
		//         } else {
		//             // Reconstruct an unsaved inactive value and copy it into this node's buffer.
		//             destBuf[destIdx] = (selectionMask.isOn(destIdx) ? inactiveVal1 : inactiveVal0);
		//         }
		//     }
	}

	// childMask.forEachOn
	if (childMask.countOn() == 0) {
		return;
	}

	for (unsigned int j = 0; j < childMask.onIndexCache.size(); j++) {
		if (childMask.onIndexCache[j]) {
			int offset = j;

			int n = offset;
			int x = n >> (2 * log2dim);
			n &= (1 << (2 * log2dim)) - 1;
			int y = n >> log2dim;
			int z = n & ((1 << log2dim) - 1);
			glm::vec3 vec(x, y, z);

			InternalNode* child = new InternalNode();
			child->sharedContext = sharedContext;
			child->parent = this;
			child->read(offset, vec, background, depth + 1);

			child->origin.x = (int)vec.x << child->total;
			child->origin.y = (int)vec.y << child->total;
			child->origin.z = (int)vec.z << child->total;

			table[offset] = child;
			leavesCount += child->leavesCount;

			if (firstChild == nullptr) {
				firstChild = child;
			}
		}
	}
}

void InternalNode::readData(unsigned int tempCount)
{
	if (sharedContext->compression.blosc) {
		readCompressedData("blosc");
	}
	else if (sharedContext->compression.zlib) {
		readCompressedData("zlib");
	}
	else {
		for (unsigned int i = 0; i < tempCount; i++) {
			this->values.push_back(sharedContext->bufferIterator->readFloat(sharedContext->useHalf ? getPrecisionIdx("half") : sharedContext->valueType));
		}
	}
}

void InternalNode::readCompressedData(std::string codec)
{
	int zippedBytesCount = sharedContext->bufferIterator->readBytes(8);

	if (zippedBytesCount <= 0) {
		for (int i = 0; i < -zippedBytesCount; i++) {
			this->values.push_back(sharedContext->bufferIterator->readFloat(sharedContext->useHalf ? getPrecisionIdx("half") : sharedContext->valueType));
		}
	}
	else {
		uint8_t* zippedBytes = sharedContext->bufferIterator->readRawBytes(zippedBytesCount);
		uint8_t* resultBytes{0};

		try {
			int valueTypeLength = sharedContext->bufferIterator->floatingPointPrecisionLUT(sharedContext->valueType).size;
			int count = this->numValues;
			long long outputLength = valueTypeLength * count;
			resultBytes = new uint8_t[outputLength];

			if (codec == "zlib") {
				uncompressZlib(zippedBytesCount, zippedBytes, outputLength, resultBytes);
			}
			else if (codec == "blosc")
			{
				// TODO: decode codecs blosc, ChildNode.js L237
			}
			else
			{
				std::cout << "[WARN] Unsupported compression codec: " << codec << std::endl;
			}

			//memcpy(values.data(), resultBytes, outputLength);
			std::vector<uint8_t> my_vector(&resultBytes[0], &resultBytes[outputLength]);
			this->values.insert(values.end(), my_vector.begin(), my_vector.end());
		}
		catch (const std::exception& e) {
			std::cout << "[WARN] readZipData uncompress error: " << e.what() << std::endl;
			// check zippedBytes if fails
		}
	}
}

glm::vec3 InternalNode::traverseOffset(InternalNode* node)
{
	glm::vec3 offset(0.f);

	if (node != nullptr) {
		offset += node->origin;

		if (node->parent != nullptr) {
			offset += traverseOffset(node->parent);
		}
	}
	return offset;
}

Bbox InternalNode::getLocalBbox()
{
	if (this->bboxInitialized) {
		return this->localBbox;
	}

	glm::vec3 sumParentOffset = traverseOffset(this);

	this->localBbox = Bbox(sumParentOffset, sumParentOffset + (float)dim);
	this->bboxInitialized = true;
	return this->localBbox;
}

bool InternalNode::contains(glm::ivec3 pos)
{
	Bbox bbox = getLocalBbox();

	return (pos.x >= bbox.min.x && pos.x <= bbox.max.x &&
			pos.y >= bbox.min.y && pos.y <= bbox.max.y &&
			pos.z >= bbox.min.z && pos.z <= bbox.max.z);
}

int InternalNode::coordToOffset(glm::ivec3 pos)
{
	if (isLeaf()) {
		return	((pos.x & this->offsetMask) << (2 * this->log2dim) |
				((pos.y & this->offsetMask) << this->log2dim) |
				(pos.z & this->offsetMask));
	}
	else
	{
		return	(((pos.x & this->offsetMask) >> this->firstChild->total) << (2 * this->log2dim)) |
				(((pos.y & this->offsetMask) >> this->firstChild->total) << this->log2dim) |
				((pos.z & this->offsetMask) >> this->firstChild->total);
	}
}

float InternalNode::getValue(glm::ivec3 pos, Accessor* accessor)
{
	if (!this->contains(pos)) {
		return 0.f;
	}

	if (accessor != nullptr) {
		accessor->cache(this);
	}

	if (isLeaf()) {
		return this->valueMask.isOn(this->coordToOffset(pos)) ? 1.0 : 0.0;
	}

	InternalNode* targetChild = this->table[this->coordToOffset(pos)];

	if (targetChild != nullptr) {
		return targetChild->getValue(pos, accessor);
	}

	return 0.f;
}

bool InternalNode::isLeaf()
{
	return depth >= 2;
}