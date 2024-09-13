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

		std::vector<InternalNode*> table;
		std::vector<float> values;

		bool oldVersion;
		bool useCompression;

		Mask childMask;
		Mask valueMask;
		Mask selectionMask;

		InternalNode* parent;
		InternalNode* firstChild;

		InternalNode();

		void read(unsigned int id, glm::vec3 origin, float background, unsigned int depth = 0);
		void readValues();
		void readData(const bool seek, uint32_t offset, uint32_t num_indices, unsigned int tempCount);
		void readCompressedData(std::string codec);

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