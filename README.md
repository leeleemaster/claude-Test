# claude-Test

C++ 정적 힙 메모리 오류 분석기 (**StaticHeapAnalyzer**) 프로젝트 저장소.

VS2008 / MFC / C++ 소스를 **실행 없이** 정적 검사하여 런타임 힙 손상(heap
corruption)을 유발할 수 있는 코드 패턴을 탐지·보고한다. 설계서
`heap_audit` 명세(전처리·규칙 R1~R9·심각도·종료코드)를 C++ 구현으로 반영했다.

## 빌드

- Visual Studio 2017+ (`StaticHeapAnalyzer/StaticHeapAnalyzer.sln`), 또는
- 표준 C++14 컴파일러
  ```
  g++ -std=c++14 -I StaticHeapAnalyzer/StaticHeapAnalyzer \
      StaticHeapAnalyzer/StaticHeapAnalyzer/*.cpp \
      StaticHeapAnalyzer/StaticHeapAnalyzer/analyzers/*.cpp \
      StaticHeapAnalyzer/StaticHeapAnalyzer/report/*.cpp \
      -lstdc++fs -o StaticHeapAnalyzer
  ```

## 실행

```
StaticHeapAnalyzer --src <파일 또는 폴더> [--report console|html|json] [--rules R1,R2,...] [--recursive]
```

| 옵션 | 기본값 | 설명 |
|---|---|---|
| `--src` | (필수) | 검사할 파일 또는 디렉터리 |
| `--report` | `console` | 출력 형식 (console / html / json) |
| `--rules` | 전체 | 활성화할 규칙(쉼표 구분). `R2` 처럼 접두만 주면 해당 계열 전체 |
| `--recursive` | off | 디렉터리 재귀 탐색 |

| 종료 코드 | 의미 |
|---|---|
| 0 | HIGH/CRITICAL 없음 |
| 1 | 인자/입력 오류 |
| 2 | HIGH 이상 발견 (빌드 파이프라인 실패 처리용) |

## 탐지 규칙

전처리 단계에서 주석·문자열·문자상수를 공백으로 치환(줄번호 보존)하여
오탐을 줄인 뒤 규칙을 적용한다.

### 설계 규칙 (R1~R9)

| 규칙 | 이름 | 심각도 | 탐지 대상 |
|---|---|---|---|
| R1 | overrun | HIGH | 경계 검사 없는 복사/포맷 함수(`strcpy`,`strcat`,`sprintf`,`gets`,`wcscpy`,`lstrcpy`,`wsprintf`,`_tcscpy`,`_stprintf`,`strncpy` 등) |
| R2 | new-delete-mismatch | HIGH | `new[]`↔`delete`, `new`↔`delete[]` 짝 불일치 |
| R3 | new-free-mix | HIGH | `new`→`free()`, `malloc`→`delete` 혼용 |
| R4 | sizeof-ptr | MEDIUM | `memset/memcpy(v,…,sizeof(v))` 대상과 sizeof 인자가 동일 식별자 |
| R5 | memset-zero | MEDIUM | `memset(v,…,0)` — 길이 인자가 리터럴 0 |
| R6 | off-by-one | MEDIUM | `for(...; i <= N; ...)` + 동일 카운터로 배열 인덱싱 |
| R7 | getbuffer | MEDIUM | `CString::GetBuffer()` 에 짝이 되는 `ReleaseBuffer()` 부재 |
| R9 | double-free | HIGH | 재할당 없이 동일 변수 2회 해제 |

R2/R3/R9 는 중괄호 스코프 프레임 스택으로 변수의 할당 종류(new/new[]/malloc/미상)와
해제 여부를 추적한다. 중간 일반 대입(`p = next;`)은 해제 상태를 리셋하므로
루프 노드 해제 패턴(`delete p; p = next; delete p;`)을 이중 해제로 오탐하지 않는다.

### 확장 규칙 (설계 비범위, 추가 제공)

| 규칙 | 심각도 | 탐지 대상 |
|---|---|---|
| ALLOC-001 | HIGH | `::operator new` 직접 호출 후 placement new 초기화 없이 사용 |
| CTOR-001 | HIGH | 포인터 멤버를 생성자에서 초기화하지 않고 사용 |
| STACK-001 | CRITICAL | 스택 객체 주소를 가진 포인터에 `delete` |
| THREAD-001 | MEDIUM | static/global 포인터 접근 시 Lock 미사용 |

## 테스트 샘플

- `StaticHeapAnalyzer/test_samples/rule_samples.cpp` — R1~R9 및 음성(무경보) 케이스
- `StaticHeapAnalyzer/test_samples/bad_sample.cpp` — 확장 규칙 포함 혼합 사례

## 런타임 힙 상태 확인 (Release 어태치 환경)

정적 분석 외에 Release 빌드에 디버거를 붙인 상태에서 힙 사용량을 직접 확인할 수 있다.

```cpp
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

void PrintHeap() {
    PROCESS_MEMORY_COUNTERS m = { sizeof(m) };
    GetProcessMemoryInfo(GetCurrentProcess(), &m, sizeof(m));
    char s[128];
    sprintf_s(s, "WS: %Iu MB / Peak: %Iu MB\n",
        m.WorkingSetSize >> 20, m.PeakWorkingSetSize >> 20);
    OutputDebugStringA(s);
}
```

원하는 지점에 `PrintHeap()` 를 호출하면 **DebugView** 또는 **WinDbg Output** 창에 출력된다.
