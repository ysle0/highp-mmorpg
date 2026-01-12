#pragma once

namespace highp::fn {

// Primary template: Result<TData, E>
template <typename TData, typename E>
class [[nodiscard]] Result final {
private:
	Result() : _data{}, _err{}, _hasError(false) {}
	explicit Result(TData data) : _data(std::move(data)), _err{}, _hasError(false) {}
	Result(E err, bool) : _data{}, _err(err), _hasError(true) {}

public:
	static Result Ok() {
		return Result();
	}

	static Result Ok(TData data) {
		return Result(std::move(data));
	}

	static Result Err(E err) {
		return Result(err, true);
	}

	bool IsOk() const { return !_hasError; }
	bool HasErr() const { return _hasError; }
	TData Data() const { return _data; }
	E Err() const { return _err; }

private:
	TData _data;
	E _err;
	bool _hasError;
};

// Specialization: Result<void, E> - 데이터 없이 성공/실패만
template <typename E>
class [[nodiscard]] Result<void, E> final {
private:
	Result() : _err{}, _hasError(false) {}
	Result(E err, bool) : _err(err), _hasError(true) {}

public:
	static Result Ok() {
		return Result();
	}

	static Result Err(E err) {
		return Result(err, true);
	}

	bool IsOk() const { return !_hasError; }
	bool HasErr() const { return _hasError; }
	E Err() const { return _err; }

private:
	E _err;
	bool _hasError;
};

} // namespace highp::fn
