# Demo: initialize

Minimal trace demo that emits one traced line.

## Build SDK

```bash
./abuild.py -a <name> -d build/test/
```

## Build demo

```bash
./abuild.py -a <name> -d demo/initialize/build/test/ --ktrace-sdk build/test/sdk/
```

## Run

```bash
./demo/initialize/build/test/test
```
