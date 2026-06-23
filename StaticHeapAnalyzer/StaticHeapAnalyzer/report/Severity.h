#ifndef SEVERITY_H
#define SEVERITY_H

enum class Severity {
    CRITICAL,
    HIGH,
    MEDIUM,
    LOW
};

inline const char* SeverityToString(Severity severity) {
    switch (severity) {
    case Severity::CRITICAL: return "CRITICAL";
    case Severity::HIGH: return "HIGH";
    case Severity::MEDIUM: return "MEDIUM";
    default: return "LOW";
    }
}

inline const char* SeverityToHtmlColor(Severity severity) {
    switch (severity) {
    case Severity::CRITICAL: return "#ff4d4f";
    case Severity::HIGH: return "#fa8c16";
    case Severity::MEDIUM: return "#fadb14";
    default: return "#91d5ff";
    }
}

#endif
