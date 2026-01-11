#pragma once

// pure virtual function macro
#define PURE = 0

// constructor / destructor declaration for interface
#define INIT_INTERFACE(className) \
public: \
	virtual ~className() = default;

