#include "grid.h"

#include <iostream>
#include <string>
#include <sstream>

#include "accessor.h"
#include "versions.h"

Grid::Grid()
{
	accessor = new Accessor(this);
	transform = new VDB_Transform();
}

void Grid::read()
{
	readHeader(); // 11 + 30 + 4 + 8 + 8 + 8 = 69 bytes

	if (gridBufferPosition != sharedContext->bufferIterator->offset) {
		sharedContext->bufferIterator->offset = gridBufferPosition; // seek to grid
	}

	// Read grid interval
	readCompression();	// 4 bytes
	readMetadata();		// 43575 bytes (because of the strange metadata string)

	if (*(sharedContext->version) >= OPENVDB_FILE_VERSION_GRID_INSTANCING) {
		readGridTransform();	// 28 + 144 = 172 btyes
		readTopology();			// 
		readBuffers(); // read tree data
	}
	else {
		readTopology();
		readGridTransform();
		readBuffers();
	}

	// Hack to make multi - grid VDBs work without reading leaf values
	sharedContext->bufferIterator->offset = endBufferPosition;
}

void Grid::readHeader()
{
	uniqueName = sharedContext->bufferIterator->readString(); // "density" are 7 chars, so we read size = 4 bytes (which is 7) + 7 chars = 11 bytes
	gridName = uniqueName.substr(0, uniqueName.find("\x1e"));
	gridType = sharedContext->bufferIterator->readString(); // "Tree_float_5_4_3_HalfFloat" are 26 + 4 of size = 30 bytes
	if (gridType.find(halfFloatGridPrefix) != -1) {
		// TODO in case it contains half floats
		saveAsHalfFloat = true;
		char* ptr;
		ptr = strtok((char*)gridType.data(), halfFloatGridPrefix.data());
		//gridType = gridType.split(halfFloatGridPrefix).join("");
	}

	if (*(sharedContext->version) >= OPENVDB_FILE_VERSION_GRID_INSTANCING) {
		instanceParentName = sharedContext->bufferIterator->readString(); // 0, so 4 bytes
	}
	else {
		//todo!("instance_parent, file version: {}", header.file_version)
	}

	// Buffer offset at which grid description ends
	gridBufferPosition = sharedContext->bufferIterator->readFloat(int64Type); // 8 bytes (is 134, which is the offset two 8 bytes more from here)
	// Buffer offset at which grid blocks end
	blockBufferPosition = sharedContext->bufferIterator->readFloat(int64Type); // 8 bytes (1.011.285)
	// Buffer offset at which the file ends
	endBufferPosition = sharedContext->bufferIterator->readFloat(int64Type); // 8 bytes (12.340.550, exactly the size of the file, because 1 grid)
}

void Grid::readCompression()
{
	if (*(sharedContext->version) >= OPENVDB_FILE_VERSION_NODE_MASK_COMPRESSION) {
		unsigned int compress = sharedContext->bufferIterator->readBytes(uint32Size);
		sharedContext->compression.none = compress & 0x0;
		sharedContext->compression.zlib = compress & 0x1;
		sharedContext->compression.activeMask = compress & 0x2;
		sharedContext->compression.blosc = compress & 0x4;
	}
	else {
		// NOTE Use inherited compression instead
		// TODO ??
	}
}

void Grid::readMetadata()
{
	unsigned int metadataSize = sharedContext->bufferIterator->readBytes(uint32Size);	// 4 bytes (we will have 11 entries)
	for (int i = 0; i < metadataSize; i++) {
		std::string name = sharedContext->bufferIterator->readString();					// 9 + 17 + 17 + 20 + 21 + 18 + 20 + 26 + 8 + 19 + 12 = 187 bytes
		std::string type = sharedContext->bufferIterator->readString();					// 10 + 9 + 9 + 10 + 17 + 9 + 9 + 8 + 10 + 9 + 9 = 109 bytes
		std::string value = sharedContext->bufferIterator->readString(type);			// 11 + 16 + 16 + 25 + 43151 + 12 + 12 + 5 + 11 + 8 + 8 = 43275 bytes

		if (name == "file_delayed_load")
		{
			sharedContext->useDelayedLoadMeta = true;
			sharedContext->delayedLoadMeta = value;
		}

		metadata.push_back(Metadata(name, type, value));
	}

	// todo check
	if (*(sharedContext->version) < 219u) {
		for (int i = 0; i < metadataSize; i++) {
			if (metadata[i].name == "name")
				metadata[i].value = gridName;
		}
	}
}

void Grid::readGridTransform()
{
	//transform = new Transform(); // already in the constructor

	transform->mapType = sharedContext->bufferIterator->readString(); // 28 bytes
	if (*(sharedContext->version) < 219u) {
		std::cout << "[WARN] GridDescriptor::getGridTransform old - style transforms currently not supported. Fallback to identity transform." << std::endl;
		return;
	}

	if (transform->mapType == uniformScaleTranslateMap || transform->mapType == scaleTranslateMap) { // 144 bytes
		transform->translation = sharedContext->bufferIterator->readVector3();
		transform->scale = sharedContext->bufferIterator->readVector3();
		transform->voxelSize = sharedContext->bufferIterator->readVector3();
		transform->scaleInverse = sharedContext->bufferIterator->readVector3();
		transform->scaleInverseSq = sharedContext->bufferIterator->readVector3();
		transform->scaleInverseDouble = sharedContext->bufferIterator->readVector3();
	}
	else if (transform->mapType == uniformScaleMap || transform->mapType == scaleMap) {
		transform->scale = sharedContext->bufferIterator->readVector3();
		transform->voxelSize = sharedContext->bufferIterator->readVector3();
		transform->scaleInverse = sharedContext->bufferIterator->readVector3();
		transform->scaleInverseSq = sharedContext->bufferIterator->readVector3();
		transform->scaleInverseDouble = sharedContext->bufferIterator->readVector3();
	}
	else if (transform->mapType == translationMap) {
		transform->translation = sharedContext->bufferIterator->readVector3();
	}
	else if (transform->mapType == unitaryMap) {
		// TODO https://github.com/AcademySoftwareFoundation/openvdb/blob/master/openvdb/openvdb/math/Maps.h#L1809
		std::cout << "[WARN] Unsupported: GridDescriptor::UnitaryMap" << std::endl;
	}
	else if (transform->mapType == nonlinearFrustumMap) {
		// TODO https://github.com/AcademySoftwareFoundation/openvdb/blob/master/openvdb/openvdb/math/Maps.h#L2418
		std::cout << "[WARN] Unsupported: GridDescriptor::NonlinearFrustumMap" << std::endl;
	}
	else {
		// Support for any magical map types from https://github.com/AcademySoftwareFoundation/openvdb/blob/master/openvdb/openvdb/math/Maps.h#L538 to be added
		std::cout << "[WARN] Unsupported: GridDescriptor::Matrix4x4" << std::endl;
		// 4x4 transformation matrix
	}

	// Trigger cache // Do I need to do this?????
	//this.applyTransformMap(new Vector3());
}

void Grid::readTopology() // in implosion vdb here we are in offset 43885
{
	sharedContext->useHalf = saveAsHalfFloat;
	sharedContext->valueType = getGridValueType();

	unsigned int topologyBufferCount = sharedContext->bufferIterator->readBytes(uint32Size); // 4 bytes
	if (topologyBufferCount != 1) {
		// NOTE https://github.com/AcademySoftwareFoundation/openvdb/blob/master/openvdb/openvdb/tree/Tree.h#L1120
		std::cout << "[WARN] Unsupported: Multi-buffer trees" << std::endl;
		return;
	}

	root = RootNode();
	root.sharedContext = sharedContext;
	root.read();
	leavesCount = root.leavesCount;

	std::cout << "Block buffer " << blockBufferPosition << " " << sharedContext->bufferIterator->offset << std::endl; // assert
}

void Grid::readBuffers()
{
	// TODO
}

float Grid::getValue(glm::vec3 pos)
{
	glm::ivec3 roundPos = round(pos);

	return this->accessor->getValue(roundPos);
}

std::string Grid::getMetadata(std::string s)
{
	for (int i = 0; i < this->metadata.size(); i++) {
		if (this->metadata[i].name == s) {
			return this->metadata[i].value;
		}
	}

	return std::string("undefined");
}

Bbox Grid::getPreciseWorldBbox()
{
	std::string minString = getMetadata("file_bbox_min");
	std::string maxString = getMetadata("file_bbox_max");
	
	std::stringstream minStream(minString);
	std::stringstream maxStream(maxString);
	std::string segment;

	glm::vec3 minbbox;
	std::getline(minStream, segment, ',');
	minbbox.x = std::stof(segment);
	std::getline(minStream, segment, ',');
	minbbox.y = std::stof(segment);
	std::getline(minStream, segment, ',');
	minbbox.z = std::stof(segment);

	glm::vec3 maxbbox;
	std::getline(maxStream, segment, ',');
	maxbbox.x = std::stof(segment);
	std::getline(maxStream, segment, ',');
	maxbbox.y = std::stof(segment);
	std::getline(maxStream, segment, ',');
	maxbbox.z = std::stof(segment);

	this->transform->applyTransformMap(minbbox);
	this->transform->applyTransformMap(maxbbox);

	return Bbox(minbbox, maxbbox);
}

Precision Grid::getGridValueType()
{
	for (unsigned int i = 0; i < metadata.size(); i++) {
		/*if (metadata[i].name == "is_saved_as_half_float") {
			return getPrecisionIdx("half");
		}*/
		if (metadata[i].name == "value_type") {
			return getPrecisionIdx(metadata[i].value);
		}
	}

	std::cout << "[WARN] Precision type not found in the metadata, setting it as float" << std::endl;
	return floatType; // undefined pr floatType or "float"
}

VDB_Transform::VDB_Transform() {
	this->translation = glm::vec3(0.f);
	this->scale = glm::vec3(0.f);
	this->voxelSize = glm::vec3(0.f);
	this->scaleInverse = glm::vec3(0.f);
	this->scaleInverseSq = glm::vec3(0.f);
	this->scaleInverseDouble = glm::vec3(0.f);
}