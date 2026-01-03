#pragma once

// interface

// pure virtual function macro
#define PURE = 0

// constructor / destructor declaration fork interface
#define CDTOR_INTERFACE(className) \
	virtual ~className() = default; \
protected: \
	className() = default; \
	className(const className&) = default; \
	className& operator=(const className&) = default; \
	className(className&&) = default;

