package com.cleverraven.cataclysmdda

import android.content.Context
import android.content.Intent
import android.content.res.AssetManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.provider.Settings
import androidx.preference.PreferenceManager
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Checkbox
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.lifecycleScope
import java.io.File
import java.io.FileOutputStream
import java.io.InputStream
import java.io.OutputStream
import java.util.Timer
import java.util.TimerTask
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

private const val TAG = "Splash"
private const val LEGACY_STORAGE_SETTING = "Use Legacy Storage"

private val settingsNames = listOf(
    "Software rendering",
    "Force fullscreen",
    "Trap Back button",
    LEGACY_STORAGE_SETTING,
)
private val defaultSettingsValues = listOf(false, false, true, false)
private val preserveSubfolders = setOf("sound", "mods", "gfx")
private val preserveFolders = setOf("font")
private val preserveFiles = setOf("default_mods.json")

data class InstallUiState(
    val cleanInstall: Boolean = true,
    val totalFiles: Int = 0,
    val installedFiles: Int = 0,
    val installing: Boolean = true,
    val error: String? = null,
    val showSettings: Boolean = false,
    val showHelp: Boolean = false,
    val settingsValues: List<Boolean> = defaultSettingsValues,
)

class SplashScreen : ComponentActivity() {
    private var uiState by mutableStateOf(InstallUiState())

    override fun onCreate(savedInstanceState: Bundle?) {
        Log.e(TAG, "onCreate()")
        super.onCreate(savedInstanceState)

        val preferences = PreferenceManager.getDefaultSharedPreferences(applicationContext)
        if (versionName() == preferences.getString("installed", "")) {
            startGameActivity(delay = false)
            return
        }

        uiState = uiState.copy(cleanInstall = preferences.getString("installed", "").isNullOrEmpty())
        setContent {
            CataclysmInstallScreen(
                state = uiState,
                onSettingChanged = ::setSettingValue,
                onStartGame = ::saveSettingsAndStartGame,
                onShowHelp = { uiState = uiState.copy(showHelp = true) },
                onDismissHelp = { uiState = uiState.copy(showHelp = false) },
                onExit = ::finish,
            )
        }
        installProgram()
    }

    private fun versionName(): String = try {
        val packageInfo = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            packageManager.getPackageInfo(packageName, android.content.pm.PackageManager.PackageInfoFlags.of(0))
        } else {
            @Suppress("DEPRECATION")
            packageManager.getPackageInfo(packageName, 0)
        }
        packageInfo.versionName ?: "error"
    } catch (error: Exception) {
        error.printStackTrace()
        "error"
    }

    private fun installProgram() {
        lifecycleScope.launch {
            val result = runCatching {
                withContext(Dispatchers.IO) {
                    val assetManager = assets
                    val totalFiles = countTotalAssets(assetManager, "data") +
                        countTotalAssets(assetManager, "gfx") +
                        countTotalAssets(assetManager, "lang")
                    withContext(Dispatchers.Main) {
                        uiState = uiState.copy(totalFiles = totalFiles, installing = true)
                    }

                    val externalFilesDirPath = checkNotNull(getExternalFilesDir(null)).path
                    deleteRecursive(assetManager, externalFilesDirPath, File("$externalFilesDirPath/data"))
                    deleteRecursive(assetManager, externalFilesDirPath, File("$externalFilesDirPath/gfx"))
                    deleteRecursive(assetManager, externalFilesDirPath, File("$externalFilesDirPath/lang"))
                    copyAssetFolder(assetManager, "data", "$externalFilesDirPath/data")
                    copyAssetFolder(assetManager, "gfx", "$externalFilesDirPath/gfx")
                    copyAssetFolder(assetManager, "lang", "$externalFilesDirPath/lang")
                }
            }

            result.onSuccess {
                PreferenceManager.getDefaultSharedPreferences(applicationContext)
                    .edit()
                    .putString("installed", versionName())
                    .apply()
                uiState = uiState.copy(
                    installing = false,
                    installedFiles = uiState.totalFiles,
                    showSettings = true,
                )
                Log.d(TAG, "Total number of files copied: ${uiState.installedFiles}")
            }.onFailure { error ->
                uiState = uiState.copy(installing = false, error = error.message ?: error.toString())
            }
        }
    }

    private suspend fun copyAsset(assetManager: AssetManager, fromAssetPath: String, toPath: String): Boolean {
        withContext(Dispatchers.Main) {
            uiState = uiState.copy(installedFiles = uiState.installedFiles + 1)
        }
        return try {
            assetManager.open(fromAssetPath).use { input ->
                File(toPath).also { it.parentFile?.mkdirs() }.createNewFile()
                FileOutputStream(toPath).use { output -> copyFile(input, output) }
            }
            true
        } catch (error: Exception) {
            error.printStackTrace()
            throw error
        }
    }

    private suspend fun copyAssetFolder(assetManager: AssetManager, fromAssetPath: String, toPath: String): Boolean = try {
        val files = assetManager.list(fromAssetPath).orEmpty()
        File(toPath).mkdirs()
        files.fold(true) { copied, file ->
            val assetPath = "$fromAssetPath/$file"
            val targetPath = "$toPath/$file"
            val subdirFiles = assetManager.list(assetPath).orEmpty()
            copied && if (subdirFiles.isEmpty()) {
                copyAsset(assetManager, assetPath, targetPath)
            } else {
                copyAssetFolder(assetManager, assetPath, targetPath)
            }
        }
    } catch (error: Exception) {
        error.printStackTrace()
        throw error
    }

    private fun deleteRecursive(assetManager: AssetManager, externalFilesDir: String, fileOrDirectory: File) {
        if (!fileOrDirectory.exists()) {
            return
        }
        val parentFolder = fileOrDirectory.parentFile?.name?.lowercase().orEmpty()
        val fileOrDirectoryName = fileOrDirectory.name.lowercase()
        if (fileOrDirectory.isDirectory) {
            if (fileOrDirectoryName in preserveFolders) {
                return
            }
            if (parentFolder in preserveSubfolders && !assetExists(assetManager, fileOrDirectory.path.substring(externalFilesDir.length + 1))) {
                return
            }
            fileOrDirectory.listFiles().orEmpty().forEach { deleteRecursive(assetManager, externalFilesDir, it) }
        } else if (fileOrDirectoryName in preserveFiles) {
            return
        }
        fileOrDirectory.delete()
    }

    private fun assetExists(assetManager: AssetManager, assetPath: String, assetName: String = ""): Boolean = try {
        val files = assetManager.list(assetPath).orEmpty()
        if (assetName.isEmpty()) {
            files.isNotEmpty()
        } else {
            files.any { it.equals(assetName, ignoreCase = true) }
        }
    } catch (error: Exception) {
        error.printStackTrace()
        false
    }

    private fun countTotalAssets(assetManager: AssetManager, assetPath: String): Int = try {
        assetManager.list(assetPath).orEmpty().sumOf { file ->
            val filePath = "$assetPath/$file"
            val subdirFiles = assetManager.list(filePath).orEmpty()
            if (subdirFiles.isEmpty()) 1 else countTotalAssets(assetManager, filePath)
        }
    } catch (error: Exception) {
        error.printStackTrace()
        throw error
    }

    private fun copyFile(input: InputStream, output: OutputStream) {
        val buffer = ByteArray(1024)
        while (true) {
            val read = input.read(buffer)
            if (read == -1) {
                return
            }
            output.write(buffer, 0, read)
        }
    }

    private fun setSettingValue(index: Int, value: Boolean) {
        uiState = uiState.copy(settingsValues = uiState.settingsValues.mapIndexed { currentIndex, currentValue ->
            if (currentIndex == index) value else currentValue
        })
    }

    private fun saveSettingsAndStartGame() {
        val editor = PreferenceManager.getDefaultSharedPreferences(applicationContext).edit()
        settingsNames.zip(uiState.settingsValues).forEach { (name, value) -> editor.putBoolean(name, value) }
        editor.apply()
        startGameActivity(delay = false)
    }

    private fun startGameActivity(delay: Boolean) {
        if (!delay) {
            runOnUiThread { launchGameOrRequestStorage() }
            return
        }
        Timer().schedule(object : TimerTask() {
            override fun run() {
                runOnUiThread { launchGameOrRequestStorage() }
            }
        }, 1500)
    }

    private fun launchGameOrRequestStorage() {
        val useLegacyStorage = PreferenceManager.getDefaultSharedPreferences(applicationContext)
            .getBoolean(LEGACY_STORAGE_SETTING, false)
        val needsStoragePermission = !useLegacyStorage && Build.VERSION.SDK_INT >= 30 && !Environment.isExternalStorageManager()
        if (needsStoragePermission) {
            startActivity(
                Intent(
                    Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                    Uri.parse("package:$packageName"),
                ),
            )
            finish()
            return
        }
        startActivity(Intent(this, CataclysmDDA::class.java).addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION))
        finish()
        overridePendingTransition(0, 0)
    }
}

@Composable
private fun CataclysmInstallScreen(
    state: InstallUiState,
    onSettingChanged: (Int, Boolean) -> Unit,
    onStartGame: () -> Unit,
    onShowHelp: () -> Unit,
    onDismissHelp: () -> Unit,
    onExit: () -> Unit,
) {
    MaterialTheme(colorScheme = darkColorScheme()) {
        Surface(modifier = Modifier.fillMaxSize()) {
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(32.dp),
                verticalArrangement = Arrangement.Center,
            ) {
                when {
                    state.error != null -> InstallError(error = state.error, onExit = onExit)
                    state.showSettings -> SettingsContent(
                        state = state,
                        onSettingChanged = onSettingChanged,
                        onStartGame = onStartGame,
                        onShowHelp = onShowHelp,
                    )
                    else -> InstallProgress(state = state)
                }
            }
            if (state.showHelp) {
                AlertDialog(
                    onDismissRequest = onDismissHelp,
                    title = { Text(text = stringResource(R.string.helpTitle)) },
                    text = { Text(text = stringResource(R.string.helpMessage)) },
                    confirmButton = {
                        TextButton(onClick = onDismissHelp) {
                            Text(text = "OK")
                        }
                    },
                )
            }
        }
    }
}

@Composable
private fun InstallProgress(state: InstallUiState) {
    Text(
        text = stringResource(if (state.cleanInstall) R.string.installTitle else R.string.upgradeTitle),
        style = MaterialTheme.typography.headlineSmall,
    )
    Spacer(modifier = Modifier.height(16.dp))
    if (state.totalFiles == 0) {
        LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
    } else {
        LinearProgressIndicator(
            progress = { state.installedFiles.toFloat() / state.totalFiles.toFloat() },
            modifier = Modifier.fillMaxWidth(),
        )
        Spacer(modifier = Modifier.height(8.dp))
        Text(text = "${state.installedFiles} / ${state.totalFiles}")
    }
}

@Composable
private fun SettingsContent(
    state: InstallUiState,
    onSettingChanged: (Int, Boolean) -> Unit,
    onStartGame: () -> Unit,
    onShowHelp: () -> Unit,
) {
    Text(text = "Settings", style = MaterialTheme.typography.headlineSmall)
    Spacer(modifier = Modifier.height(16.dp))
    settingsNames.forEachIndexed { index, name ->
        Row(verticalAlignment = Alignment.CenterVertically) {
            Checkbox(
                checked = state.settingsValues[index],
                onCheckedChange = { onSettingChanged(index, it) },
            )
            Spacer(modifier = Modifier.width(8.dp))
            Text(text = name)
        }
    }
    Spacer(modifier = Modifier.height(24.dp))
    Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
        Button(onClick = onStartGame) {
            Text(text = "Start game")
        }
        TextButton(onClick = onShowHelp) {
            Text(text = "Show help")
        }
    }
}

@Composable
private fun InstallError(error: String, onExit: () -> Unit) {
    Text(text = "Installation Failed", style = MaterialTheme.typography.headlineSmall)
    Spacer(modifier = Modifier.height(16.dp))
    Text(text = error)
    Spacer(modifier = Modifier.height(24.dp))
    Button(onClick = onExit) {
        Text(text = "OK")
    }
}
