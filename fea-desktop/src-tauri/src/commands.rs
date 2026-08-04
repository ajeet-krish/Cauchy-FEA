use std::sync::Mutex;
use tauri::command;
use tauri::Manager;
use crate::solver_bridge;

// Global solver log storage
static SOLVER_LOG: Mutex<Vec<String>> = Mutex::new(Vec::new());

pub fn log_message(msg: &str) {
    if let Ok(mut log) = SOLVER_LOG.lock() {
        log.push(msg.to_string());
    }
}

fn validate_path(path: &str) -> Result<(), String> {
    let p = std::path::Path::new(path);
    if p.components().any(|c| matches!(c, std::path::Component::ParentDir)) {
        return Err("Invalid path: contains ..".to_string());
    }
    Ok(())
}

#[command]
pub fn generate_mesh(
    shapes_json: String,
    nx: i32,
    ny: i32,
    elem_type: i32,
    app: tauri::AppHandle,
) -> Result<String, String> {
    let output_dir = app
        .path()
        .app_data_dir()
        .unwrap()
        .join("meshes")
        .to_string_lossy()
        .to_string();

    // Create directory if it doesn't exist
    std::fs::create_dir_all(&output_dir).map_err(|e| e.to_string())?;

    log_message(&format!(
        "[mesh] Generating mesh: {}x{}, elem_type={}",
        nx, ny, elem_type
    ));
    log_message(&format!("[mesh] Output directory: {}", output_dir));

    solver_bridge::generate_mesh(&shapes_json, nx, ny, elem_type, &output_dir)?;

    // Read the generated mesh.json and return it
    let mesh_path = format!("{}/mesh.json", output_dir);
    let mesh_data = std::fs::read_to_string(&mesh_path)
        .map_err(|e| format!("Failed to read generated mesh: {}", e))?;

    log_message("[mesh] Mesh generation complete.");
    Ok(mesh_data)
}

#[command]
pub fn run_fea_solve(
    mesh_json: String,
    config_json: String,
    app: tauri::AppHandle,
) -> Result<String, String> {
    let output_dir = app
        .path()
        .app_data_dir()
        .unwrap()
        .join("results")
        .to_string_lossy()
        .to_string();

    std::fs::create_dir_all(&output_dir).map_err(|e| e.to_string())?;

    log_message("[solver] Running FEA solve...");

    solver_bridge::run_solve(&mesh_json, &config_json, &output_dir)?;

    // Read results and return as JSON
    let disp_path = format!("{}/displacement.json", output_dir);
    let stress_path = format!("{}/stress.json", output_dir);
    let meta_path = format!("{}/meta.json", output_dir);

    let disp = std::fs::read_to_string(&disp_path).unwrap_or_default();
    let stress = std::fs::read_to_string(&stress_path).unwrap_or_default();
    let meta = std::fs::read_to_string(&meta_path).unwrap_or_default();

    // Combine into single response
    let result = format!(
        "{{\"displacement\":{},\"stress\":{},\"meta\":{}}}",
        disp, stress, meta
    );

    log_message("[solver] FEA solve complete.");
    Ok(result)
}

#[command]
pub fn save_project(path: String, project_json: String) -> Result<(), String> {
    validate_path(&path)?;
    std::fs::write(&path, &project_json)
        .map_err(|e| format!("Failed to save project: {}", e))
}

#[command]
pub fn load_project(path: String) -> Result<String, String> {
    validate_path(&path)?;
    std::fs::read_to_string(&path)
        .map_err(|e| format!("Failed to load project: {}", e))
}

#[command]
pub fn get_solver_log(last_n: Option<usize>) -> Result<Vec<String>, String> {
    let log = SOLVER_LOG
        .lock()
        .map_err(|e| format!("Failed to lock log: {}", e))?;
    match last_n {
        Some(n) => {
            let start = log.len().saturating_sub(n);
            Ok(log[start..].to_vec())
        }
        None => Ok(log.clone()),
    }
}

#[command]
pub fn clear_solver_log() -> Result<(), String> {
    let mut log = SOLVER_LOG
        .lock()
        .map_err(|e| format!("Failed to lock log: {}", e))?;
    log.clear();
    Ok(())
}
