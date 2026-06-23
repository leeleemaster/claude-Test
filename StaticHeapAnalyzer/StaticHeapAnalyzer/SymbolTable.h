#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <map>
#include <string>

struct SymbolEntry {
    std::string name;
    std::string type;
    bool isArray;
    int declLine;
    int deleteLine;
    std::string scope;
};

class SymbolTable {
public:
    void RecordNew(const std::string& name, const std::string& type, bool isArray, int declLine, const std::string& scope);
    void MarkDelete(const std::string& name, const std::string& scope, int deleteLine);
    const SymbolEntry* Find(const std::string& name, const std::string& scope) const;

private:
    std::map<std::string, SymbolEntry> m_entries;
};

#endif
