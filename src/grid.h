#pragma once

#include <vector>
#include <string>

#include <glm/vec3.hpp>
#include <glm/common.hpp>

#include "node.h"
#include "bbox.h"

const std::string uniformScaleTranslateMap = "UniformScaleTranslateMap";
const std::string scaleTranslateMap = "ScaleTranslateMap";
const std::string uniformScaleMap = "UniformScaleMap";
const std::string scaleMap = "ScaleMap";
const std::string translationMap = "TranslationMap";
const std::string unitaryMap = "UnitaryMap";
const std::string nonlinearFrustumMap = "NonlinearFrustumMap";

class VDB_Transform {
public:

	// transformMap
	std::string mapType;
	glm::vec3 translation;
	glm::vec3 scale;
	glm::vec3 voxelSize;
	glm::vec3 scaleInverse;
	glm::vec3 scaleInverseSq;
	glm::vec3 scaleInverseDouble;

	VDB_Transform();

	void applyTransformMap(glm::vec3& vector) {
		if (this->mapType == uniformScaleTranslateMap || this->mapType == scaleTranslateMap) {
			vector = vector * this->scale;
		}
		else if (this->mapType == uniformScaleMap || this->mapType == scaleMap) {
			vector = vector * this->scale + this->translation; // check the operation order
		}
		else if (this->mapType == translationMap) {
			vector = vector + this->translation;
		}
		else if (this->mapType == unitaryMap) {
			std::cout << "[WARN] Unsupported: Grid type UnitaryMap" << std::endl;
		}
		else if (this->mapType == nonlinearFrustumMap) {
			std::cout << "[WARN] Unsupported: Grid type NonLinearFrustumMap" << std::endl;
		}
		else {
			std::cout << "[WARN] Unsupported: Grid type matrix4x4" << std::endl;
		}
	};

	void applyInverseTransformMap(glm::vec3& vector) {
		glm::vec3 result;
		if (this->mapType == uniformScaleTranslateMap || this->mapType == scaleTranslateMap) {
			vector = vector * this->scaleInverse;
		}
		else if (this->mapType == uniformScaleMap || this->mapType == scaleMap) {
			vector = vector * this->scaleInverse;
		}
		else if (this->mapType == translationMap) {
			vector = vector - this->translation - this->translation;
		}
		else if (this->mapType == unitaryMap) {
			std::cout << "[WARN] Unsupported: Grid type UnitaryMap" << std::endl;
		}
		else if (this->mapType == nonlinearFrustumMap) {
			std::cout << "[WARN] Unsupported: Grid type NonLinearFrustumMap" << std::endl;
		}
		else {
			std::cout << "[WARN] Unsupported: Grid type Matrix4x4" << std::endl;
		}
	}
};

class Grid {
public:
	SharedContext* sharedContext;

	Accessor* accessor;

	std::string halfFloatGridPrefix = "_HalfFloat";

	std::string instanceParentName;

	bool saveAsHalfFloat = false;
	unsigned int leavesCount = 0u;

	std::string uniqueName;
	std::string gridName;
	std::string gridType;

	unsigned int gridBufferPosition;
	float blockBufferPosition;
	float endBufferPosition;

	//Compression compression;
	std::vector<Metadata> metadata;

	VDB_Transform* transform;

	RootNode root;

	Grid();

	void read();
	void readHeader();
	void readCompression();
	void readMetadata();
	void readGridTransform();
	void readTopology();
	void readBuffers();

	float getValue(glm::vec3 pos);
	std::string getMetadata(std::string s);
	Bbox getPreciseWorldBbox();
	Precision getGridValueType();
};