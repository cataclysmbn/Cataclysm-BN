# JSON 스타일 가이드

이 문서는 Cataclysm: Bright Nights JSON 파일의 스타일 가이드라인을 설명합니다.

## 형식

### 들여쓰기

- **2칸 공백** 사용 (탭 아님)
- 일관된 들여쓰기 레벨 유지

```json
{
  "type": "item",
  "id": "my_item",
  "name": {
    "str": "my item"
  }
}
```

### 중괄호 및 대괄호

- 여는 중괄호/대괄호는 같은 줄에
- 닫는 중괄호/대괄호는 새 줄에

```json
{
  "array": [
    "item1",
    "item2"
  ],
  "object": {
    "key": "value"
  }
}
```

## 명명 규칙

### ID

- **소문자** 사용
- 단어는 **밑줄**로 구분
- 설명적이고 고유해야 함

```json
{
  "id": "my_cool_item", // ✓ 좋음
  "id": "MyCoolItem", // ✗ 나쁨
  "id": "item1" // ✗ 설명적이지 않음
}
```

### 필드

- 소문자 사용
- 밑줄로 구분된 스네이크 케이스
- 기존 필드명과 일관성 유지

```json
{
  "weight": "1 kg",
  "volume": "250 ml",
  "longest_side": "30 cm"
}
```

## 순서

필드는 다음 순서로 배치:

1. `type`
2. `id`
3. `name` / `description`
4. 기타 필수 필드
5. 선택적 필드 (알파벳순)

```json
{
  "type": "TOOL",
  "id": "screwdriver",
  "name": "screwdriver",
  "description": "A tool for screws.",
  "weight": "200 g",
  "volume": "100 ml",
  "price": "5 USD",
  "material": ["steel"],
  "symbol": ";",
  "color": "light_gray"
}
```

## 문자열

### 번역 가능한 문자열

- 사용자에게 표시되는 텍스트는 번역 가능해야 함
- 간단한 문자열에는 문자열 사용
- 복수형이 필요하면 객체 사용

```json
{
  "name": "apple", // 단수만
  "name": { // 단수/복수
    "str": "apple",
    "str_pl": "apples"
  }
}
```

### 설명

- 명확하고 간결하게 작성
- 완전한 문장 사용
- 마침표로 끝내기

```json
{
  "description": "A fresh red apple.  Delicious and nutritious."
}
```

## 배열

### 단일 항목

- 대괄호 사용

```json
{
  "material": ["steel"]
}
```

### 여러 항목

- 각 항목을 새 줄에
- 마지막 항목 뒤에 쉼표 없음

```json
{
  "material": [
    "steel",
    "plastic",
    "wood"
  ]
}
```

## 주석

JSON은 공식적으로 주석을 지원하지 않지만, 게임은 `//` 주석을 허용합니다:

```json
{
  "type": "item",
  "id": "my_item",
  // 이것은 주석입니다
  "name": "my item"
}
```

**참고**: 주석은 다음과 같은 경우에만 사용:

- 복잡한 로직 설명
- 임시 노트 (나중에 제거)
- 디버깅 정보

## 단위

명시적 단위 사용:

```json
{
  "weight": "1 kg", // ✓ 명시적
  "weight": 1000, // ✗ 모호함
  "volume": "250 ml", // ✓ 명시적
  "volume": "0.25 L" // ✓ 또한 유효함
}
```

## 검증

변경 전에 항상 JSON 유효성 검사:

```bash
# JSON 파일 검증
./build-scripts/lint-json.sh

# JSON 형식 지정
make style-json
```

## 모범 사례

1. **일관성**: 기존 파일의 스타일 따르기
2. **가독성**: 코드를 명확하고 이해하기 쉽게 작성
3. **검증**: 커밋 전에 항상 유효성 검사
4. **문서화**: 복잡한 정의에 주석 추가
5. **테스트**: 게임 내에서 변경사항 테스트

## 관련 문서

- [파일 설명](file_description.md)
- [로딩 순서](loading_order.md)
