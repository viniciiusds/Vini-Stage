use nord_format::formats::npno::{codec, Piano};
use std::{ffi::{CStr, c_char}, fs::File, io::BufReader, panic::{catch_unwind, AssertUnwindSafe}};

struct Recording { root: u8, bank: u8, layer: u8, channels: u16, frames: usize, pcm: Vec<f32> }
pub struct Library { name: std::ffi::CString, keys: [u8; 128], fine: [i8; 128], recordings: Vec<Recording>, gain: f32, damper: u8 }
fn load(path: &str) -> Result<Library, Box<dyn std::error::Error>> {
    let piano = Piano::read_from(&mut BufReader::new(File::open(path)?))?;
    let lib = piano.library()?;
    let mut out = Library { name: std::ffi::CString::new(piano.name()?.0)?, keys: [255;128], fine: [0;128], recordings: Vec::new(), gain: 10f32.powf(lib.gain() as f32 / 200.0), damper: lib.damper_top() };
    out.keys.copy_from_slice(lib.key_map());
    for k in 0..128 { out.fine[k] = lib.fine_tune(k as u8)?; }
    for stroke in lib.strokes() {
        let audio = codec::decode(stroke, lib.channels())?;
        if audio.clipped != 0 { return Err(format!("Audio saturado na nota {}", stroke.root).into()); }
        // Preserve the decoded recording level. The per-stroke trim interpretation
        // is not validated for this Clavinet (values reach 147); treating it as
        // whole dB makes the upper keyboard inaudible.
        let gain = 1.0;
        out.recordings.push(Recording { root: stroke.root, bank: stroke.bank_code(), layer: stroke.layer(), channels: lib.channels(), frames: audio.frames(), pcm: audio.interleaved().into_iter().map(|s| s as f32 / 32768.0 * gain).collect() });
    }
    if !out.recordings.iter().any(|r| r.bank == 0 && r.frames > 0) { return Err("Arquivo sem gravacoes de ataque".into()); }
    Ok(out)
}

#[no_mangle]
pub unsafe extern "C" fn np_load(path: *const c_char, error: *mut c_char, cap: usize) -> *mut Library {
    let result = catch_unwind(AssertUnwindSafe(|| {
        if path.is_null() { return Err("Caminho vazio".to_string()); }
        let path = CStr::from_ptr(path).to_str().map_err(|e| e.to_string())?;
        load(path).map_err(|e| e.to_string())
    }));
    match result {
        Ok(Ok(lib)) => Box::into_raw(Box::new(lib)),
        failure => {
            let message = match failure { Ok(Err(e)) => e, _ => "Falha ao interpretar o arquivo".into() };
            if !error.is_null() && cap > 0 { let n = message.len().min(cap-1); std::ptr::copy_nonoverlapping(message.as_ptr(), error as *mut u8, n); *error.add(n) = 0; }
            std::ptr::null_mut()
        }
    }
}
#[no_mangle] pub unsafe extern "C" fn np_free(p: *mut Library) { if !p.is_null() { drop(Box::from_raw(p)); } }
#[no_mangle] pub unsafe extern "C" fn np_name(p: *const Library) -> *const c_char { (*p).name.as_ptr() }
#[no_mangle] pub unsafe extern "C" fn np_count(p: *const Library) -> usize { (*p).recordings.len() }
#[no_mangle] pub unsafe extern "C" fn np_root(p: *const Library, key: u8) -> u8 { (*p).keys[key as usize] }
#[no_mangle] pub unsafe extern "C" fn np_fine(p: *const Library, key: u8) -> i8 { (*p).fine[key as usize] }
#[no_mangle] pub unsafe extern "C" fn np_gain(p: *const Library) -> f32 { (*p).gain }
#[no_mangle] pub unsafe extern "C" fn np_damper(p: *const Library) -> u8 { (*p).damper }
#[no_mangle] pub extern "C" fn np_rate() -> u32 { codec::RATE }
#[repr(C)] pub struct View { pub data: *const f32, pub frames: usize, pub channels: u16, pub root: u8, pub bank: u8, pub layer: u8 }
#[no_mangle] pub unsafe extern "C" fn np_recording(p: *const Library, index: usize, out: *mut View) -> bool {
    if p.is_null() || out.is_null() { return false; }
    if let Some(r) = (&(*p).recordings).get(index) { *out = View { data: r.pcm.as_ptr(), frames: r.frames, channels: r.channels, root: r.root, bank: r.bank, layer: r.layer }; true } else { false }
}
