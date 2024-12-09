#include "node.h"

#include <bitset>
#include <iostream>

#include "bufferIterator.h"
#include "versions.h"

#include "accessor.h"
#include "compression.h"

using namespace easyVDB;

RootNode::RootNode()
{
	origin = glm::vec3(0.0f);
	leavesCount = 0;
}

void RootNode::read()
{
	background = sharedContext->bufferIterator->readFloat(floatType); // always read background as float, even if the data is stored as half floats
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
	table[id].readTopology(id, vec, background, 0);

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
	this->firstChild = nullptr;
	this->parent = nullptr;

	this->bboxInitialized = false;
}

void InternalNode::readTopology(unsigned int _id, glm::vec3 _origin, float _background, unsigned int _depth)
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
	this->numValues = 1 << (3 * this->log2dim); // linear dimension
	int level = 2 - _depth;
	int numVoxels = 1 << (3 * total);
	this->offsetMask = dim - 1;

	// Read Child Mask
	if (depth < 2) {
		childMask = Mask();
		childMask.read(this);
	}
	// Read Value Mask
	valueMask = Mask();
	valueMask.read(this);

	if (isLeaf()) {
		// In case we have a leaf here, end the function
		leavesCount = 1u;
		this->sharedContext->delayedMetada.leafIndex++;

		// We can skip this since a node3 only stores the value mask in the topology (?)
		//return;

		// this helps to debug if topology is parsed correctly
		this->values = std::vector<float>(this->valueMask.size, 0.0f);

		if (this->valueMask.countOn() == 0) {
			return;
		}

		for (int i = 0; i < this->valueMask.size; i++) {
			this->values[i] = (float)this->valueMask.onIndexCache[i];
		}

		return;
	}
	else {
		table = std::vector<InternalNode*>(numValues, nullptr); // only init table for the non leafs
		leavesCount = 0u;
	}

	this->firstChild = nullptr;

	// Read Values Array
	if (*(this->sharedContext->version) < OPENVDB_FILE_VERSION_INTERNALNODE_COMPRESSION) {
		std::cout << "[WARN] Unsupported: Internal-node compression" << std::endl;
		// https://github.com/AcademySoftwareFoundation/openvdb/blob/5201109680912e0f8333971dfabd33c13655b314/openvdb/openvdb/tree/InternalNode.h#L2205
		return;
	}
	else {
		this->oldVersion = *(this->sharedContext->version) < OPENVDB_FILE_VERSION_NODE_MASK_COMPRESSION;
		int num_values = this->oldVersion ? this->childMask.countOff() : this->numValues;

		this->values.resize(num_values);
		readValues(this->values, num_values);

		// Iterate and read recursively all the child nodes
		for (unsigned int j = 0; j < this->childMask.onIndexCache.size(); j++) {
			if (this->childMask.onIndexCache[j]) {
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
				child->readTopology(offset, vec, background, depth + 1);

				child->origin.x = (int)vec.x << child->total;
				child->origin.y = (int)vec.y << child->total;
				child->origin.z = (int)vec.z << child->total;

				this->table[offset] = child;
				this->leavesCount += child->leavesCount;

				if (this->firstChild == nullptr) {
					this->firstChild = child;
				}
			}
		}
	}
}

ErrorCode InternalNode::readValues(std::vector<float>& destBuffer, int destCount)
{
	// Get compression settings
	this->useCompression = this->sharedContext->compression.activeMask;
	DelayedLoadMetadata& delayedMetada = this->sharedContext->delayedMetada;
	unsigned int metadata = EASYVDB_NO_MASK_AND_ALL_VALS;

	// Get delayed load metadata if it exists
	uint64_t leafIndex = 0;
	if (this->sharedContext->useDelayedLoadMeta) {
		leafIndex = delayedMetada.leafIndex;
	}

	if (*(this->sharedContext->version) >= OPENVDB_FILE_VERSION_NODE_MASK_COMPRESSION) {
		if (!this->useCompression) {
			this->sharedContext->bufferIterator->readBytes(1u); // skip 1 byte
		}
		else if (this->sharedContext->useDelayedLoadMeta) {
			// read metadata from delayed load meta and skip 1 byte
			uint32_t& offset_ref = delayedMetada.offset = 0;
			uint32_t& num_indices_ref = delayedMetada.num_indices = 0;
			std::string& metadata_string_ref = delayedMetada.metadata_string;

			num_indices_ref = static_cast<uint8_t>(metadata_string_ref[offset_ref++]); // number of total leaves
			num_indices_ref |= static_cast<uint8_t>(metadata_string_ref[offset_ref++]) << 8;
			num_indices_ref |= static_cast<uint8_t>(metadata_string_ref[offset_ref++]) << 16;
			num_indices_ref |= static_cast<uint8_t>(metadata_string_ref[offset_ref++]) << 24;

			uint32_t mMask_length = static_cast<uint8_t>(metadata_string_ref[offset_ref++]); // compressed
			mMask_length |= static_cast<uint8_t>(metadata_string_ref[offset_ref++]) << 8;
			mMask_length |= static_cast<uint8_t>(metadata_string_ref[offset_ref++]) << 16;
			mMask_length |= static_cast<uint8_t>(metadata_string_ref[offset_ref++]) << 24;

			uint8_t* mMask = new uint8_t[num_indices_ref];
			uint8_t* compressedArray = reinterpret_cast<uint8_t*> (metadata_string_ref.data()) + offset_ref;
			uncompressBlosc(mMask_length, compressedArray, num_indices_ref * sizeof(uint8_t), mMask);
			offset_ref += mMask_length;

			metadata = mMask[leafIndex];
			this->sharedContext->bufferIterator->readBytes(1u);
		}
		else {
			metadata = this->sharedContext->bufferIterator->readBytes(1u); // read 1 byte
		}
	}

	float inactiveVal1 = this->background;
	float inactiveVal0 = metadata == EASYVDB_NO_MASK_OR_INACTIVE_VALS ? this->background : -this->background;

	if (metadata == EASYVDB_NO_MASK_AND_ONE_INACTIVE_VAL || metadata == EASYVDB_MASK_AND_ONE_INACTIVE_VAL || metadata == EASYVDB_MASK_AND_TWO_INACTIVE_VALS)
	{
		std::cout << "[WARN] Unsupported: Different inactive values behavior" << std::endl;
		// https://github.com/AcademySoftwareFoundation/openvdb/blob/5201109680912e0f8333971dfabd33c13655b314/openvdb/openvdb/io/Compression.h#L513
	}

	this->selectionMask = Mask(this->numValues);
	// For use in mask compression (only), read the bitmask that selects between two distinct inactive values
	if (metadata == EASYVDB_MASK_AND_NO_INACTIVE_VALS || metadata == EASYVDB_MASK_AND_ONE_INACTIVE_VAL || metadata == EASYVDB_MASK_AND_TWO_INACTIVE_VALS) {
		this->selectionMask.read(this);
	}

	unsigned int tempCount = destCount;
	std::vector<float> tempBuf = std::vector<float>(this->numValues);

	if (this->useCompression && metadata != EASYVDB_NO_MASK_AND_ALL_VALS && *(this->sharedContext->version) >= OPENVDB_FILE_VERSION_NODE_MASK_COMPRESSION) {
		tempCount = this->valueMask.countOn();
		// If this node has inactive voxels, allocate a temporary buffer into which to read just the active values.
		if (tempCount != destCount) {
			tempBuf.resize(tempCount);
		}
	}

	// Read in the buffer
	readData(tempCount, tempBuf); // read value array of nodes 5-4

	// If mask compression is enabled and the number of active values read into the temp buffer is 
	// smaller than the size of the destination buffer, then there are missing (inactive) values
	if (this->useCompression && tempCount != destCount)
	{
		// Restore inactive values, using the background value and, if available, the inside/outside mask. 
		// (For fog volumes, the destination buffer is assumed to be initialized to background value zero, so inactive values can be ignored.)
		for (int32_t destIdx = 0, tempIdx = 0; destIdx < this->valueMask.size; destIdx++) {
			if (this->valueMask.isOn(destIdx)) {
				// Copy a saved active value into this node's buffer.
				destBuffer[destIdx] = tempBuf[tempIdx];
				tempIdx++;
			}
			else {
				// Reconstruct an unsaved inactive value and copy it into this node's buffer
				destBuffer[destIdx] = (this->selectionMask.size > 0 && this->selectionMask.isOn(destIdx)) ? inactiveVal1 : inactiveVal0;
			}
		}
	}

	return EASYVDB_SUCCESS;
}

ErrorCode InternalNode::readData(unsigned int tempCount, std::vector<float>& outArray)
{
	if (sharedContext->useHalf && tempCount < 1) {
		return EASYVDB_SUCCESS;
	}

	if (this->sharedContext->useDelayedLoadMeta && this->useCompression && false) { // only if seek, remove (?)
		// to do: optimize
		uint32_t& offset_ref = this->sharedContext->delayedMetada.offset;
		uint32_t& num_indices_ref = this->sharedContext->delayedMetada.num_indices;
		std::string& metadata_string_ref = this->sharedContext->delayedMetada.metadata_string;

		uint32_t mCompressedSize_length = static_cast<uint8_t>(metadata_string_ref[offset_ref++]); // length of compressed mCompressedSize vector ()
		mCompressedSize_length |= static_cast<uint8_t>(metadata_string_ref[offset_ref++]) << 8;
		mCompressedSize_length |= static_cast<uint8_t>(metadata_string_ref[offset_ref++]) << 16;
		mCompressedSize_length |= static_cast<uint8_t>(metadata_string_ref[offset_ref++]) << 24;

		if (mCompressedSize_length == 0x7FFFFFFF) { // there is no compressedSize info (check)
			// to do maybe
			std::cout << "Unsupported" << std::endl;
			assert(false);
		}
		else if (mCompressedSize_length == 0) {
			std::cout << "Unsupported" << std::endl;
			assert(false);
		}

		uint8_t* tempCompressedSize = new uint8_t[num_indices_ref * sizeof(int64_t)];
		uint8_t* compressedArray = reinterpret_cast<uint8_t*> (metadata_string_ref.data()) + offset_ref;
		uncompressBlosc(mCompressedSize_length, compressedArray, num_indices_ref * sizeof(int64_t), tempCompressedSize);
		int64_t* mCompressedSize = reinterpret_cast<int64_t*>(tempCompressedSize);

		// size_t compressedSize = metadata->getCompressedSize(metadataOffset);
		int64_t compressedSize = mCompressedSize[this->sharedContext->delayedMetada.leafIndex];

		sharedContext->bufferIterator->readBytes(compressedSize); // skip compressedSize bytes
	}
	else if (this->sharedContext->compression.blosc) {
		return readCompressedData("blosc", outArray);
	}
	else if (this->sharedContext->compression.zlib) {
		return readCompressedData("zlib", outArray);
	}
	else {
		for (unsigned int i = 0; i < tempCount; i++) {
			outArray[i] = this->sharedContext->bufferIterator->readFloat(this->sharedContext->useHalf ? getPrecisionIdx("half") : this->sharedContext->valueType);
		}
	}

	return EASYVDB_SUCCESS;
}

ErrorCode InternalNode::readCompressedData(std::string codec, std::vector<float> &outArray)
{
	long long compressedBytesCount = sharedContext->bufferIterator->readBytes(8);

	if (compressedBytesCount <= 0) {
		PrecisionLUT precisionLUT = sharedContext->bufferIterator->floatingPointPrecisionLUT(sharedContext->useHalf ? getPrecisionIdx("half") : sharedContext->valueType);
		unsigned int s = precisionLUT.size;

		if (sharedContext->useHalf)
		{
			for (int i = 0; i < -compressedBytesCount; i += s) {
				outArray[i] = sharedContext->bufferIterator->readFloat(getPrecisionIdx("half"));
			}
		}
		else {
			for (int i = 0; i < -compressedBytesCount; i += s) {
				outArray[i] = sharedContext->bufferIterator->readFloat(sharedContext->valueType);
			}
		}
	}
	else {
		uint8_t* compressedBytes = sharedContext->bufferIterator->readRawBytes(compressedBytesCount);
		uint8_t* decompressedBytes{ 0 };

		int valueTypeLength = sharedContext->bufferIterator->floatingPointPrecisionLUT(sharedContext->valueType).size;
		long long outputLength = valueTypeLength * this->numValues;
		decompressedBytes = new uint8_t[outputLength];

		if (codec == "zlib") {
			uncompressZlib(compressedBytesCount, compressedBytes, outputLength, decompressedBytes);
		}
		else if (codec == "blosc") {
			uncompressBlosc(compressedBytesCount, compressedBytes, outputLength, decompressedBytes);
		}
		else {
			std::cout << "[WARN] Unsupported compression codec: " << codec << std::endl;
			return EASYVDB_UNKNOWN_COMPRESSION;
		}

		// join bytes depending on the value type
		switch (sharedContext->valueType)
		{
			case halfFloatType:
			{
				uint16_t* halfFloatBytes = reinterpret_cast<uint16_t*>(decompressedBytes);
				std::vector<uint16_t> halfFloatVec(&halfFloatBytes[0], &halfFloatBytes[outputLength / 2]); // correct the hardcoded 2
				for (int i = 0; i < halfFloatVec.size(); i++) {
					outArray[i] = half_to_float(halfFloatVec[i]);
				}
				break;
			}
			case floatType:
			{
				float* floatBytes = reinterpret_cast<float*>(decompressedBytes);
				std::vector<float> floatVec(&floatBytes[0], &floatBytes[outputLength / 4]); // correct the hardcoded 4
				outArray = floatVec;
				break;
			}
			default:
				std::cout << "[WARN] Cannot decompress. Unknown out type for the values." << std::endl;
				return EASYVDB_UNKNOWN_VALUE_TYPE;
		}
	}

	return EASYVDB_SUCCESS;
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
		//return this->valueMask.isOn(this->coordToOffset(pos)) ? 1.0 : 0.0; // old, this reads the mask instead of the correct voxel value (useful to test topology)
		
		// reads the tree data, if voxel is not activated, return 0 as value
		int offsetPos = this->coordToOffset(pos);
		return this->valueMask.isOn(offsetPos) ? this->data[offsetPos] : 0.0f;
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