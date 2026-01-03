#pragma once

#include <macro/macro.h>

namespace highp::network
{
class ISession
{
public:
	virtual ~ISession() = default;
protected:
	ISession() = default;
	ISession(const ISession&) = default;
	ISession& operator=(const ISession&) = default;
	ISession(ISession&&) = default;
};
}