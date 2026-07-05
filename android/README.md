# Android command line builds

Use the Gradle wrapper for builds and Android CLI for agent/device helpers.

- Android CLI: <https://developer.android.com/tools/agents/android-cli>
- Android command line builds: <https://developer.android.com/build/building-cmdline>

```sh
cd android
./gradlew -Pj=$(nproc) -Pabi_arm_32=false -Pabi_x86_32=false -Pabi_x86_64=false assembleExperimentalDebug
android describe --project_dir .
android run --apks app/build/outputs/apk/experimental/debug/*.apk
```

`android run` deploys an existing APK; it does not replace Gradle builds.
