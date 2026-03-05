#include <ktrace.hpp>

#include <iostream>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    ktrace::Initialize();
    ktrace::RegisterChannel("bootstrap", ktrace::Color("BrightGreen"));
    ktrace::EnableChannel(".bootstrap");
    KTRACE("bootstrap", "ktrace bootstrap compile/link check");

    std::cout << "Bootstrap succeeded.\n";
    return 0;
}
