#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod solver_bridge;
mod commands;

fn main() {
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri_plugin_fs::init())
        .invoke_handler(tauri::generate_handler![
            commands::generate_mesh,
            commands::run_fea_solve,
            commands::save_project,
            commands::load_project,
            commands::get_solver_log,
            commands::clear_solver_log,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
