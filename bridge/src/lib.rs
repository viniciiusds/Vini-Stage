use nord_format::formats::npno::{codec, Piano};
use std::{ffi::{CStr, c_char, c_void}, fs::File, io::{BufReader, Read, Seek, SeekFrom}, panic::{catch_unwind, AssertUnwindSafe}};

type ProgressCallback=Option<unsafe extern "C" fn(*mut c_void,u32)>;
struct Progress { callback:ProgressCallback, context:*mut c_void, last:Option<u32> }
impl Progress {
    fn report(&mut self,value:u32){
        let value=value.min(100);
        if self.last.map_or(true,|last|value>last){
            self.last=Some(value);
            if let Some(callback)=self.callback {unsafe{callback(self.context,value);}}
        }
    }
}
struct ProgressReader<'a>{inner:BufReader<File>,position:u64,furthest:u64,total:u64,progress:&'a mut Progress}
impl Read for ProgressReader<'_>{
    fn read(&mut self,buffer:&mut [u8])->std::io::Result<usize>{
        let count=self.inner.read(buffer)?;
        self.position+=count as u64;self.furthest=self.furthest.max(self.position);
        self.progress.report((self.furthest.min(self.total)*20/self.total.max(1)) as u32);
        Ok(count)
    }
}
impl Seek for ProgressReader<'_>{
    fn seek(&mut self,from:SeekFrom)->std::io::Result<u64>{self.position=self.inner.seek(from)?;Ok(self.position)}
}

struct Recording { root: u8, bank: u8, layer: u8, channels: u16, frames: usize, pcm: Vec<f32>, loop_start:usize, loop_end:usize, loop_decay:f32 }
// Vendor records locate a repeat using a start frame plus its length. The next
// marks bracket the decoder block containing the start. Reject inconsistent data.
fn loop_info(record:&[u8;118],bank:u8,frames:usize)->(usize,usize,f32){
    let word=|at|u32::from_be_bytes(record[at..at+4].try_into().unwrap()) as usize;
    let start=word(0x1c);let length=word(0x20);let block_start=word(0x24);let block_end=word(0x28);
    let end=start.checked_add(length).unwrap_or(usize::MAX);
    if bank>1||start==0||length<64||end>frames||block_start>start||start>=block_end||block_end>frames{return (0,0,1.0);}
    let decay=word(0x2e) as f64 / 8_388_608.0;
    (start,end,if decay>0.0&&decay<=1.0{decay as f32}else{1.0})
}
pub struct Library { name: std::ffi::CString, keys: [u8; 128], fine: [i8; 128], recordings: Vec<Recording>, gain: f32, damper: u8 }
fn load(path: &str,progress:&mut Progress) -> Result<Library, Box<dyn std::error::Error>> {
    let file=File::open(path)?;let total=file.metadata()?.len();
    let piano={
        let mut reader=ProgressReader{inner:BufReader::new(file),position:0,furthest:0,total,progress};
        Piano::read_from(&mut reader)?
    };
    progress.report(20);
    let lib = piano.library()?;
    progress.report(25);
    let mut out = Library { name: std::ffi::CString::new(piano.name()?.0)?, keys: [255;128], fine: [0;128], recordings: Vec::new(), gain: 10f32.powf(lib.gain() as f32 / 200.0), damper: lib.damper_top() };
    out.keys.copy_from_slice(lib.key_map());
    for k in 0..128 { out.fine[k] = lib.fine_tune(k as u8)?; }
    let total_audio=lib.strokes().iter().map(|stroke|stroke.audio().len() as u64).sum::<u64>().max(1);
    let mut decoded=0u64;
    for stroke in lib.strokes() {
        let audio = codec::decode(stroke, lib.channels())?;
        if audio.clipped != 0 { return Err(format!("Audio saturado na nota {}", stroke.root).into()); }
        // Preserve the decoded recording level. The per-stroke trim interpretation
        // is not validated for this Clavinet (values reach 147); treating it as
        // whole dB makes the upper keyboard inaudible.
        let gain = 1.0;
        let (loop_start,loop_end,loop_decay)=loop_info(stroke.record(),stroke.bank_code(),audio.frames());
        out.recordings.push(Recording { root: stroke.root, bank: stroke.bank_code(), layer: stroke.layer(), channels: lib.channels(), frames: audio.frames(), loop_start, loop_end, loop_decay, pcm: audio.interleaved().into_iter().map(|s| s as f32 / 32768.0 * gain).collect() });
        decoded+=stroke.audio().len() as u64;
        progress.report(25+(decoded*74/total_audio) as u32);
    }
    if !out.recordings.iter().any(|r| r.bank == 0 && r.frames > 0) { return Err("Arquivo sem gravacoes de ataque".into()); }
    Ok(out)
}

#[no_mangle]
pub unsafe extern "C" fn np_load(path: *const c_char, error: *mut c_char, cap: usize) -> *mut Library {
    np_load_with_progress(path,error,cap,None,std::ptr::null_mut())
}
#[no_mangle]
pub unsafe extern "C" fn np_load_with_progress(path:*const c_char,error:*mut c_char,cap:usize,callback:ProgressCallback,context:*mut c_void)->*mut Library {
    let mut progress=Progress{callback,context,last:None};
    progress.report(0);
    let result = catch_unwind(AssertUnwindSafe(|| {
        if path.is_null() { return Err("Caminho vazio".to_string()); }
        let path = CStr::from_ptr(path).to_str().map_err(|e| e.to_string())?;
        load(path,&mut progress).map_err(|e| e.to_string())
    }));
    match result {
        Ok(Ok(lib)) => {let pointer=Box::into_raw(Box::new(lib));progress.report(100);pointer},
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
#[repr(C)] pub struct View { pub data: *const f32, pub frames: usize, pub channels: u16, pub root: u8, pub bank: u8, pub layer: u8, pub loop_start:usize, pub loop_end:usize, pub loop_decay:f32 }
#[no_mangle] pub unsafe extern "C" fn np_recording(p: *const Library, index: usize, out: *mut View) -> bool {
    if p.is_null() || out.is_null() { return false; }
    if let Some(r) = (&(*p).recordings).get(index) { *out = View { data: r.pcm.as_ptr(), frames: r.frames, channels: r.channels, root: r.root, bank: r.bank, layer: r.layer, loop_start:r.loop_start, loop_end:r.loop_end, loop_decay:r.loop_decay }; true } else { false }
}

#[cfg(test)]
mod tests {
    use super::loop_info;
    fn record()->[u8;118]{
        let mut result=[0;118];
        for (offset,value) in [(0x1c,1000u32),(0x20,2000),(0x24,900),(0x28,1100),(0x2e,8_388_423)]{
            result[offset..offset+4].copy_from_slice(&value.to_be_bytes());
        }result
    }
    #[test]fn loop_uses_start_plus_length_and_q23_decay(){
        let (start,end,decay)=loop_info(&record(),1,3500);
        assert_eq!((start,end),(1000,3000));assert!(decay>0.999&&decay<1.0);
    }
    #[test]fn release_and_malformed_marks_do_not_loop(){
        assert_eq!(loop_info(&record(),2,3500),(0,0,1.0));
        assert_eq!(loop_info(&record(),0,2000),(0,0,1.0));
        assert_eq!(loop_info(&[0;118],0,3500),(0,0,1.0));
        let mut bad=record();bad[0x24..0x28].copy_from_slice(&1001u32.to_be_bytes());
        assert_eq!(loop_info(&bad,0,3500),(0,0,1.0));
    }
}
