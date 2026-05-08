## 작업 요약

-

## 변경 내용

-

## 목표 테스트

- [ ] `make tests/vm/<test-name>.result`

## 테스트 결과

| 테스트 | 결과 |
| --- | --- |
| `<test-name>` | PASS |

## 리뷰 포인트

-

## 위험 요소

-

## 체크리스트

- [ ] build 성공
- [ ] 목표 테스트 통과
- [ ] 관련 회귀 테스트 확인
- [ ] debug print 제거
- [ ] busy waiting 없음
- [ ] interrupt off 구간 최소화
- [ ] 테스트 이름 기반 하드코딩 없음
- [ ] 복잡한 invariant는 주석 또는 문서에 설명
