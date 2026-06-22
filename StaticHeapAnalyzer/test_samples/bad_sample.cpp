#include <cstring>

class Bar {
public:
    void DoSth() {}
};

class Foo {
    Bar* m_bar;
public:
    Foo() {}
    void Use() { m_bar->DoSth(); }
};

struct MyObj {
    int v;
};

static MyObj* g_instance = NULL;

void BuildSingleton() {
    if (!g_instance) {
        g_instance = new MyObj();
    }
}

void HeapMismatch() {
    char* p = new char[256];
    delete p;
}

void BufferOverflow(const char* src) {
    for (int i = 0; i < 3; ++i) {
        char* buf = new char[8];
        strcpy(buf, src);
        delete[] buf;
    }
}

void AllocMisuse() {
    MyObj* p = (MyObj*)::operator new(sizeof(MyObj));
    p->v = 1;
    ::operator delete(p);
}

void StackDelete() {
    MyObj obj;
    MyObj* p = &obj;
    delete p;
}
