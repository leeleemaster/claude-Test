#include "SymbolTable.h"

namespace {
std::string MakeKey(const std::string& name, const std::string& scope) {
    return scope + "::" + name;
}
}

void SymbolTable::RecordNew(const std::string& name, const std::string& type, bool isArray, int declLine, const std::string& scope) {
    SymbolEntry entry;
    entry.name = name;
    entry.type = type;
    entry.isArray = isArray;
    entry.declLine = declLine;
    entry.deleteLine = -1;
    entry.scope = scope;
    m_entries[MakeKey(name, scope)] = entry;
}

void SymbolTable::MarkDelete(const std::string& name, const std::string& scope, int deleteLine) {
    std::map<std::string, SymbolEntry>::iterator it = m_entries.find(MakeKey(name, scope));
    if (it != m_entries.end()) {
        it->second.deleteLine = deleteLine;
    }
}

const SymbolEntry* SymbolTable::Find(const std::string& name, const std::string& scope) const {
    std::map<std::string, SymbolEntry>::const_iterator it = m_entries.find(MakeKey(name, scope));
    if (it == m_entries.end()) {
        return 0;
    }
    return &it->second;
}
