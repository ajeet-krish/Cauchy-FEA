use std::ffi::c_int;
use std::os::raw::c_char;

extern "C" {
    fn fea_generate_mesh_c(
        shapes_json: *const c_char,
        nx: c_int,
        ny: c_int,
        elem_type: c_int,
        output_dir: *const c_char,
    ) -> c_int;

    fn fea_solve_c(
        mesh_json: *const c_char,
        config_json: *const c_char,
        output_dir: *const c_char,
    ) -> c_int;
}

pub fn generate_mesh(
    shapes_json: &str,
    nx: i32,
    ny: i32,
    elem_type: i32,
    output_dir: &str,
) -> Result<i32, String> {
    let c_shapes = std::ffi::CString::new(shapes_json).map_err(|e| e.to_string())?;
    let c_output = std::ffi::CString::new(output_dir).map_err(|e| e.to_string())?;
    unsafe {
        let result = fea_generate_mesh_c(
            c_shapes.as_ptr(),
            nx,
            ny,
            elem_type,
            c_output.as_ptr(),
        );
        Ok(result)
    }
}

pub fn run_solve(
    mesh_json: &str,
    config_json: &str,
    output_dir: &str,
) -> Result<i32, String> {
    let c_mesh = std::ffi::CString::new(mesh_json).map_err(|e| e.to_string())?;
    let c_config = std::ffi::CString::new(config_json).map_err(|e| e.to_string())?;
    let c_output = std::ffi::CString::new(output_dir).map_err(|e| e.to_string())?;
    unsafe {
        let result = fea_solve_c(
            c_mesh.as_ptr(),
            c_config.as_ptr(),
            c_output.as_ptr(),
        );
        Ok(result)
    }
}
