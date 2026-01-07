#pragma once

namespace highp::func
{
template <typename T>
class [[nodiscard]] Result
{
	T _err;
	bool _hasError;

public:
	static Result<T> Ok;	
	static Result<T> Err(T err) {
		return Result<T>(err);
	}

	bool IsOk() const {
		return !_hasError;
	}

	bool IsErr() const {
		return _hasError;
	}

	T Err() const {
		return _err;
	}

private:
	Result() : _err{}, _hasError(false) { }
	explicit Result(T err) : _err(err), _hasError(true) { }
};

template <typename T>
Result<T> Result<T>::Ok = Result<T>();

}

