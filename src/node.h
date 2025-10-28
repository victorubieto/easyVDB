#pragma once

#include <glm/vec3.hpp>

//#include "bufferIterator.h"
#include "mask.h"
#include "bbox.h"
#include "versions.h"

namespace easyVDB
{

	class Accessor;
	struct SharedContext;

	class InternalNode {
	public:
		SharedContext* sharedContext = nullptr;

		unsigned int log2dimMap[3] = { 5, 4, 3 };
		unsigned int totalMap[3] = { 4, 3, 0 };
		std::string nodeTypeMap[3] = { "internal", "internal", "leaf" };

		unsigned int leavesCount = 0;

		unsigned int depth = 0;
		unsigned int id = 0;
		glm::vec3 origin = {};
		float background = 0.0f;
		int total = 0;
		unsigned int dim = 0;
		int offsetMask = 0;

		unsigned int log2dim = 0;
		int numValues = 0;

		bool bboxInitialized = 0;
		Bbox localBbox;

		bool oldVersion = false;
		bool useCompression = false;

		std::vector<InternalNode*> table;

		Mask childMask; // node 5-4-3
		Mask valueMask; // node 5-4
		Mask selectionMask; // node 5-4

		std::vector<float> values; // node 5-4
		std::vector<float> data; // node 3

		InternalNode* parent = nullptr;
		InternalNode* firstChild = nullptr;

		InternalNode();

		void readTopology(unsigned int id, glm::vec3 origin, float background, unsigned int depth = 0);
		ErrorCode readValues(std::vector<float>& destBuffer, int destCount); // readCompressedValues() in openVDB git repo
		ErrorCode readData(unsigned int tempCount, std::vector<float>& outArray);
		ErrorCode readCompressedData(std::string codec, std::vector<float>& outArray);

		glm::vec3 traverseOffset(InternalNode* node);
		Bbox getLocalBbox();
		bool contains(glm::ivec3 pos);
		int coordToOffset(glm::ivec3 pos);
		float getValue(glm::ivec3 pos, Accessor* accessor = nullptr);
		bool isLeaf();
	};

	class RootNode {
	public:
		SharedContext* sharedContext = nullptr;

		unsigned int leavesCount = 0;

		float background = 0.0f;
		unsigned int numTiles = 0;
		unsigned int numChildren = 0;
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