use std::fs;
use std::path::PathBuf;
use tauri::{AppHandle, Manager};

/// 应用数据目录下的主数据文件。首次运行时自动创建目录。
fn data_file(app: &AppHandle) -> Result<PathBuf, String> {
    let dir = app
        .path()
        .app_data_dir()
        .map_err(|e| format!("无法定位应用数据目录: {}", e))?;
    fs::create_dir_all(&dir).map_err(|e| format!("无法创建数据目录: {}", e))?;
    Ok(dir.join("grades.json"))
}

/// 备份目录：数据目录下 backups/，保留最近 10 份
fn backup_dir(app: &AppHandle) -> Result<PathBuf, String> {
    let dir = data_file(app)?
        .parent()
        .ok_or_else(|| "无效的数据目录".to_string())?
        .join("backups");
    fs::create_dir_all(&dir).map_err(|e| format!("无法创建备份目录: {}", e))?;
    Ok(dir)
}

fn local_stamp() -> String {
    // 使用系统本地时间生成文件名，避免引入 chrono 依赖
    use std::time::{SystemTime, UNIX_EPOCH};
    let secs = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_secs())
        .unwrap_or(0);
    format!("{}", secs)
}

/// 返回主数据文件的绝对路径，用于界面展示"数据保存在哪里"
#[tauri::command]
fn data_path(app: AppHandle) -> Result<String, String> {
    data_file(&app).map(|p| p.to_string_lossy().to_string())
}

/// 读取主数据文件。文件不存在时返回 "null"，由前端初始化默认数据。
#[tauri::command]
fn load_data(app: AppHandle) -> Result<String, String> {
    let path = data_file(&app)?;
    if !path.exists() {
        return Ok("null".to_string());
    }
    fs::read_to_string(&path).map_err(|e| format!("读取数据失败: {}", e))
}

/// 写入主数据文件。采用"临时文件 + 原子重命名"，防止写入中断导致数据损坏。
#[tauri::command]
fn save_data(app: AppHandle, content: String) -> Result<(), String> {
    let path = data_file(&app)?;
    let tmp = path.with_extension("json.tmp");
    fs::write(&tmp, content.as_bytes()).map_err(|e| format!("写入临时文件失败: {}", e))?;
    fs::rename(&tmp, &path).map_err(|e| format!("保存数据失败: {}", e))
}

/// 在创建备份后返回备份文件路径
#[tauri::command]
fn create_backup(app: AppHandle) -> Result<String, String> {
    let src = data_file(&app)?;
    if !src.exists() {
        return Ok(String::new());
    }
    let dir = backup_dir(&app)?;
    let dest = dir.join(format!("grades-{}.json", local_stamp()));
    fs::copy(&src, &dest).map_err(|e| format!("创建备份失败: {}", e))?;
    prune_backups(&dir)?;
    Ok(dest.to_string_lossy().to_string())
}

/// 清理超出上限的历史备份，只保留最新 10 份
fn prune_backups(dir: &PathBuf) -> Result<(), String> {
    let mut entries: Vec<(PathBuf, u64)> = Vec::new();
    for entry in fs::read_dir(dir).map_err(|e| format!("读取备份目录失败: {}", e))? {
        let entry = entry.map_err(|e| e.to_string())?;
        let meta = entry.metadata().map_err(|e| e.to_string())?;
        if meta.is_file() {
            let stamp = meta
                .modified()
                .ok()
                .and_then(|m| m.duration_since(std::time::UNIX_EPOCH).ok())
                .map(|d| d.as_secs())
                .unwrap_or(0);
            entries.push((entry.path(), stamp));
        }
    }
    entries.sort_by(|a, b| b.1.cmp(&a.1));
    for (path, _) in entries.iter().skip(10) {
        let _ = fs::remove_file(path);
    }
    Ok(())
}

/// 读取任意路径的文本文件（导入功能，路径由前端文件对话框提供）
#[tauri::command]
fn read_file(path: String) -> Result<String, String> {
    fs::read_to_string(&path).map_err(|e| format!("读取文件失败: {}", e))
}

/// 写入任意路径的文本文件（导出功能，路径由前端文件对话框提供）
#[tauri::command]
fn write_file(path: String, content: String) -> Result<(), String> {
    fs::write(&path, content.as_bytes()).map_err(|e| format!("导出文件失败: {}", e))
}

/// 上报前端启动结果。
///
/// 用途：WebView 里的 JS 报错不会出现在终端，因此如果没有这一步，"打包后的应用能否
/// 正常启动"就只能靠人工观察窗口来判断。前端在 boot() 成功或失败时各调用一次，
/// 结果落在数据目录的 boot-status.json，既可被自动化脚本断言，也方便用户排查问题。
#[tauri::command]
fn report_boot(app: AppHandle, ok: bool, detail: String, at: String) -> Result<(), String> {
    let path = data_file(&app)?.with_file_name("boot-status.json");
    let payload = serde_json::json!({
        "ok": ok,
        "at": at,
        "detail": detail,
        "version": env!("CARGO_PKG_VERSION"),
    });
    fs::write(&path, payload.to_string()).map_err(|e| format!("写入启动状态失败: {}", e))
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .invoke_handler(tauri::generate_handler![
            data_path,
            load_data,
            save_data,
            create_backup,
            read_file,
            write_file,
            report_boot
        ])
        .run(tauri::generate_context!())
        .expect("启动班级成绩管理系统失败");
}
