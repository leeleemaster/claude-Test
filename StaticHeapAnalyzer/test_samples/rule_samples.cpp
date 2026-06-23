// 설계서(heap_audit) 탐지 규칙 R1~R9 검증용 샘플.
// 각 함수는 설계 10장 테스트 계획(TC-1~TC-8)에 대응한다.

#include <cstring>
#include <cstdlib>

// TC-1: R1 overrun
void tc1_overrun(char* src) {
    char buf[16];
    strcpy(buf, src);            // R1
}

// TC-2: R2 new[] -> delete
void tc2_new_delete() {
    int* p = new int[10];
    delete p;                    // R2
}

// TC-2b: R2 new -> delete[]
void tc2b_new_delete_arr() {
    int* q = new int;
    delete[] q;                  // R2
}

// TC-3: R3 malloc -> delete, new -> free
void tc3_mix() {
    char* a = (char*)malloc(10);
    delete a;                    // R3
    int* b = new int;
    free(b);                     // R3
}

// TC-5: R4 sizeof(ptr)
void tc4_sizeof(char* p) {
    memset(p, 0, sizeof(p));     // R4
}

// R5 memset 길이 0
void tc5_memset_zero(char* p) {
    memset(p, 1, 0);             // R5
}

// TC-6: R6 off-by-one
void tc6_offbyone() {
    int arr[10];
    for (int i = 0; i <= 10; ++i) {
        arr[i] = i;              // R6
    }
}

// TC-7: R7 GetBuffer 단독(ReleaseBuffer 없음)
void tc7_getbuffer(CString& s) {
    char* p = s.GetBuffer(10);   // R7
    p[0] = 'x';
}

// TC-4: R9 double-free
void tc9_double_free() {
    int* p = new int;
    delete p;
    delete p;                    // R9
}

// ---- TC-8 (음성: 무경보여야 함) ----

void tc8_ok_arr() {
    int* p = new int[10];
    delete[] p;                  // OK
}

void tc8_ok_free() {
    char* p = (char*)malloc(10);
    free(p);                     // OK
}

void tc8_getbuffer_ok(CString& s) {
    char* p = s.GetBuffer(10);
    p[0] = 'x';
    s.ReleaseBuffer();           // OK (짝 존재)
}

void tc8_reassigned() {
    int* p = new int;
    delete p;
    p = new int;                 // 재할당 -> 이중 해제 아님
    delete p;                    // OK
}

// 주석/문자열 내부 토큰은 오탐되지 않아야 한다.
// strcpy(buf, src); delete p;            <- 주석: 무경보
const char* g_s = "delete p; strcpy(x,y);"; // 문자열: 무경보
