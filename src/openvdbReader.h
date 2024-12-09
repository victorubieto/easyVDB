#pragma once

#include <vector>
#include <string>

//#include "grid.h"
//#include "bbox.h"
#include "bufferIterator.h"

namespace easyVDB
{

	struct FileVersion
	{
		unsigned int version;
		unsigned int minor;
		unsigned int major;
	};

	class Grid;

	class OpenVDBReader {
	public:
		SharedContext sharedContext;

		std::vector<uint8_t> rawBuffer;
		BufferIterator* bufferIterator;

		// header data
		FileVersion fileVersion;
		bool hasGridOffsets;
		Compression compression;
		std::vector<Metadata> metadata; // list of metadata per grid

		// grid data
		Grid* grids;
		unsigned int gridsSize;

		OpenVDBReader();
		~OpenVDBReader();

		void deinit();
		bool openFile(std::string file_path);

		bool read(std::string file_path);
		bool isValidFile();
		bool readFileVersion();
		bool readHeader();
		bool readGrids();
	};

}