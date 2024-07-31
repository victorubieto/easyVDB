#include "openvdbReader.h"
#include <iostream>

#include "versions.h"

OpenVDBReader::OpenVDBReader()
{
	this->fileVersion.minor = 0u;
	this->fileVersion.major = 0u;

	sharedContext.version = &fileVersion.version;

	sharedContext.useHalf = false;
	sharedContext.useDelayedLoadMeta = false;
}

void OpenVDBReader::read(std::vector<uint8_t> source)
{
	const auto start = std::chrono::high_resolution_clock::now();

	// TODO: prepare compression modules

	bufferIterator = new BufferIterator(source);
	sharedContext.bufferIterator = bufferIterator;

	if (isValidFile()) {					// 8 bytes
		readFileVersion();					// 3 * 4 bytes = 12 bytes 
		readHeader();						// 1 + 36 + 4 = 41 bytes (metadata is especified per grid, so here is 0)
		readGrids();						// 4 
	}
	else {
		std::cout << "[ERROR] Not a VDB file." << std::endl;
	}

	const auto end = std::chrono::high_resolution_clock::now();
	std::cout << "[INFO] VDB read successfully in " << std::chrono::duration_cast<std::chrono::milliseconds> (end - start).count() << "ms" << std::endl;
}

bool OpenVDBReader::isValidFile()
{
	unsigned int magic = bufferIterator->readBytes(uint64Size);
	return 0x56444220 == magic;
}

bool OpenVDBReader::readFileVersion()
{
	fileVersion.version = bufferIterator->readBytes(uint32Size);

	if (fileVersion.version < OPENVDB_MIN_SUPPORTED_VERSION) {
		return false;
	}

	// Stored from 211 onward, our minimum supported version is 213
	fileVersion.major = bufferIterator->readBytes(uint32Size);
	fileVersion.minor = bufferIterator->readBytes(uint32Size);
	
	return true;
}

bool OpenVDBReader::readHeader()
{
	// Stored from 212 onward, our minimum supported version is 213
	hasGridOffsets = bufferIterator->readBytes(charSize);

	if (fileVersion.version >= OPENVDB_FILE_VERSION_SELECTIVE_COMPRESSION && fileVersion.version < OPENVDB_FILE_VERSION_NODE_MASK_COMPRESSION) {
		unsigned int compress = bufferIterator->readBytes(charSize);
		compression.none = compress & 0x0;
		compression.zlib = compress & 0x1;
		compression.activeMask = compress & 0x2;
		compression.blosc = compress & 0x4;
	} else {
		// From version 222 on, compression information is stored per grid
		compression.none = false;
		compression.zlib = false;
		compression.activeMask = true;
		compression.blosc = false;
	}
	sharedContext.compression = compression;
	
	// UUID is stored as fixed-length ASCII string
	// The extra 4 bytes are for the hyphens (-)
	std::string uuid = "";
	for (int i = 0; i < 36; i++) {
		uuid += bufferIterator->readBytes(1);
	}

	unsigned int metadataSize = bufferIterator->readBytes(uint32Size);
	for (int i = 0; i < metadataSize; i++) {
		std::string name = bufferIterator->readString();
		std::string type = bufferIterator->readString();
		std::string value = bufferIterator->readString(type);

		metadata.push_back(Metadata(name, type, value));
	}

	return true;
}

bool OpenVDBReader::readGrids()
{
	if (!hasGridOffsets) {
		// TODO Handle case without grid offsets
		// File.cc:364
		std::cout << "[WARN] Unsupported: VDB without grid offsets" << std::endl;
	}
	else {
		unsigned int gridCount = bufferIterator->readBytes(uint32Size);
		gridsSize = gridCount;
		grids = new Grid[gridCount];

		for (unsigned int i = 0u; i < gridCount; i++) {
			grids[i].sharedContext = &sharedContext;
			grids[i].read();
			std::cout << "Grids read: " << i+1 << std::endl;
		}
	}

	return true;
}