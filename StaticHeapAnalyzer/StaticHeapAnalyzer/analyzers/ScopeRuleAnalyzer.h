#ifndef SCOPE_RULE_ANALYZER_H
#define SCOPE_RULE_ANALYZER_H

#include "../RuleEngine.h"

// 스코프 추적 규칙 엔진 (설계 4.3)
//  R2 new-delete-mismatch : new[]<->delete, new<->delete[] 짝 불일치
//  R3 new-free-mix        : new->free(), malloc->delete 혼용
//  R9 double-free         : 재할당 없이 동일 변수 2회 해제
//
// 중괄호 깊이로 스코프를 추적하며 변수의 할당 종류(new/new[]/malloc/미상)와
// 해제 여부(deleted)를 기억한다. 일반 대입은 deleted 상태를 해제하여
// 루프 노드 해제 패턴(delete p; p = next; delete p;)의 오탐을 막는다.
class ScopeRuleAnalyzer {
public:
    static void Analyze(const std::string& file, const std::vector<TokenLine>& tokens, std::vector<Finding>& findings);
};

#endif
