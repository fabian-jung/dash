#pragma once

#include <iostream>

namespace dash {

class Profiler {
public:

	Profiler() :
		count(0)
	{

	}

	~Profiler() {
		std::cout << "Profiler overview for unit " << myid << ": " << std::endl;
		std::cout << "Dereferenced " << count << " times." << std::endl;
	}

	void track() {
		++count;
	}

	void setId(size_t id) {
		myid = id;
	}
private:
	size_t myid;
	size_t count;
};

}
