# Executable Demo

Build executable against KTraceSDK + AlphaSDK + BetaSDK + DeltaSDK:

```bash
./abuild.py -a <name> -d build/test/ \
  --ktrace-sdk ../../build/test/sdk/ \
  --alpha-sdk ../libraries/alpha/build/test/sdk/ \
  --beta-sdk ../libraries/beta/build/test/sdk/ \
  --delta-sdk ../libraries/delta/build/test/sdk/
```

Run:

```bash
./build/test/test
```
