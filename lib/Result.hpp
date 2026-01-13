#pragma once

namespace highp::fn {

/// <summary>
/// 성공/실패 결과를 표현하는 타입. std::expected (C++23) 과 동일.
/// [[nodiscard]] 속성으로 결과 무시 방지.
/// </summary>
/// <typeparam name="TData">성공 시 반환할 데이터 타입</typeparam>
/// <typeparam name="E">실패 시 에러 코드 타입</typeparam>
template <typename TData, typename E>
class [[nodiscard]] Result final {
private:
	Result() : _data{}, _err{}, _hasError(false) {}
	explicit Result(TData data) : _data(std::move(data)), _err{}, _hasError(false) {}
	Result(E err, bool) : _data{}, _err(err), _hasError(true) {}

public:
	/// <summary>데이터 없이 성공 결과 생성</summary>
	static Result Ok() {
		return Result();
	}

	/// <summary>데이터와 함께 성공 결과 생성</summary>
	static Result Ok(TData data) {
		return Result(std::move(data));
	}

	/// <summary>에러 코드와 함께 실패 결과 생성</summary>
	static Result Err(E err) {
		return Result(err, true);
	}

	/// <summary>성공 여부 확인</summary>
	bool IsOk() const { return !_hasError; }

	/// <summary>에러 발생 여부 확인</summary>
	bool HasErr() const { return _hasError; }

	/// <summary>성공 시 데이터 반환</summary>
	TData Data() const { return _data; }

	/// <summary>실패 시 에러 코드 반환</summary>
	E Err() const { return _err; }

private:
	/// <summary>성공 시 데이터</summary>
	TData _data;

	/// <summary>실패 시 에러 코드</summary>
	E _err;

	/// <summary>에러 발생 여부 플래그</summary>
	bool _hasError;
};

/// <summary>
/// Result의 void 특수화. 데이터 없이 성공/실패만 표현.
/// </summary>
/// <typeparam name="E">실패 시 에러 코드 타입</typeparam>
template <typename E>
class [[nodiscard]] Result<void, E> final {
private:
	Result() : _err{}, _hasError(false) {}
	Result(E err, bool) : _err(err), _hasError(true) {}

public:
	/// <summary>성공 결과 생성</summary>
	static Result Ok() {
		return Result();
	}

	/// <summary>에러 코드와 함께 실패 결과 생성</summary>
	static Result Err(E err) {
		return Result(err, true);
	}

	/// <summary>성공 여부 확인</summary>
	bool IsOk() const { return !_hasError; }

	/// <summary>에러 발생 여부 확인</summary>
	bool HasErr() const { return _hasError; }

	/// <summary>실패 시 에러 코드 반환</summary>
	E Err() const { return _err; }

private:
	/// <summary>실패 시 에러 코드</summary>
	E _err;

	/// <summary>에러 발생 여부 플래그</summary>
	bool _hasError;
};

} // namespace highp::fn
