# Coding Agent Bootstrap

## Overview

Familiarize yourself with this project by reading:

- README.md
- CMakeLists.txt
- src/*               Source tree
- include/*           Public API


## Projects

Ongoing projects can be found in agent/projects/*.md

If any projects are found, present them to the operator after bootstrap is complete.

## Issues / Recommendations

If you notice any issues or have any recommendations about the codebase, please bring them up to the operator.

## Building with kbuild.py

- Always use kbuild.py for building. Do not use cmake.
- Always call kbuild.py from the repo root: `./kbuild.py`
- Make a test run of `./kbuild.py --help`.
- With no arguments, `./kbuild.py` builds into build/latest/
- Use `./kbuild.py --build-demos` for building demos into demo/.../build/latest

## Testing

- Most testing -- aside from very specific unit testing -- should be done using the demo/executable binary. Add scripting test cases to make specific calls to demo binaries as necessary in the tests/ directory.

## Rules and Regulations

- **Always plan first**
- **Discuss, then code**
- Do not jump to straight to coding when given a prompt. Always consult with the operator and come up with a structured plan before any code changes. Explain to the operator what changes you will be making and where.
- If you have not been given an explicit instruction to being coding do not start coding.
- Do not interpret questions begin with words like "Can I/we/you...?" or "Is it possible to...?" as instructions to begin coding. Answer the question, then ask if you should begin coding.
- Always provide a summary of changes when you are finished.

