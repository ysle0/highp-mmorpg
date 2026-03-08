# 스마트 포인터 소유권 정리

이 문서는 `std::unique_ptr`, `std::shared_ptr`를 함수 파라미터로 넘기고, 받는 쪽에서 저장하거나 다시 다른 함수로 전달할 때 소유권이 어떻게 움직이는지 헷갈리지 않도록 정리한 문서다.

## 1. 먼저 머릿속 모델을 분리해야 한다

헷갈림은 보통 아래 3가지를 섞어서 생각할 때 생긴다.

- 객체의 생명주기
- 포인터 핸들의 생명주기
- 소유권이 배타적인지, 공유되는지

`std::unique_ptr<T>`는 "소유자 핸들이 정확히 1개"라는 뜻이다.

`std::shared_ptr<T>`는 "소유자 핸들이 여러 개 있을 수 있다"는 뜻이다.

`T*`, `T&`는 "소유권과 무관하게 객체를 참조만 한다"는 뜻이다.

## 2. 파라미터 타입이 의미하는 것

### `unique_ptr`

- `std::unique_ptr<T> p`
  - sink 파라미터
  - callee가 소유권을 가져간다
  - caller는 `std::move(ptr)`로 넘겨야 한다

- `const std::unique_ptr<T>& p`
  - 관찰만 한다
  - 소유권 이전 없음
  - callee가 뺏어갈 수 없다

- `std::unique_ptr<T>& p`
  - caller의 owning pointer 자체를 바꾸거나 reset할 수 있다
  - 의도가 매우 분명할 때만 쓴다

- `T*` 또는 `T&`
  - 객체만 사용한다
  - 소유권은 다루지 않는다

### `shared_ptr`

- `std::shared_ptr<T> p`
  - callee도 owner가 된다
  - caller는 복사로 넘길 수도 있고 move로 넘길 수도 있다

- `const std::shared_ptr<T>& p`
  - 기존 shared owner를 관찰만 한다
  - callee가 저장하지 않으면 owner 수는 늘지 않는다

- `std::shared_ptr<T>& p`
  - caller의 shared_ptr 자체를 바꿀 수 있다
  - 흔한 형태는 아니다

- `T*` 또는 `T&`
  - 객체만 사용한다
  - 소유권은 다루지 않는다

## 3. 가장 간단한 규칙

- 소유권을 멤버에 저장할 것이라면: 스마트 포인터를 값으로 받고 멤버로 `std::move`
- 객체만 쓸 것이라면: `T&` 또는 `T*`
- 스마트 포인터 자체를 읽기만 할 것이라면: `const &`

## 4. 왜 "값으로 받고 멤버로 move"가 일반적인가

예시:

```cpp
class Foo {
    std::shared_ptr<A> _a;

public:
    explicit Foo(std::shared_ptr<A> a)
        : _a(std::move(a)) {
    }
};
```

이 패턴의 의미는 다음과 같다.

- caller가 복사로 넘길지, move로 넘길지 결정한다
- ctor는 멤버 저장을 효율적으로 처리한다

호출 예시:

```cpp
auto p = std::make_shared<A>();   // use_count = 1
Foo f(p);                         // 파라미터로 복사, 그 다음 멤버로 move
                                  // caller의 p는 그대로 유효
                                  // 최종 owner: p, f._a

Foo g(std::move(p));              // 파라미터로 move, 그 다음 멤버로 move
                                  // caller의 p는 null
```

핵심은 ctor 내부의 `std::move(a)`가 caller 원본을 직접 움직이는 것이 아니라, "파라미터로 받은 로컬 복사본 또는 로컬 이동본"을 멤버로 옮긴다는 점이다.

## 5. 지금 가장 헷갈리는 지점: 받아서 다시 전달하기

예시:

```cpp
void C(std::shared_ptr<A> p) {
    B(std::move(p));
}
```

### 경우 1: caller가 복사로 넘김

```cpp
std::shared_ptr<A> x = std::make_shared<A>();
C(x);
```

이 경우:

- `x`가 `C`의 파라미터로 복사된다
- `C` 안에서는 그 로컬 복사본이 `B`로 move된다
- caller의 `x`는 그대로 살아 있다

즉, callee가 `std::move(p)`를 했더라도 caller가 복사로 넘겼다면 caller의 원본은 영향받지 않는다.

### 경우 2: caller가 move로 넘김

```cpp
std::shared_ptr<A> x = std::make_shared<A>();
C(std::move(x));
```

이 경우:

- caller가 자기 핸들을 `C`에 넘긴다
- `C`는 그 핸들을 다시 `B`로 넘긴다
- caller의 `x`는 null이 된다

즉, 중요한 질문은 "callee가 move했는가?"가 아니라 "callee의 파라미터가 caller로부터 복사된 것인가, move된 것인가?"다.

## 6. `unique_ptr` 버전은 더 단순하다

```cpp
void C(std::unique_ptr<A> p) {
    B(std::move(p));
}
```

caller는 반드시 이렇게 호출해야 한다.

```cpp
auto x = std::make_unique<A>();
C(std::move(x));
```

호출 후:

- `x == nullptr`
- 소유권은 `C`로 갔다가 다시 `B`로 이동한다

`unique_ptr`는 복사가 불가능하므로 경우의 수가 적다.

## 7. 왜 값 파라미터에서 다시 `move`하나

### `unique_ptr`

복사가 불가능하므로 `std::move`가 필요하다.

### `shared_ptr`

복사는 가능하지만 refcount 증가/감소가 추가로 발생한다.

예를 들어:

```cpp
class Foo {
    std::shared_ptr<A> _a;

public:
    explicit Foo(std::shared_ptr<A> a)
        : _a(std::move(a)) {
    }
};
```

여기서 move를 쓰면 "파라미터에서 멤버로 옮길 때" 불필요한 refcount 증감을 줄일 수 있다.

즉:

- caller -> 파라미터 단계에서는 caller가 복사로 넘겼다면 refcount가 1 증가할 수 있다
- 파라미터 -> 멤버 단계에서는 move로 넘겨서 추가 증가를 피한다

## 8. 스마트 포인터를 항상 파라미터로 받아야 하는 것은 아니다

함수가 소유권을 저장하지 않고, 그냥 객체의 메서드만 호출한다면 보통 스마트 포인터를 받을 이유가 없다.

예를 들어:

```cpp
void Render(const Mesh& mesh);
void Update(Player& player);
void Send(PacketHandler* handler);
```

이런 형태가 오히려 더 명확하다.

규칙은 간단하다.

- 소유권이 필요하면 스마트 포인터
- 객체만 쓰면 참조 또는 raw pointer

## 9. 실전 API 규칙

다음 규칙만 지켜도 대부분의 혼란이 사라진다.

- "내가 owner로 저장한다" -> `std::shared_ptr<T>` 또는 `std::unique_ptr<T>`를 값으로 받는다
- "나는 객체만 쓴다" -> `T&` 또는 `T*`
- "비동기 작업, 생명주기 연장이 필요하다" -> `std::shared_ptr<T>`
- "owner는 반드시 1명이어야 한다" -> `std::unique_ptr<T>`
- "caller가 자기 핸들을 넘기게 만들고 싶다" -> caller 쪽에서 `std::move(...)`

## 10. 현재 Logger 코드에 적용하면

현재 [lib/inc/logger/Logger.hpp](../lib/inc/logger/Logger.hpp)에서는 다음 패턴을 쓰고 있다.

```cpp
Logger(std::shared_ptr<ILogger> impl, std::string prefix = {})
    : _impl(std::move(impl))
      , _prefix(std::move(prefix)) {
}
```

그리고 child logger를 만들 때는 이런 식이다.

```cpp
return std::make_shared<Logger>(_impl, CombinePrefix(prefix));
```

여기서 중요한 점:

- `_impl`은 lvalue이므로 ctor 파라미터로 들어갈 때 먼저 복사된다
- ctor 안의 `std::move(impl)`는 그 "로컬 파라미터"를 멤버로 옮기는 것이다
- 부모 logger의 `_impl` 자체를 훔쳐가는 것이 아니다

즉:

- parent logger는 계속 유효하다
- child logger도 같은 `ILogger` sink를 공유한다
- prefix만 다른 wrapper logger가 여러 개 생기는 구조다

이 설계는 "shared backend + per-wrapper context"라는 목적에 맞다.

## 11. 마지막으로 한 줄로 요약

- `unique_ptr`는 소유권 이전
- `shared_ptr`는 소유권 공유 또는 handle 이전
- `std::move`는 "객체 자체를 옮긴다"기보다 "이 핸들을 더 이상 내가 쓰지 않겠다"는 의사표현에 가깝다
- 값으로 받은 스마트 포인터를 멤버로 `move`하는 것은 매우 일반적인 패턴이다
