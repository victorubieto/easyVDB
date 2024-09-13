#pragma once

#include <glm/vec3.hpp>

namespace easyVDB
{

	class Bbox {
	public:
		glm::vec3 min;
		glm::vec3 max;

		Bbox() {
			this->min = glm::vec3(0.f);
			this->max = glm::vec3(0.f);
		};
		Bbox(glm::vec3 min, glm::vec3 max) {
			this->min = min;
			this->max = max;
		};

		void set(glm::vec3 min, glm::vec3 max) {
			this->min = min;
			this->max = max;
		}

		glm::vec3 getCenter() {
			return (min + max) * 0.5f;
		}
		glm::vec3 getSize() {
			return max - min;
		}
	};

}