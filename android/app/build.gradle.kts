import com.android.build.gradle.BaseExtension
import org.gradle.internal.os.OperatingSystem
import java.io.ByteArrayOutputStream
import java.net.URL
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Properties
import java.util.TimeZone

tasks.withType<JavaCompile>().configureEach {
    options.encoding = "UTF-8"
    //options.compilerArgs.add("-Xlint:deprecation")
}

plugins {
    id("com.android.application")
}

val localProperties = Properties()
val localPropertiesFile = rootProject.file("local.properties")
if (localPropertiesFile.exists()) {
    localPropertiesFile.reader(Charsets.UTF_8).use(localProperties::load)
}

var keystorePropertiesFilename = "keystore.properties"
localProperties.getProperty("keystorePropertiesFilename")?.let { keystorePropertiesFilename = it }

val keystoreProperties = Properties()
val keystorePropertiesFile = rootProject.file(keystorePropertiesFilename)
if (keystorePropertiesFile.exists()) {
    keystorePropertiesFile.reader(Charsets.UTF_8).use(keystoreProperties::load)
}

fun gradleProperty(name: String) = localProperties.getProperty(name) ?: project.findProperty(name)?.toString().orEmpty()

val njobs = gradleProperty("j")
val localize = gradleProperty("localize").toBoolean()
val abiArm32 = gradleProperty("abi_arm_32").toBoolean()
val abiArm64 = gradleProperty("abi_arm_64").toBoolean()
val abiX8632 = gradleProperty("abi_x86_32").toBoolean()
val abiX8664 = gradleProperty("abi_x86_64").toBoolean()
val deps = gradleProperty("deps")
val overrideVersion = gradleProperty("override_version")
val versionHeaderPath = gradleProperty("version_header_path")
val overrideCompileSdkVersion = gradleProperty("override_compileSdkVersion").toInt()
val overrideMinSdkVersion = gradleProperty("override_minSdkVersion").toInt()
val overrideTargetSdkVersion = gradleProperty("override_targetSdkVersion").toInt()
val overrideNdkBuildAppPlatform = gradleProperty("override_ndkBuildAppPlatform")
val overrideNdkVersion = gradleProperty("override_ndkVersion")
val versionHeaderFile = rootProject.file(versionHeaderPath)

println("Using [              njobs]: $njobs")
println("Using [           localize]: $localize")
println("Using [               deps]: $deps")
println("Using [   override_version]: $overrideVersion")
println("Using [version_header_path]: $versionHeaderPath")
println("Using [  compileSdkVersion]: $overrideCompileSdkVersion")
println("Using [      minSdkVersion]: $overrideMinSdkVersion")
println("Using [   targetSdkVersion]: $overrideTargetSdkVersion")
println("Using [ndkBuildAppPlatform]: $overrideNdkBuildAppPlatform")
println("Using [         ndkVersion]: $overrideNdkVersion")
println("Using [         abi_arm_32]: $abiArm32")
println("Using [         abi_arm_64]: $abiArm64")
println("Using [         abi_x86_32]: $abiX8632")
println("Using [         abi_x86_64]: $abiX8664")

if (!abiArm32 && !abiArm64 && !abiX8632 && !abiX8664) {
    throw GradleException("All supported ABI properties are set to false!")
}
if (!file(deps).exists()) {
    throw GradleException("Dependencies file does not exist:$deps")
}

if (overrideVersion.isNotEmpty()) {
    if (versionHeaderPath.isEmpty()) {
        throw GradleException("`version_header_path` cannot be empty when `override_version` is not empty")
    } else {
        println("Overriding version number to $overrideVersion using path $versionHeaderPath")
    }
}

val unzipDeps by tasks.registering(Copy::class) {
    println("Using dependencies file: $deps")
    from(zipTree(file(deps)))
    into(file("."))
}

val fetchSqlite by tasks.registering {
    doLast {
        val url = URL("https://www.sqlite.org/2025/sqlite-amalgamation-3480000.zip")
        val file = File("/tmp/sqlite-amalgamation-3480000.zip")
        url.openStream().use { input -> file.outputStream().use(input::copyTo) }
    }
}

val unzipSqlite by tasks.registering(Copy::class) {
    dependsOn(fetchSqlite)
    from(zipTree(file("/tmp/sqlite-amalgamation-3480000.zip"))) {
        includeEmptyDirs = false
        eachFile {
            if (path.startsWith("sqlite-amalgamation-3480000/")) {
                path = path.removePrefix("sqlite-amalgamation-3480000/")
            }
        }
    }
    into("jni/sqlite3")
}

val compileLocalization by tasks.registering(Exec::class) {
    workingDir("../..")
    if (localize) {
        println("Building with localization'")
        val operatingSystem = OperatingSystem.current()
        when {
            operatingSystem.isLinux -> commandLine("lang/compile_mo.sh", "all")
            operatingSystem.isWindows -> commandLine("sh.exe", "-c", "lang/compile_mo.sh all")
            else -> commandLine("echo", "Building without localization")
        }
    } else {
        commandLine("echo", "Building without localization")
    }
}

unzipDeps.configure { dependsOn(compileLocalization) }
tasks.named("preBuild") { dependsOn(unzipDeps, unzipSqlite) }

tasks.configureEach {
    if (name.startsWith("configureCMake") || name.startsWith("externalNativeBuild")) {
        dependsOn(unzipDeps, unzipSqlite)
    }
}

fun writeVersionHeader(version: String) {
    val timestamp = SimpleDateFormat("yyyy-MM-dd-HHmm").apply {
        timeZone = TimeZone.getTimeZone("UTC")
    }.format(Date())
    versionHeaderFile.parentFile.mkdirs()
    versionHeaderFile.writeText("// NOLINT(cata-header-guard)\n#define VERSION \"$version\"\n#define BUILD_TIMESTAMP \"$timestamp\"\n")
}

fun generateVersionHeader() {
    if (overrideVersion.isEmpty()) {
        println("Generating version number to $versionHeaderPath")
        val stdout = ByteArrayOutputStream()
        exec {
            workingDir("../..")
            commandLine("git", "describe", "--tags", "--always", "--match", "[0-9A-Z]*.[0-9A-Z]*")
            standardOutput = stdout
        }
        writeVersionHeader(stdout.toString().trim())
    } else {
        println("Overriding version number to $overrideVersion")
        writeVersionHeader(overrideVersion)
    }
}

generateVersionHeader()

fun BaseExtension.configureCommonAndroid() {
    namespace = "com.cleverraven.cataclysmdda"
    compileSdkVersion(overrideCompileSdkVersion)
    ndkVersion = overrideNdkVersion

    defaultConfig {
        minSdkVersion(overrideMinSdkVersion)
        targetSdkVersion(overrideTargetSdkVersion)
        versionCode = (System.getenv("UPSTREAM_BUILD_NUMBER") ?: "1").toInt()
        versionName = versionHeaderFile.readText().split('"')[1]
        applicationId = "com.cataclysmbnteam.cataclysmbn"
        setProperty("archivesBaseName", "cataclysmbn-$versionName")
        resValue("string", "app_name", "Cataclysm BN")

        externalNativeBuild {
            cmake {
                arguments(
                    "-DANDROID_PLATFORM=$overrideNdkBuildAppPlatform",
                    "-DANDROID_STL=c++_shared",
                    "-DCMAKE_BUILD_PARALLEL_LEVEL=$njobs",
                )
            }
        }
        testInstrumentationRunner = "android.support.test.runner.AndroidJUnitRunner"
    }

    splits {
        abi {
            isEnable = true
            reset()
            if (abiArm32) {
                include("armeabi-v7a")
            }
            if (abiArm64) {
                include("arm64-v8a")
            }
            if (abiX8632) {
                include("x86")
            }
            if (abiX8664) {
                include("x86_64")
            }
            isUniversalApk = false
        }
    }

    flavorDimensions("version")

    productFlavors {
        create("stable") {
            dimension = "version"
            resValue("string", "app_name", "Cataclysm BN")
        }
        create("experimental") {
            dimension = "version"
            applicationIdSuffix = ".experimental"
            resValue("string", "app_name", "Cataclysm BN (experimental)")
        }
    }

    signingConfigs {
        keystoreProperties.getProperty("storeFile")?.let { storeFileName ->
            if (file(storeFileName).exists()) {
                create("release") {
                    storeFile = file(storeFileName)
                    storePassword = keystoreProperties.getProperty("storePassword")
                    keyAlias = keystoreProperties.getProperty("keyAlias")
                    keyPassword = keystoreProperties.getProperty("keyPassword")
                }
            } else {
                throw GradleException("Keystore file $storeFileName was not found.\n")
            }
        }
        keystoreProperties.getProperty("debug_storeFile")?.let { debugStoreFileName ->
            if (file(debugStoreFileName).exists()) {
                create("debug") {
                    storeFile = file(debugStoreFileName)
                    storePassword = keystoreProperties.getProperty("debug_storePassword")
                    keyAlias = keystoreProperties.getProperty("debug_seyAlias")
                    keyPassword = keystoreProperties.getProperty("debug_keyPassword")
                }
            } else {
                throw GradleException("Keystore file $debugStoreFileName was not found.\n")
            }
        }
    }

    buildTypes {
        getByName("release") {
            isMinifyEnabled = false
            proguardFiles(getDefaultProguardFile("proguard-android.txt"), "proguard-rules.pro")
            if (signingConfigs.names.contains("release")) {
                signingConfig = signingConfigs.getByName("release")
            }
            externalNativeBuild {
                cmake {
                    cFlags("-DNDEBUG", "-DRELEASE", "-Os")
                    cppFlags("-DNDEBUG", "-DRELEASE", "-Os")
                }
            }
        }

        getByName("debug") {
            isMinifyEnabled = false
            proguardFiles(getDefaultProguardFile("proguard-android.txt"), "proguard-rules.pro")
            if (signingConfigs.names.contains("debug")) {
                signingConfig = signingConfigs.getByName("debug")
            }
            externalNativeBuild {
                cmake {
                    cFlags("-DRELEASE", "-O2", "-g0")
                    cppFlags("-DRELEASE", "-O2", "-g0")
                }
            }
        }
    }

    if (!project.hasProperty("EXCLUDE_NATIVE_LIBS")) {
        sourceSets.getByName("main") {
            jniLibs.srcDir("libs")
        }
        externalNativeBuild {
            cmake {
                path = file("jni/CMakeLists.txt")
            }
        }
    }

    lintOptions {
        isAbortOnError = false
    }

    sourceSets {
        getByName("main") {
            java.srcDirs("src/main/java")
            resources.srcDirs("src/main/res")
        }
        getByName("stable") {}
        getByName("experimental") {
            resources.srcDirs("src/experimental/res")
        }
    }
}

extensions.configure<BaseExtension>("android") {
    configureCommonAndroid()
}

dependencies {
    add("api", fileTree(mapOf("include" to listOf("*.jar"), "dir" to "libs")))
}
