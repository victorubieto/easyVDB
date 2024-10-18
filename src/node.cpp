#include "node.h"

#include <bitset>
#include <iostream>

#include "accessor.h"
#include "compression.h"
#include "versions.h"

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
	this->numValues = 1 << (3 * this->log2dim); // linear dimension
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
	oldVersion = *(sharedContext->version) < OPENVDB_FILE_VERSION_NODE_MASK_COMPRESSION;
	useCompression = sharedContext->compression.activeMask;
	const bool seek = this->values.size() == 0;

	if (isLeaf()) {
		// We can skip this since a node3 only stores the value mask, which was already read in the previous function
		//return;

		// REMOVE???
		this->values = std::vector<float>(valueMask.size, 0.0f);

		if (valueMask.countOn() == 0) {
			return;
		}

		for (int i = 0; i < valueMask.size; i++) {
			this->values[i] = (float)valueMask.onIndexCache[i];
		}

		return;
	}

	int destCount = oldVersion ? childMask.countOff() : this->numValues;
	unsigned int metadata = NodeMetaData::NoMaskAndAllVals;

	// Get delayed load metadata if it exists
	//uint64_t leafIndex = 0;
	//if (seek && this->sharedContext->useDelayedLoadMeta) {
	//	// leafINndex = meta->leaf();
	//}

	uint32_t offset = 0;
	uint32_t num_indices = 0;
	if (*(sharedContext->version) >= OPENVDB_FILE_VERSION_NODE_MASK_COMPRESSION) {
		if (seek && !useCompression) {
			sharedContext->bufferIterator->readBytes(1u); // skip 1 byte
		}
		// to do: optimize
		else if (seek && this->sharedContext->useDelayedLoadMeta) {
			std::string delayLoadMeta = this->sharedContext->delayedLoadMeta;

			num_indices = static_cast<uint8_t>(delayLoadMeta[offset++]); // number of total leaves
			num_indices |= static_cast<uint8_t>(delayLoadMeta[offset++]) << 8;
			num_indices |= static_cast<uint8_t>(delayLoadMeta[offset++]) << 16;
			num_indices |= static_cast<uint8_t>(delayLoadMeta[offset++]) << 24;

			uint32_t mMask_length = static_cast<uint8_t>(delayLoadMeta[offset++]); // compressed
			mMask_length |= static_cast<uint8_t>(delayLoadMeta[offset++]) << 8;
			mMask_length |= static_cast<uint8_t>(delayLoadMeta[offset++]) << 16;
			mMask_length |= static_cast<uint8_t>(delayLoadMeta[offset++]) << 24;

			uint8_t* mMask = new uint8_t[num_indices];
			uint8_t* compressedArray = reinterpret_cast<uint8_t*> (delayLoadMeta.data()) + offset;
			uncompressBlosc(mMask_length, compressedArray, num_indices * sizeof(uint8_t), mMask);
			offset += mMask_length;

			metadata = mMask[0]; // leafIndex = 0

			sharedContext->bufferIterator->readBytes(1u); // skip 1 byte
		}
		else {
			metadata = sharedContext->bufferIterator->readBytes(1u); // 1 byte
		}
	}

	float inactiveVal1 = background;
	float inactiveVal0 = metadata == NodeMetaData::NoMaskOrInactiveVals ? background : -background;

	if (metadata == NodeMetaData::NoMaskAndOneInactiveVal || metadata == NodeMetaData::MaskAndOneInactiveVal || metadata == NodeMetaData::MaskAndTwoInactiveVals) {
		std::cout << "[WARN] Unsupported: Compression::readCompressedValues first conditional" << std::endl;
		// ref: https://github.com/Traverse-Research/vdb-rs/blob/f146fc083a2df19da555b1c976ed8cd6b5498748/src/reader.rs#L324
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
		assert(false);
	}

	selectionMask = Mask(numValues);
	if (metadata == NodeMetaData::MaskAndNoInactiveVals || metadata == NodeMetaData::MaskAndOneInactiveVal || metadata == NodeMetaData::MaskAndTwoInactiveVals) {
		selectionMask.read(this);
	}

	unsigned int tempCount = destCount;
	std::vector<float> tempBuf = this->values;

	if (useCompression && metadata != NodeMetaData::NoMaskAndAllVals && *(sharedContext->version) >= OPENVDB_FILE_VERSION_NODE_MASK_COMPRESSION) {
		tempCount = valueMask.countOn();
		if (tempCount != destCount) { // todo: add !seek in the condition
			// If this node has inactive voxels, allocate a temporary buffer into which to read just the active values.
			tempBuf.resize(tempCount);
		}
	}

	this->values = std::vector<float>(this->numValues);
	readData(seek, offset, num_indices, tempCount, this->values); // read value array of nodes 5-4

	// https://github.com/AcademySoftwareFoundation/openvdb/blob/aea0f0e022141a32fee66378d1fee53b3a3fbc2e/openvdb/openvdb/io/Compression.h#L569
	// If mask compression is enabled and the number of active values read into the temp buffer is
	// smaller than the size of the destination buffer, then there are missing (inactive) values
	if (useCompression && tempCount != destCount) {
		// ref: https://github.com/Traverse-Research/vdb-rs/blob/f146fc083a2df19da555b1c976ed8cd6b5498748/src/reader.rs#L369
		this->values.resize(numValues);
		// Restore inactive values, using the background value and, if available,
		// the inside/outside mask. (For fog volumes, the destination buffer is assumed
		// to be initialized to background value zero, so inactive values can be ignored.)
		for (int32_t destIdx = 0, tempIdx = 0; destIdx < this->childMask.size; destIdx++)
		{
			if (valueMask.isOn(destIdx)) {
				// Copy a saved active value into this node's buffer.
				this->values[destIdx] = tempBuf[tempIdx];
				tempIdx++;
			}
			else {
				// Reconstruct an unsaved inactive value and copy it into this node's buffer
				this->values[destIdx] = selectionMask.isOn(destIdx) ? inactiveVal1 : inactiveVal0;
			}
		}
	}

	// In case we have a leaf here, end the function
	if (childMask.countOn() == 0) {
		return;
	}

	// Iterate recursively for all the children nodes
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

void InternalNode::readData(const bool seek, uint32_t offset, uint32_t num_indices, unsigned int tempCount, std::vector<float>& outArray)
{
	if (sharedContext->useHalf) {
		if (tempCount < 1) return;
	}

	/*if (!seek) {
		std::cout << "TO DO: seek" << std::endl;
		assert(false);
	}*/

	if (this->sharedContext->useDelayedLoadMeta && seek && useCompression) {
		// to do: optimize
		std::string delayLoadMeta = this->sharedContext->delayedLoadMeta;

		uint32_t mCompressedSize_length = static_cast<uint8_t>(delayLoadMeta[offset++]); // length of compressed mCompressedSize vector ()
		mCompressedSize_length |= static_cast<uint8_t>(delayLoadMeta[offset++]) << 8;
		mCompressedSize_length |= static_cast<uint8_t>(delayLoadMeta[offset++]) << 16;
		mCompressedSize_length |= static_cast<uint8_t>(delayLoadMeta[offset++]) << 24;

		if (mCompressedSize_length == 0x7FFFFFFF) { // there is no compressedSize info (check)
			// to do maybe
			std::cout << "Unsupported" << std::endl;
			assert(false);
		} 
		else if (mCompressedSize_length == 0) {
			std::cout << "Unsupported" << std::endl;
			assert(false);
		}

		uint8_t* tempCompressedSize = new uint8_t[num_indices * sizeof(int64_t)];
		uint8_t* compressedArray = reinterpret_cast<uint8_t*> (delayLoadMeta.data()) + offset;
		uncompressBlosc(mCompressedSize_length, compressedArray, num_indices * sizeof(int64_t), tempCompressedSize);
		int64_t* mCompressedSize = reinterpret_cast<int64_t*>(tempCompressedSize);

		// size_t compressedSize = metadata->getCompressedSize(metadataOffset);
		int64_t compressedSize = mCompressedSize[0]; // leafIndex = 0

		sharedContext->bufferIterator->readBytes(compressedSize); // skip compressedSize bytes
	}
	else if (sharedContext->compression.blosc) {
		readCompressedData("blosc", outArray);
	}
	else if (sharedContext->compression.zlib) {
		readCompressedData("zlib", outArray);
	}
	else if (seek) {
		PrecisionLUT precisionLUT = sharedContext->bufferIterator->floatingPointPrecisionLUT(sharedContext->useHalf ? getPrecisionIdx("half") : sharedContext->valueType);
		sharedContext->bufferIterator->readBytes(precisionLUT.size * tempCount); // skip as much bytes as size * length of data type
	}
	else {
		for (unsigned int i = 0; i < tempCount; i++) {
			outArray[i] = sharedContext->bufferIterator->readFloat(sharedContext->useHalf ? getPrecisionIdx("half") : sharedContext->valueType);
		}
	}
}

void InternalNode::readCompressedData(std::string codec, std::vector<float> &outArray)
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

		try {
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
			}

			//std::vector<uint8_t> my_vector(&decompressedBytes[0], &decompressedBytes[outputLength]);

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
					break;
			}
			
		}
		catch (const std::exception& e) {
			std::cout << "[WARN] " + codec + " uncompress error: " << e.what() << std::endl;
			// check compressedBytes if fails
			assert(false);
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
		//return this->valueMask.isOn(this->coordToOffset(pos)) ? 1.0 : 0.0; // old, this reads the mask instead of the correct voxel value
		
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