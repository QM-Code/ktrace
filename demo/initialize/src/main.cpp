#include "ktrace/trace.hpp"

int main() {
    ktrace::RegisterChannel("demo", ktrace::ResolveColor("Plum1"));
    ktrace::EnableChannel("demo.demo");
    KTRACE("demo", "ktrace demo trace line");
    return 0;
}
