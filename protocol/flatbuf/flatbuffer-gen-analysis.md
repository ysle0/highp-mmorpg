# flatbuffer-gen-analysis

플랫퍼버가 뭔지도 모르고 패킷 Serializer 로 쓸 수 는 없어서 공식 문서를 읽어보고
너무 내용이 요약되어있어 생성된 코드를 분석해보기로 하였습니다.

## 루트패킷

모든 패킷의 부모 패킷. (gen/packet_generated.h)

```c++
namespace highp::protocol {
struct Packet : private flatbuffers::Table {
public:
  1. MessageType type() const: 바이트 단위로 type 시작점부터 u16 을 가져옴.
  2. Payload payload_type() const: 바이트 단위로 payload 시작점부터 u8 을 가져옴.
  3. const void* payload() const: 페이로드를 immutable void*로 가져옴. 이후 직접 하위 패킷타입으로 캐스팅하여 사용. union 이므로 2. 로 분기처리함.
  4. template <typename T> const T* payload_as() const: 3. 을 템플릿을 활용하여 선캐스팅. badcastexception 주의.
  5. const messages::UNDERLYING_PACKET_TYPE* payload_as_messages_UNDERLYING_PACKET_TYPE() const:
     4. 를 미리 생성해둔것.
  6. 
  
  
};
} // namespace highp::protocol
```
