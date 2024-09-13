#pragma once

#include <vector>
#include <glm/vec3.hpp>

#include "node.h"
#include "grid.h"

namespace easyVDB
{

	class Accessor {
	public:
		Grid* grid = nullptr;
		std::vector<InternalNode*> stack;

		Accessor(Grid* grid) {
			this->grid = grid;
		}

		float probeValues(glm::ivec3 pos) {
			if (!this->stack.empty()) {
				InternalNode* cachedNode = this->stack.back();
				this->stack.pop_back();

				if (cachedNode->contains(pos)) {
					return cachedNode->getValue(pos, this);
				}
			}
			else
			{
				return this->grid->root.getValue(pos, this);
			}

			return this->probeValues(pos);
		};
		float getValue(glm::ivec3 pos) {
			return this->probeValues(pos);
		};
		void cache(InternalNode* node) {
			this->stack.push_back(node);
		}
	};

}