#pragma once

#include <glm/vec3.hpp>

#include "bufferIterator.h"
#include "mask.h"
#include "bbox.h"

namespace easyVDB
{

	class Accessor;

	class InternalNode {
	public:
		SharedContext* sharedContext;

		unsigned int log2dimMap[3] = { 5, 4, 3 };
		unsigned int totalMap[3] = { 4, 3, 0 };
		std::string nodeTypeMap[3] = { "internal", "internal", "leaf" };

		unsigned int leavesCount;

		unsigned int depth;
		unsigned int id;
		glm::vec3 origin;
		float background;
		int total;
		unsigned int dim;
		int offsetMask;

		unsigned int log2dim;
		int numValues;

		bool bboxInitialized;
		Bbox localBbox;

		bool oldVersion;
		bool useCompression;

		std::vector<InternalNode*> table;

		Mask childMask; // node 5-4-3
		Mask valueMask; // node 5-4
		Mask selectionMask; // node 5-4

		std::vector<float> values; // node 5-4
		std::vector<float> data; // node 3

		InternalNode* parent;
		InternalNode* firstChild;

		InternalNode();

		void read(unsigned int id, glm::vec3 origin, float background, unsigned int depth = 0);
		void readValues();
		void readData(const bool seek, uint32_t offset, uint32_t num_indices, unsigned int tempCount, std::vector<float>& outArray);
		void readCompressedData(std::string codec, std::vector<float>& outArray);

		glm::vec3 traverseOffset(InternalNode* node);
		Bbox getLocalBbox();
		bool contains(glm::ivec3 pos);
		int coordToOffset(glm::ivec3 pos);
		float getValue(glm::ivec3 pos, Accessor* accessor = nullptr);
		bool isLeaf();
	};

	class RootNode {
	public:
		SharedContext* sharedContext;

		unsigned int leavesCount;

		float background;
		unsigned int numTiles;
		unsigned int numChildren;
		glm::vec3 origin;

		std::vector<InternalNode> table; // ?

		RootNode();

		void read();
		void readChildren();
		void readTile();
		void readInternalNode(unsigned int id);

		float getValue(glm::ivec3 pos, Accessor* accessor = nullptr);
		virtual bool isLeaf() { return false; };
	};

}