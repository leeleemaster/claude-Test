#ifndef LINE_RULE_ANALYZER_H
#define LINE_RULE_ANALYZER_H

#include "../RuleEngine.h"

// 라인 단위 규칙 엔진 (설계 4.2)
//  R1 overrun     : 경계 검사 없는 복사/포맷 함수 호출
//  R4 sizeof-ptr  : memset/memcpy(v, ..., sizeof(v)) 동일 식별자
//  R5 memset-zero : memset(v, ..., 0) 길이 인자가 리터럴 0
//  R6 off-by-one  : for(...; i <= N; ...) + 동일 카운터 배열 인덱싱
//  R7 getbuffer   : CString::GetBuffer() 에 짝이 되는 ReleaseBuffer() 부재
class LineRuleAnalyzer {
public:
    static void Analyze(const std::string& file, const std::vector<TokenLine>& tokens, std::vector<Finding>& findings);
};

#endif
